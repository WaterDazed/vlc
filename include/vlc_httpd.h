/*****************************************************************************
 * vlc_httpd.h: builtin HTTP/RTSP server.
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

#ifndef VLC_HTTPD_H
#define VLC_HTTPD_H 1

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <vlc_list.h>
#include <vlc_threads.h>

typedef enum vlc_httpd_method_t
{
    VLC_HTTPD_MSG_NONE,

    /* answer */
    VLC_HTTPD_MSG_ANSWER,

    /* http request */
    VLC_HTTPD_MSG_GET,
    VLC_HTTPD_MSG_HEAD,
    VLC_HTTPD_MSG_POST,

    /* rtsp request */
    /* VLC_HTTPD_MSG_OPTIONS,
    VLC_HTTPD_MSG_DESCRIBE,
    VLC_HTTPD_MSG_SETUP,
    VLC_HTTPD_MSG_PLAY,
    VLC_HTTPD_MSG_PAUSE,
    VLC_HTTPD_MSG_GETPARAMETER,
    VLC_HTTPD_MSG_TEARDOWN, */

    /* just to track the count of MSG */
    VLC_HTTPD_MSG_MAX
} vlc_httpd_method_t;

enum
{
    VLC_HTTPD_PROTO_NONE,
    VLC_HTTPD_PROTO_HTTP, /* HTTP */
    VLC_HTTPD_PROTO_RTSP, /* RTSP */
};

typedef struct vlc_httpd_host_t vlc_httpd_host_t;

/**
 * Create a new HTTP host.
 *
 * @return A pointer to the newly created host object, or NULL on failure.
 */
VLC_API vlc_httpd_host_t *vlc_httpd_HostNew(vlc_object_t *) VLC_USED;

/**
 * Create a new HTTP host with TLS support.
 * This function requires the "http-cert" and "http-key" variables to be set.
 *
 * @return A pointer to the newly created host object, or NULL on failure.
 */
VLC_API vlc_httpd_host_t *vlc_httpd_HostNewTLS(vlc_object_t *) VLC_USED;

/**
 * Delete an HTTP host.
 */
VLC_API void vlc_httpd_HostDelete(vlc_httpd_host_t *host);

/**
 * HTTP header structure.
 */
typedef struct
{
    char *name;
    char *value;
} vlc_httpd_header;

/**
 * HTTP request/response message structure.
 */
typedef struct vlc_httpd_message_t
{
    vlc_httpd_method_t method;
    uint8_t proto;   /* not required by MHD, required by legacy httpd */
    uint8_t version; /* not required by MHD, required by legacy httpd */

    /* response status */
    unsigned int status;

    /* request URL */
    char *url;

    /* request query parameters */
    uint8_t *args;

    /* request and response headers */
    size_t headerslen;
    vlc_httpd_header *headers;

    /* body */
    uint64_t body_offset;
    size_t bodylen;
    uint8_t *body;

    int64_t keyframe_wait_to_pass; /* only used for stream responses */

    /* TODO: Temporary workaround, as the zero-copy buffer transfer mechanism
       has not yet been implemented by MHD. This will be removed later, since
       Chunked Transfer Encoding does not impose an upper limit on the chunk
       size and should send as much data as is available from the stream. */
    size_t max_chunk_size;
} vlc_httpd_message_t;

/**
 * Initialize a message structure
 */
VLC_API void vlc_httpd_MsgInit(vlc_httpd_message_t *msg);

/**
 * Clean a message structure
 */
VLC_API void vlc_httpd_MsgClean(vlc_httpd_message_t *msg);

/**
 * Add a header to the message
 *
 * @param msg The message structure to which the header will be added.
 * @param name The name of the header.
 * @param value The value of the header, which can be a format string.
 * @return VLC_SUCCESS on success, VLC_ENOMEM on memory allocation failure.
 */
VLC_API int vlc_httpd_MsgAddHeader(vlc_httpd_message_t *msg,
                                   const char *name,
                                   const char *value,
                                   ...) VLC_FORMAT(3, 4);

/**
 * Get a header value from the message
 * @param msg The message structure from which to get the header.
 * @param name The name of the header to retrieve.
 * @return The value of the header, or NULL if the header does not exist.
 */
VLC_API const char *vlc_httpd_MsgGetHeader(const vlc_httpd_message_t *,
                                           const char *name);

typedef struct vlc_httpd_credential_t
{
    const char *user;
    const char *password;
} vlc_httpd_credential_t;

typedef struct vlc_httpd_url_t vlc_httpd_url_t;
typedef struct vlc_httpd_callback_sys_t vlc_httpd_callback_sys_t;
typedef int (*vlc_httpd_callback_t)(vlc_httpd_callback_sys_t *,
                                    vlc_httpd_message_t *answer,
                                    const vlc_httpd_message_t *query);

struct vlc_httpd_url_t
{
    vlc_httpd_host_t *host;
    struct vlc_list node;
    vlc_mutex_t lock;

    char *url;
    char *user;
    char *password;

    /* true if this url is used for streaming
       (progressive delivery of long/ongoing dynamic content),
       false for non-stream use cases */
    bool stream_mode;

    struct
    {
        vlc_httpd_callback_t cb;
        vlc_httpd_callback_sys_t *sys;
    } catch[VLC_HTTPD_MSG_MAX];
};

struct vlc_httpd_host_operations
{
    void (*close)(vlc_httpd_host_t *);
};

struct vlc_httpd_host_t
{
    struct vlc_object_t obj;
    module_t *module;
    struct vlc_list node;
    /* ref count */
    atomic_uint ref;

    const char *cert;
    const char *key;

    vlc_mutex_t lock;
    struct vlc_list urls;

    /* address/port and socket for listening at connections */
    int *fds;
    unsigned nfd;
    unsigned port;

    unsigned timeout_sec;

    void *sys; /* private data for callbacks */

    const struct vlc_httpd_host_operations *ops;
};

/**
 * Create a new URL for the HTTP server.
 *
 * @param host The host to which the URL belongs.
 * @param url The URL string.
 * @param crd Optional credentials for the URL.
 * @param stream_mode True if the URL is for streaming, false otherwise.
 * @return A pointer to the newly created URL object, or NULL on failure.
 */
VLC_API vlc_httpd_url_t *vlc_httpd_UrlNew(vlc_httpd_host_t *,
                                          const char *url,
                                          vlc_httpd_credential_t *crd,
                                          bool stream_mode) VLC_USED;

/**
 * Register a callback on a URL.
 *
 * @param method The HTTP method for which the callback is registered.
 * @param cb The callback function to be called when the method is invoked.
 * @param sys Optional user-defined data to be passed to the callback.
 * @return VLC_SUCCESS on success, or an error code on failure.
 */
VLC_API int vlc_httpd_UrlCatch(vlc_httpd_url_t *,
                               vlc_httpd_method_t method,
                               vlc_httpd_callback_t,
                               vlc_httpd_callback_sys_t *);

/**
 * Delete a URL from the HTTP server.
 */
VLC_API void vlc_httpd_UrlDelete(vlc_httpd_url_t *);

/* High level */

typedef struct vlc_httpd_file_t vlc_httpd_file_t;
typedef struct vlc_httpd_file_sys_t vlc_httpd_file_sys_t;

typedef struct vlc_httpd_file_args
{
    vlc_httpd_file_sys_t *sys;
    const vlc_httpd_message_t *request;
    vlc_httpd_message_t *response;
} vlc_httpd_file_args;

typedef struct vlc_httpd_file_cbs
{
    int (*fill)(vlc_httpd_file_args *args);
} vlc_httpd_file_cbs;

/**
 * Create a new file handler for the HTTP server.
 *
 * @param url The URL for the file.
 * @param method The HTTP method to handle (GET, POST, etc.).
 * @param mime The MIME type of the file.
 * @param crd Optional credentials for accessing the file.
 * @param cbs Callbacks for handling file operations.
 * @param sys Optional user-defined data for the file handler.
 * @return A pointer to the newly created file handler, or NULL on failure.
 */
VLC_API vlc_httpd_file_t *vlc_httpd_FileNew(vlc_httpd_host_t *,
                                            const char *url,
                                            vlc_httpd_method_t method,
                                            const char *mime,
                                            vlc_httpd_credential_t *crd,
                                            vlc_httpd_file_cbs *cbs,
                                            vlc_httpd_file_sys_t *) VLC_USED;

/**
 * Delete a file handler from the HTTP server.
 *
 * @return optional user-defined data for the file handler
 */
VLC_API vlc_httpd_file_sys_t *vlc_httpd_FileDelete(vlc_httpd_file_t *);

typedef struct vlc_httpd_redirect_t vlc_httpd_redirect_t;

/**
 * Create a new redirect handler for the HTTP server.
 *
 * @param url_dst The destination URL for the redirect.
 * @param url_src The source URL that triggers the redirect.
 * @return A pointer to the newly created redirect handler, or NULL on failure.
 */
VLC_API vlc_httpd_redirect_t *vlc_httpd_RedirectNew(vlc_httpd_host_t *,
                                                    const char *url_dst,
                                                    const char *url_src)
    VLC_USED;

/**
 * Delete a redirect handler from the HTTP server.
 */
VLC_API void vlc_httpd_RedirectDelete(vlc_httpd_redirect_t *);

typedef struct vlc_httpd_stream_t vlc_httpd_stream_t;

/**
 * Create a new stream handler for the HTTP server.
 *
 * @param url The URL for the stream.
 * @param mime The MIME type of the stream.
 * @param crd Optional credentials for accessing the stream.
 * @return A pointer to the newly created stream handler, or NULL on failure.
 */
VLC_API vlc_httpd_stream_t *vlc_httpd_StreamNew(vlc_httpd_host_t *,
                                                const char *url,
                                                const char *mime,
                                                vlc_httpd_credential_t *crd)
    VLC_USED;

/**
 * Delete a stream handler from the HTTP server.
 */
VLC_API void vlc_httpd_StreamDelete(vlc_httpd_stream_t *);

/**
 * Set stream header data.
 * This function sets the header that will be sent as the first packet of the
 * stream.
 *
 * @param data Pointer to the header data.
 * @param datalen Length of the header data.
 * @return VLC_SUCCESS on success, or an error code on failure.
 */
VLC_API int
vlc_httpd_StreamHeader(vlc_httpd_stream_t *, uint8_t *data, size_t datalen);

/**
 * Send a block of data to the stream.
 *
 * @param block The block of data to send.
 * @return VLC_SUCCESS on success, or an error code on failure.
 */
VLC_API int vlc_httpd_StreamSend(vlc_httpd_stream_t *, const block_t *block);

/**
 * Set HTTP headers for the stream.
 * This function allows setting custom HTTP headers for the stream.
 *
 * @param headers Pointer to an array of HTTP headers.
 * @param headerslen Length of the headers array.
 * @return VLC_SUCCESS on success, or an error code on failure.
 */
VLC_API int vlc_httpd_StreamSetHTTPHeaders(vlc_httpd_stream_t *,
                                           const vlc_httpd_header *,
                                           size_t);

#endif /* _VLC_HTTPD_H */
