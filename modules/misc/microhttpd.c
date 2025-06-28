/*****************************************************************************
 * microhttpd.c: microhttpd plugin for VLC
 *****************************************************************************
 * Copyright (C) 2025 VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#if HAVE_POLL_H
#include <poll.h>
#endif

#include <vlc_httpd.h>
#include <vlc_interrupt.h>
#include <vlc_plugin.h>
#include <vlc_poll.h>
#include <vlc_threads.h>
#include <vlc_tls.h>

/* below macros referenced by MHD */
#define MHD_APP_SOCKET_CNTX_TYPE struct vlc_httpd_socket_context
#define MHD_APP_DAEMON_EXTRN_TLS_CNTX_TYPE vlc_object_t
#include <microhttpd2.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <wincrypt.h> /* For entropy generation */
#else
#include <fcntl.h> /* open() function */
#include <sys/types.h>
#include <unistd.h> /* close() function */
#endif

/**
 * Initialise random data
 * @return non-zero if succeed,
 *         zero if failed
 */
static int init_entropy_bytes(char *entropy_bytes, size_t entropy_bytes_size)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
    int fd;
    ssize_t len;
    size_t off;

    fd = open("/dev/urandom", O_RDONLY);
    if (-1 == fd)
    {
        fd = open("/dev/arandom", O_RDONLY);
        if (-1 == fd)
            fd = open("/dev/random", O_RDONLY);
    }
    if (0 > fd)
        return 0;

    for (off = 0; off < entropy_bytes_size; off += (size_t)len)
    {
        len = read(fd, entropy_bytes + off, entropy_bytes_size - off);
        if (0 >= len)
        {
            (void)close(fd);
            return 0;
        }
    }
    (void)close(fd);
    return !0;
#else  /* Native W32 */
    HCRYPTPROV cc;
    BOOL b;

    b = CryptAcquireContext(
        &cc, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if (FALSE == b)
        return 0;

    b = CryptGenRandom(cc, entropy_bytes_size, (BYTE *)entropy_bytes);
    CryptReleaseContext(cc, 0);
    return (FALSE != b);
#endif /* Native W32 */
}

#define HTML_ERROR_RESPONSE(TITLE, HEADING)                                    \
    "<?xml version=\"1.0\" encoding=\"ascii\" ?>\n"                            \
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "              \
    "\"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"                \
    "<html lang=\"en\">\n"                                                     \
    "<head>\n"                                                                 \
    "<title>" TITLE "</title>\n"                                               \
    "</head>\n"                                                                \
    "<body>\n"                                                                 \
    "<h1>" HEADING "</h1>\n"                                                   \
    "<hr />\n"                                                                 \
    "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"                       \
    "</body>\n"                                                                \
    "</html>\n"

#define NOT_FOUND_ERROR_RESPONSE                                               \
    HTML_ERROR_RESPONSE("Not found", "404 Not found")
#define INTERNAL_SERVER_ERROR_RESPONSE                                         \
    HTML_ERROR_RESPONSE("Internal server error", "500 Internal server error")

#define MAX_POST_BODY_SIZE 65536

/* struct to represent a socket */
struct vlc_httpd_socket_context
{
    struct vlc_list node;
    struct MHD_EventUpdateContext *ecb_cntx;
    MHD_Socket fd;
    struct MHD_Daemon *daemon;
    short events;
};

struct vlc_httpd_daemon
{
    struct vlc_httpd_host_sys_t *sys;
    struct MHD_Daemon *daemon;
};

typedef struct vlc_httpd_host_sys_t
{
    vlc_thread_t thread;
    struct vlc_httpd_daemon **vlc_daemons;
    struct vlc_list socket_contexts; /* list of vlc_httpd_socket_context */
    unsigned int total_fds; /* total number of FDs in use (server sockets +
                               client sockets) */
} vlc_httpd_host_sys_t;

struct cb_data_context
{
    vlc_mutex_t lock;
    vlc_httpd_message_t *query;
    vlc_httpd_message_t *answer;
    vlc_httpd_callback_t cb;
    vlc_httpd_callback_sys_t *sys;
    struct MHD_Response *error_response;
    struct MHD_Response *response;
    unsigned int timeout_sec;
};

static vlc_tls_t *mhd_ServerSessionCreateFromFd(vlc_tls_server_t *crd,
                                                int fd,
                                                const char *const *alpn)
{
    vlc_tls_t *sk = vlc_tls_SocketOpen(fd);
    if (unlikely(sk == NULL))
        return NULL;

    vlc_tls_t *tls = vlc_tls_ServerSessionCreate(crd, sk, alpn);
    if (tls == NULL)
    {
        vlc_tls_SessionDelete(sk);
        return NULL;
    }

    return tls;
}

static ssize_t mhd_NetRecv(vlc_tls_t *session, void *buf, size_t len)
{
    struct iovec iov = {.iov_base = buf, .iov_len = len};
    ssize_t ret = session->ops->readv(session, &iov, 1);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EINTR)
            return -2; /* return -2 to indicate that more reading is needed */
        else
            return -1; /* return -1 to indicate an error */
    }

    return ret;
}

static ssize_t mhd_NetSend(vlc_tls_t *session, void *buf, size_t len)
{
    const struct iovec iov = {.iov_base = buf, .iov_len = len};
    ssize_t ret = session->ops->writev(session, &iov, 1);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EINTR)
            return -2; /* return -2 to indicate that more reading is needed */
        else
            return -1; /* return -1 to indicate an error */
    }

    return ret;
}

/* Callback function to concatenate query parameters */
static enum MHD_Bool concatenate_query_params(void *cls,
                                              enum MHD_ValueKind kind,
                                              const struct MHD_NameAndValue *nv)
{
    if (kind != MHD_VK_GET_ARGUMENT)
        return MHD_YES;

    char **result = (char **)cls;
    const char *name = nv->name.cstr;
    const char *value = nv->value.cstr;

    char *new_result = NULL;

    if (*result && (*result)[0] != '\0')
    {
        if (asprintf(&new_result, "%s&%s=%s", *result, name, value) == -1)
            return MHD_NO;
    }
    else
    {
        if (asprintf(&new_result, "%s=%s", name, value) == -1)
            return MHD_NO;
    }

    free(*result);
    *result = new_result;

    return MHD_YES;
}

static uint8_t *get_query_params(struct MHD_Request *request)
{
    uint8_t *query_params = malloc(1);
    if (query_params == NULL)
        return NULL;

    query_params[0] = '\0';

    MHD_request_get_values_cb(
        request, MHD_VK_GET_ARGUMENT, concatenate_query_params, &query_params);

    return query_params;
}

static const vlc_httpd_method_t mhd_to_vlc_method_type_t[] = {
    [MHD_HTTP_METHOD_GET] = VLC_HTTPD_MSG_GET,
    [MHD_HTTP_METHOD_POST] = VLC_HTTPD_MSG_POST,
    [MHD_HTTP_METHOD_HEAD] = VLC_HTTPD_MSG_HEAD
    /* TODO: add RTSP methods */
};

static enum MHD_Bool
header_cb(void *cls, enum MHD_ValueKind kind, const struct MHD_NameAndValue *nv)
{
    if (kind == MHD_VK_HEADER)
    {
        vlc_httpd_message_t *query = cls;
        if (nv->value.cstr != NULL)
        {
            if (vlc_httpd_MsgAddHeader(query, nv->name.cstr, nv->value.cstr))
                return MHD_NO; /* abort iteration */
        }
    }
    return MHD_YES;
}

/* Parse MHD_Request and fill the query message */
static int fill_query(vlc_httpd_message_t *query,
                      struct MHD_Request *request,
                      enum MHD_HTTP_Method method,
                      const char *url_string)
{
    if ((unsigned)method >=
        sizeof(mhd_to_vlc_method_type_t) / sizeof(mhd_to_vlc_method_type_t[0]))
        return -1;

    /* method */
    query->method = mhd_to_vlc_method_type_t[method];

    /* protocol (RTSP and HTTP 2/3 not implemented yet) */
    query->proto = VLC_HTTPD_PROTO_HTTP;
    query->version = 1;

    /* URL */
    query->url = strdup(url_string);
    if (query->url == NULL)
        return -1;

    /* query parameters */
    query->args = get_query_params(request);
    if (query->args == NULL)
        return -1;

    /* request headers */
    MHD_request_get_values_cb(request, MHD_VK_HEADER, header_cb, query);
    return 0;
}

/* parse `answer` and fill response object */
static int parse_answer(vlc_httpd_message_t *answer,
                        struct MHD_Response **response)
{
    /* status code */
    enum MHD_HTTP_StatusCode response_status = answer->status;

    /* parse response body */
    void *buffer = malloc(answer->bodylen);
    if (buffer == NULL)
        return -1;

    memcpy(buffer, answer->body + answer->body_offset, answer->bodylen);

    *response = MHD_response_from_buffer(
        response_status, answer->bodylen, buffer, &free, buffer);

    /* Parse response headers */
    for (size_t i = 0; i < answer->headerslen; i++)
    {
        if (answer->headers[i].value != NULL)
        {
            if (MHD_SC_OK != MHD_response_add_header(*response,
                                                     answer->headers[i].name,
                                                     answer->headers[i].value))
                return -1;
        }
    }

    return 0;
}

static const struct MHD_UploadAction *
post_data_parser(void *upload_cls,
                 struct MHD_Request *request,
                 size_t content_data_size,
                 void *content_data)
{
    struct cb_data_context *pdc = upload_cls;
    const struct MHD_UploadAction *ret;
    vlc_httpd_message_t *query = pdc->query;
    query->body = malloc(content_data_size);
    query->bodylen = content_data_size;
    if (query->body == NULL)
    {
        ret = MHD_upload_action_from_response(request, pdc->error_response);
        goto cleanup;
    }

    memcpy(query->body, content_data, content_data_size);

    if (pdc->cb == NULL)
    {
        ret = MHD_upload_action_from_response(request, pdc->error_response);
        goto cleanup;
    }

    int canc = vlc_savecancel();
    vlc_mutex_lock(&pdc->lock);
    if (pdc->cb(pdc->sys, pdc->answer, pdc->query))
    {
        vlc_mutex_unlock(&pdc->lock);
        vlc_restorecancel(canc);
        ret = MHD_upload_action_from_response(request, pdc->error_response);
        goto cleanup;
    }
    vlc_mutex_unlock(&pdc->lock);
    vlc_restorecancel(canc);

    if (parse_answer(pdc->answer, &pdc->response))
    {
        ret = MHD_upload_action_from_response(request, pdc->error_response);
        goto cleanup;
    }

    ret = MHD_upload_action_from_response(request, pdc->response);

cleanup:
    vlc_httpd_MsgClean(pdc->query);
    vlc_httpd_MsgClean(pdc->answer);
    free(pdc->query);
    free(pdc->answer);
    free(pdc);
    return ret;
}

static const struct MHD_DynamicContentCreatorAction *
stream_cb(void *dyn_cont_cls,
          struct MHD_DynamicContentCreatorContext *ctx,
          uint_fast64_t pos,
          void *buf,
          size_t max)
{
    (void)pos;
    struct cb_data_context *sdc = dyn_cont_cls;
    size_t write = 0;
    vlc_tick_t now = vlc_tick_now();
    vlc_tick_t wait_time = now + VLC_TICK_FROM_SEC(sdc->timeout_sec);
    bool to_continue = false;

    /* TODO: This while loop is a temporary workaround to wait for application
       stream data when it is unavailable, as the MHD_DCC_action_suspend API
       in MHD has not yet been implemented. Once available, we can use that
       to suspend a client connection and resume it upon receiving a signal
       from the main thread indicating that stream data is available. */
    while (write == 0)
    {
        write = max < sdc->answer->bodylen ? max : sdc->answer->bodylen;
        if (write > 0 && sdc->answer->body != NULL)
        {
            memcpy(buf, sdc->answer->body, write);
            to_continue = true;
        }
        int64_t offset = sdc->answer->body_offset;
        int64_t keyframe_wait_to_pass = sdc->answer->keyframe_wait_to_pass;
        vlc_httpd_MsgClean(sdc->answer);
        sdc->answer->body_offset = offset;
        sdc->answer->keyframe_wait_to_pass = keyframe_wait_to_pass;
        sdc->answer->max_chunk_size = max;

        int canc = vlc_savecancel();
        vlc_mutex_lock(&sdc->lock);
        sdc->cb(sdc->sys, sdc->answer, sdc->query);
        vlc_mutex_unlock(&sdc->lock);
        vlc_restorecancel(canc);
        vlc_tick_t current_time = vlc_tick_now();
        if (current_time > wait_time)
            break; /* Exit the loop after timeout seconds */
    }

    if (to_continue)
        return MHD_DCC_action_continue(ctx, write);
    else
        return MHD_DCC_action_finish(ctx);
}

static void stream_cleanup_cb(void *cls)
{
    struct cb_data_context *sdc = cls;
    vlc_httpd_MsgClean(sdc->query);
    vlc_httpd_MsgClean(sdc->answer);
    free(sdc->query);
    free(sdc->answer);
    free(sdc);
}

/* The callback for registration/de-registration of the sockets to watch.
   This is internally called by MHD_daemon_process_reg_events.
   Refer to MHD_SocketRegistrationUpdateCallback in microhttpd2.h for details.
 */
static MHD_APP_SOCKET_CNTX_TYPE *
sock_reg_update_cb(void *cls,
                   MHD_Socket fd,
                   enum MHD_FdState watch_for,
                   MHD_APP_SOCKET_CNTX_TYPE *app_cntx,
                   struct MHD_EventUpdateContext *ecb_cntx)
{
    struct vlc_httpd_daemon *vlc_daemon = cls;
    struct vlc_httpd_host_sys_t *sys = vlc_daemon->sys;
    struct MHD_Daemon *daemon = vlc_daemon->daemon;

    int canc = vlc_savecancel();
    /* socket has been removed */
    if (watch_for == MHD_FD_STATE_NONE)
    {
        /* Remove from DLL */
        vlc_list_remove(&app_cntx->node);
        free(app_cntx);
        sys->total_fds--;
        vlc_restorecancel(canc);
        return NULL;
    }
    /* socket has been added */
    if (NULL == app_cntx)
    {
        /* First time, allocate data structure to keep
           the socket and MHD's context */
        app_cntx = malloc(sizeof(MHD_APP_SOCKET_CNTX_TYPE));
        if (NULL == app_cntx)
        {
            vlc_restorecancel(canc);
            return NULL; /* closes connection */
        }

        /* prepend to DLL */
        vlc_list_append(&app_cntx->node, &sys->socket_contexts);
        app_cntx->fd = fd;
        sys->total_fds++;
    }
    /* socket already exists, just update it */
    else
    {
        /* socket must not change */
        assert(fd == app_cntx->fd);
    }
    /* MHD could change its associated context, so always update */
    app_cntx->ecb_cntx = ecb_cntx;
    app_cntx->daemon = daemon;
    app_cntx->events = 0;

    if (watch_for & MHD_FD_STATE_RECV)
        app_cntx->events |= POLLIN;
    if (watch_for & MHD_FD_STATE_SEND)
        app_cntx->events |= POLLOUT;

    vlc_restorecancel(canc);
    return app_cntx;
}

static const struct MHD_Action *req_handler(void *cls,
                                            struct MHD_Request *request,
                                            const struct MHD_String *path,
                                            enum MHD_HTTP_Method method,
                                            uint_fast64_t upload_size)
{
    struct MHD_Response *internal_server_error =
        MHD_response_from_buffer_static(MHD_HTTP_STATUS_INTERNAL_SERVER_ERROR,
                                        strlen(INTERNAL_SERVER_ERROR_RESPONSE),
                                        INTERNAL_SERVER_ERROR_RESPONSE);

    (void)MHD_response_add_header(
        internal_server_error, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");

    struct MHD_Response *not_found_error =
        MHD_response_from_buffer_static(MHD_HTTP_STATUS_NOT_FOUND,
                                        strlen(NOT_FOUND_ERROR_RESPONSE),
                                        NOT_FOUND_ERROR_RESPONSE);

    (void)MHD_response_add_header(
        not_found_error, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");

    struct MHD_Response *ret = internal_server_error;

    (void)upload_size;
    vlc_httpd_host_t *host = cls;
    const char *url_string = path->cstr;
    vlc_httpd_url_t *url;
    union MHD_RequestInfoDynamicData req_data;
    const struct MHD_AuthDigestInfo *uname;
    enum MHD_StatusCode res;
    static const char realm[] = "VLC httpd server";
    static const char auth_required_page[] =
        "You need to know the credentials to get in.\n";
    char *allowed_username = NULL;
    char *allowed_password = NULL;
    size_t allowed_username_len;

    vlc_httpd_message_t *query = malloc(sizeof(*query));

    if (query == NULL)
        return MHD_action_from_response(request, internal_server_error);

    vlc_httpd_message_t *answer = malloc(sizeof(*answer));

    if (answer == NULL)
    {
        free(query);
        return MHD_action_from_response(request, internal_server_error);
    }

    vlc_httpd_MsgInit(query);
    vlc_httpd_MsgInit(answer);

    struct MHD_Response *response;
    int canc = vlc_savecancel();
    vlc_mutex_lock(&host->lock);
    vlc_list_foreach (url, &host->urls, node)
    {
        if (strcmp(url->url, url_string))
            continue;

        /* digest authentication */
        if (url->password != NULL && url->password[0] != '\0')
        {
            allowed_username = url->user;
            allowed_username_len = strlen(allowed_username);
            allowed_password = url->password;

            res = MHD_request_get_info_dynamic(
                request, MHD_REQUEST_INFO_DYNAMIC_AUTH_DIGEST_INFO, &req_data);

            if (MHD_SC_AUTH_ABSENT == res)
            {
                vlc_mutex_unlock(&host->lock);
                vlc_httpd_MsgClean(query);
                vlc_httpd_MsgClean(answer);
                free(query);
                free(answer);
                vlc_restorecancel(canc);
                return MHD_action_digest_auth_challenge_a(
                    request,
                    realm,
                    "0",
                    NULL,
                    MHD_NO,
                    MHD_DIGEST_AUTH_MULT_QOP_AUTH,
                    MHD_DIGEST_AUTH_MULT_ALGO_ANY,
                    MHD_NO,
                    MHD_YES,
                    MHD_response_from_buffer_static(
                        MHD_HTTP_STATUS_UNAUTHORIZED,
                        sizeof(auth_required_page) / sizeof(char) - 1,
                        auth_required_page));
            }

            if (MHD_SC_REQ_AUTH_DATA_BROKEN == res)
            {
                vlc_mutex_unlock(&host->lock);
                vlc_httpd_MsgClean(query);
                vlc_httpd_MsgClean(answer);
                free(query);
                free(answer);
                vlc_restorecancel(canc);
                return MHD_action_digest_auth_challenge_a(
                    request,
                    realm,
                    "0",
                    NULL,
                    MHD_NO,
                    MHD_DIGEST_AUTH_MULT_QOP_AUTH,
                    MHD_DIGEST_AUTH_MULT_ALGO_ANY,
                    MHD_NO,
                    MHD_YES,
                    MHD_response_from_buffer_static(
                        MHD_HTTP_STATUS_UNAUTHORIZED,
                        sizeof(auth_required_page) / sizeof(char) - 1,
                        auth_required_page));
            }

            if (MHD_SC_OK != res)
            {
                vlc_mutex_unlock(&host->lock);
                vlc_httpd_MsgClean(query);
                vlc_httpd_MsgClean(answer);
                free(query);
                free(answer);
                vlc_restorecancel(canc);
                return MHD_action_abort_request(request);
            }

            uname = req_data.v_auth_digest_info;

            /* username mismatch */
            if ((uname->username.len != allowed_username_len) ||
                (memcmp(allowed_username,
                        uname->username.cstr,
                        uname->username.len) != 0))
            {
                vlc_mutex_unlock(&host->lock);
                vlc_httpd_MsgClean(query);
                vlc_httpd_MsgClean(answer);
                free(query);
                free(answer);
                vlc_restorecancel(canc);
                return MHD_action_digest_auth_challenge_a(
                    request,
                    realm,
                    "0",
                    NULL,
                    MHD_NO,
                    MHD_DIGEST_AUTH_MULT_QOP_AUTH,
                    MHD_DIGEST_AUTH_MULT_ALGO_ANY,
                    MHD_NO,
                    MHD_YES,
                    MHD_response_from_buffer_static(
                        MHD_HTTP_STATUS_UNAUTHORIZED,
                        sizeof(auth_required_page) / sizeof(char) - 1,
                        auth_required_page));
            }

            enum MHD_DigestAuthResult auth_res;

            auth_res = MHD_digest_auth_check(request,
                                             realm,
                                             allowed_username,
                                             allowed_password,
                                             0,
                                             MHD_DIGEST_AUTH_MULT_QOP_AUTH,
                                             MHD_DIGEST_AUTH_MULT_ALGO_ANY);

            if (MHD_DAUTH_NONCE_STALE == auth_res)
            {
                vlc_mutex_unlock(&host->lock);
                vlc_httpd_MsgClean(query);
                vlc_httpd_MsgClean(answer);
                free(query);
                free(answer);
                vlc_restorecancel(canc);
                return MHD_action_digest_auth_challenge_a(
                    request,
                    realm,
                    "0",
                    NULL,
                    MHD_YES,
                    MHD_DIGEST_AUTH_MULT_QOP_AUTH,
                    MHD_DIGEST_AUTH_MULT_ALGO_ANY,
                    MHD_NO,
                    MHD_YES,
                    MHD_response_from_buffer_static(
                        MHD_HTTP_STATUS_UNAUTHORIZED,
                        sizeof(auth_required_page) / sizeof(char) - 1,
                        auth_required_page));
            }

            if (auth_res <= 0)
            {
                vlc_mutex_unlock(&host->lock);
                vlc_httpd_MsgClean(query);
                vlc_httpd_MsgClean(answer);
                free(query);
                free(answer);
                vlc_restorecancel(canc);
                return MHD_action_digest_auth_challenge_a(
                    request,
                    realm,
                    "0",
                    NULL,
                    MHD_NO,
                    MHD_DIGEST_AUTH_MULT_QOP_AUTH,
                    MHD_DIGEST_AUTH_MULT_ALGO_ANY,
                    MHD_NO,
                    MHD_YES,
                    MHD_response_from_buffer_static(
                        MHD_HTTP_STATUS_UNAUTHORIZED,
                        sizeof(auth_required_page) / sizeof(char) - 1,
                        auth_required_page));
            }

            /* authentication successful */
            assert(MHD_DAUTH_OK == auth_res);
        }

        if (fill_query(query, request, method, url_string))
            goto cleanup;

        if (url->stream_mode)
        {
            struct cb_data_context *sdc = malloc(sizeof(*sdc));
            if (sdc == NULL)
                goto cleanup;

            sdc->query = query;
            sdc->answer = answer;
            sdc->lock = url->lock;
            sdc->cb = url->catch[query->method].cb;
            sdc->sys = url->catch[query->method].sys;
            sdc->timeout_sec = host->timeout_sec;

            /* parse stream headers for the first time */
            vlc_mutex_lock(&url->lock);
            if (sdc->cb(sdc->sys, sdc->answer, sdc->query))
            {
                free(sdc);
                vlc_mutex_unlock(&url->lock);
                goto cleanup;
            }
            vlc_mutex_unlock(&url->lock);

            enum MHD_HTTP_StatusCode response_status = sdc->answer->status;

            response = MHD_response_from_callback(response_status,
                                                  MHD_SIZE_UNKNOWN,
                                                  stream_cb,
                                                  sdc,
                                                  stream_cleanup_cb);

            /* parse the headers of `answer` into the response */
            for (size_t i = 0; i < sdc->answer->headerslen; i++)
            {
                if (sdc->answer->headers[i].value != NULL)
                {
                    (void)MHD_response_add_header(
                        response,
                        sdc->answer->headers[i].name,
                        sdc->answer->headers[i].value);
                }
            }

            vlc_mutex_unlock(&host->lock);
            vlc_restorecancel(canc);
            /* The below cleanup is handled in stream_cleanup_cb */
            /* vlc_httpd_MsgClean(query);
            vlc_httpd_MsgClean(answer);
            free(query);
            free(answer); */
            return MHD_action_from_response(request, response);
        }

        /* process request bodies only for POST requests */
        if (method == MHD_HTTP_METHOD_POST)
        {
            struct cb_data_context *pdc = malloc(sizeof(*pdc));
            if (pdc == NULL)
                goto cleanup;

            pdc->query = query;
            pdc->answer = answer;
            pdc->lock = url->lock;
            pdc->cb = url->catch[query->method].cb;
            pdc->sys = url->catch[query->method].sys;
            pdc->error_response = internal_server_error;
            pdc->response = response;
            vlc_mutex_unlock(&host->lock);
            vlc_restorecancel(canc);
            return MHD_action_process_upload(request,
                                             MAX_POST_BODY_SIZE,
                                             &post_data_parser,
                                             pdc,
                                             NULL,
                                             NULL);
        }
        else
        {
            if (url->catch[query->method].cb == NULL)
                goto cleanup;

            vlc_mutex_lock(&url->lock);
            if (url->catch[query->method].cb(
                    url->catch[query->method].sys, answer, query))
            {
                vlc_mutex_unlock(&url->lock);
                goto cleanup;
            }
            vlc_mutex_unlock(&url->lock);

            if (parse_answer(answer, &response))
                goto cleanup;

            vlc_mutex_unlock(&host->lock);
            vlc_httpd_MsgClean(query);
            vlc_httpd_MsgClean(answer);
            free(query);
            free(answer);
            vlc_restorecancel(canc);
            return MHD_action_from_response(request, response);
        }
    }
    ret = not_found_error;

cleanup:
    vlc_mutex_unlock(&host->lock);
    vlc_httpd_MsgClean(query);
    vlc_httpd_MsgClean(answer);
    free(query);
    free(answer);
    vlc_restorecancel(canc);

    /* free the response that isn't consumed by MHD */
    if (ret == internal_server_error)
        MHD_response_destroy(not_found_error);
    else
        MHD_response_destroy(internal_server_error);

    return MHD_action_from_response(request, ret);
}

/* TLS callbacks passed to MHD */
static const struct MHD_DaemonTLSCallbacks tls_cbs = {
    vlc_tls_ServerCreate,
    vlc_tls_ServerDelete,
    mhd_ServerSessionCreateFromFd,
    vlc_tls_Close,
    vlc_tls_SessionHandshake,
    vlc_tls_Shutdown,
    mhd_NetRecv,
    NULL,
    mhd_NetSend,
    NULL,
};

/* event loop */
static void mhd_Loop(vlc_httpd_host_t *host)
{
    uint_fast64_t next_wait, max_wait = 0;
    int timeout;
    vlc_httpd_host_sys_t *sys = host->sys;
    struct vlc_httpd_daemon **vlc_daemons = sys->vlc_daemons;
    struct vlc_httpd_socket_context *pos;
    unsigned int i;

    for (i = 0; i < host->nfd; i++)
    {
        /* Perform all daemon activities based on FDs events provided by
         * MHD_daemon_event_update() */
        MHD_daemon_process_reg_events(vlc_daemons[i]->daemon, &next_wait);
        /* store the maximum wait time */
        if (next_wait > max_wait)
            max_wait = next_wait;
    }
    timeout = (int)(max_wait / 1000); /* convert to milliseconds */

    struct pollfd ufd[sys->total_fds];
    nfds_t nfd = 0;

    vlc_list_foreach (pos, &sys->socket_contexts, node)
    {
        ufd[nfd].fd = pos->fd;
        ufd[nfd].events = pos->events;
        ufd[nfd].revents = 0;
        nfd++;
    }

    while (vlc_poll_i11e(ufd, nfd, timeout) < 0)
    {
        if (errno != EINTR)
            msg_Err(host, "polling error: %s", vlc_strerror_c(errno));
        else
            msg_Info(host, "polling interrupted by signal, retrying...");
    }

    i = 0;

    vlc_list_foreach (pos, &sys->socket_contexts, node)
    {
        enum MHD_FdState current_state = MHD_FD_STATE_NONE;

        if (ufd[i].revents & POLLIN)
            current_state |= MHD_FD_STATE_RECV;
        if (ufd[i].revents & POLLOUT)
            current_state |= MHD_FD_STATE_SEND;
        if (ufd[i].revents & POLLERR)
            current_state |= MHD_FD_STATE_EXCEPT;

        i++;

        if (current_state == MHD_FD_STATE_NONE)
            continue; /* no events to process */

        /* Update the sockets state. Must be called for every socket that got
         * state updated.*/
        MHD_daemon_event_update(pos->daemon, pos->ecb_cntx, current_state);
    }
}

static void *mhd_HostThread(void *data)
{
    vlc_thread_set_name("vlc-httpd-mhd");

    vlc_httpd_host_t *host = data;

    while (atomic_load_explicit(&host->ref, memory_order_relaxed) > 0)
        mhd_Loop(host);

    return NULL;
}

static void Close(vlc_httpd_host_t *host)
{
    vlc_httpd_host_sys_t *sys = host->sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    /* deallocate sys data members */
    for (unsigned int i = 0; i < host->nfd; i++)
    {
        MHD_daemon_destroy(sys->vlc_daemons[i]->daemon);
        free(sys->vlc_daemons[i]);
    }
    assert(vlc_list_is_empty(&sys->socket_contexts));
    assert(sys->total_fds == 0);
    free(sys->vlc_daemons);
    free(sys);
}

static const struct vlc_httpd_host_operations host_ops = {.close = Close};

static int Open(vlc_httpd_host_t *host)
{
    int ret = VLC_EGENERIC;
    char entropy_bytes[32];
    vlc_httpd_host_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    host->sys = sys;

    if (!init_entropy_bytes(entropy_bytes, sizeof(entropy_bytes)))
    {
        msg_Err(host, "Failed to initialize entropy bytes for authentication");
        goto cleanup_sys;
    }

    /* initialise and allocate sys data members */
    sys->total_fds = 0;
    vlc_list_init(&sys->socket_contexts);

    struct vlc_httpd_daemon **vlc_daemons =
        vlc_alloc(host->nfd, sizeof(struct vlc_httpd_daemon *));
    if (vlc_daemons == NULL)
    {
        msg_Err(host, "Failed to allocate memory for VLC daemons array");
        ret = VLC_ENOMEM;
        goto cleanup_sys;
    }

    sys->vlc_daemons = vlc_daemons;

    MHD_Socket socket = MHD_INVALID_SOCKET;

    for (unsigned int i = 0; i < host->nfd; i++)
    {
        vlc_daemons[i] = malloc(sizeof(struct vlc_httpd_daemon));
        if (vlc_daemons[i] == NULL)
        {
            msg_Err(host,
                    "Failed to allocate memory for VLC daemon array element");
            ret = VLC_ENOMEM;
            goto cleanup_vlc_daemons;
        }

        vlc_daemons[i]->sys = sys;
        vlc_daemons[i]->daemon = MHD_daemon_create(req_handler, host);
        if (vlc_daemons[i]->daemon == NULL)
        {
            msg_Err(host, "Failed to create MHD daemon");
            goto cleanup_vlc_daemons;
        }

        /* NOTE: On Unix, MHD_Socket is an int, and MHD_INVALID_SOCKET is -1.
           On Windows, it is SOCKET, and MHD_INVALID_SOCKET is INVALID_SOCKET.
         */
        socket = host->fds[i];

        if ((host->cert != NULL) && (host->key != NULL))
        {
            if (MHD_SC_OK !=
                MHD_DAEMON_SET_OPTIONS(
                    vlc_daemons[i]->daemon,
                    MHD_D_OPTION_EXTERNAL_TLS_DATA(&host->obj, &tls_cbs),
                    MHD_D_OPTION_TLS_CERT_KEY(host->cert, host->key, NULL),
                    MHD_D_OPTION_REREGISTER_ALL(MHD_YES),
                    MHD_D_OPTION_WM_EXTERNAL_EVENT_LOOP_CB_LEVEL(
                        sock_reg_update_cb, vlc_daemons[i]),
                    MHD_D_OPTION_DEFAULT_TIMEOUT(host->timeout_sec),
                    MHD_D_OPTION_RANDOM_ENTROPY(sizeof(entropy_bytes),
                                                entropy_bytes),
                    MHD_D_OPTION_LISTEN_SOCKET(socket)))
            {
                msg_Err(host, "Failed to set option for MHD daemon");
                goto cleanup_vlc_daemons;
            }
        }
        else
        {
            if (MHD_SC_OK !=
                MHD_DAEMON_SET_OPTIONS(
                    vlc_daemons[i]->daemon,
                    MHD_D_OPTION_REREGISTER_ALL(MHD_YES),
                    MHD_D_OPTION_WM_EXTERNAL_EVENT_LOOP_CB_LEVEL(
                        sock_reg_update_cb, vlc_daemons[i]),
                    MHD_D_OPTION_DEFAULT_TIMEOUT(host->timeout_sec),
                    MHD_D_OPTION_RANDOM_ENTROPY(sizeof(entropy_bytes),
                                                entropy_bytes),
                    MHD_D_OPTION_LISTEN_SOCKET(socket)))
            {
                msg_Err(host, "Failed to set option for MHD daemon");
                goto cleanup_vlc_daemons;
            }
        }

        if (MHD_SC_OK != MHD_daemon_start(vlc_daemons[i]->daemon))
        {
            msg_Err(host, "Failed to start MHD daemon");
            goto cleanup_vlc_daemons;
        }
    }

    /* create the event loop thread */
    if (vlc_clone(&sys->thread, mhd_HostThread, host))
    {
        msg_Err(host, "cannot spawn http host thread");
        goto cleanup_vlc_daemons;
    }

    msg_Info(host, "HTTP server running");

    host->ops = &host_ops;

    return VLC_SUCCESS;

    /* Cleanups */

cleanup_vlc_daemons:
    for (unsigned int i = 0; i < host->nfd; i++)
    {
        if (vlc_daemons[i])
        {
            if (vlc_daemons[i]->daemon)
                MHD_daemon_destroy(vlc_daemons[i]->daemon);
            free(vlc_daemons[i]);
        }
    }
    free(vlc_daemons);

cleanup_sys:
    free(sys);
    return ret;
}

vlc_module_begin()
    set_shortname(N_("microhttpd"))
    set_description(N_("microhttpd httpd plugin"))
    set_capability("httpd", 1)
    set_subcategory(SUBCAT_ADVANCED_NETWORK)
    add_shortcut("microhttpd", "mh")
    set_callback(Open)
vlc_module_end()
