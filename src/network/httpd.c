/*****************************************************************************
 * httpd.c
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
#include <stdlib.h>
#include <string.h>

#include <vlc_block.h>
#include <vlc_charset.h>
#include <vlc_httpd.h>
#include <vlc_mime.h>
#include <vlc_modules.h>
#include <vlc_network.h>
#include <vlc_rand.h>
#include <vlc_strings.h>

#include "../libvlc.h"

/* TODO: Temporary workaround, as the zero-copy buffer transfer mechanism
   has not yet been implemented by MHD. This will be removed later, since
   Chunked Transfer Encoding does not impose an upper limit on the chunk
   size and should send as much data as is available from the stream. */
#define VLC_HTTPD_STREAM_CHUNK_SIZE 32768

/* power of 2 for optimization */
#define VLC_HTTPD_STREAM_BUFFER_SIZE (4 * 1024 * 1024)

/* Use a do-while(0) block to ensure this macro behaves as a single statement,
   even when used without braces in if-else constructs, preventing ambiguity
   in control flow. */
#define ADD_HEADER_OR_FAIL(msg, key, fmt, ...)                                 \
    do                                                                         \
    {                                                                          \
        if (vlc_httpd_MsgAddHeader((msg), (key), (fmt), ##__VA_ARGS__) !=      \
            VLC_SUCCESS)                                                       \
            return VLC_ENOMEM;                                                 \
    } while (0)

void vlc_httpd_MsgInit(vlc_httpd_message_t *msg)
{
    msg->method = VLC_HTTPD_MSG_NONE;
    msg->proto = VLC_HTTPD_PROTO_NONE;
    msg->version = 0;
    msg->status = 0;
    msg->url = NULL;
    msg->args = NULL;
    msg->headerslen = 0;
    msg->headers = NULL;
    msg->body_offset = 0;
    msg->bodylen = 0;
    msg->body = NULL;
    msg->keyframe_wait_to_pass = -1;
    msg->max_chunk_size = VLC_HTTPD_STREAM_CHUNK_SIZE;
}

void vlc_httpd_MsgClean(vlc_httpd_message_t *msg)
{
    free(msg->url);
    free(msg->args);
    for (size_t i = 0; i < msg->headerslen; i++)
    {
        free(msg->headers[i].name);
        free(msg->headers[i].value);
    }
    free(msg->headers);
    free(msg->body);

    vlc_httpd_MsgInit(msg);
}

int vlc_httpd_MsgAddHeader(vlc_httpd_message_t *msg,
                           const char *name,
                           const char *value,
                           ...)
{
    vlc_httpd_header *tmp =
        realloc(msg->headers, sizeof(vlc_httpd_header) * (msg->headerslen + 1));
    if (!tmp)
        return VLC_ENOMEM;

    msg->headers = tmp;

    vlc_httpd_header *h = &msg->headers[msg->headerslen];
    h->name = strdup(name);
    if (!h->name)
        return VLC_ENOMEM;

    h->value = NULL;

    va_list args;
    va_start(args, value);
    int ret = vlc_vasprintf_c(&h->value, value, args);
    va_end(args);

    if (ret == -1)
    {
        free(h->name);
        return VLC_ENOMEM;
    }

    msg->headerslen++;
    return VLC_SUCCESS;
}

const char *vlc_httpd_MsgGetHeader(const vlc_httpd_message_t *msg,
                                   const char *name)
{
    for (size_t i = 0; i < msg->headerslen; i++)
        if (!strcasecmp(msg->headers[i].name, name))
            return msg->headers[i].value;
    return NULL;
}

vlc_httpd_url_t *vlc_httpd_UrlNew(vlc_httpd_host_t *host,
                                  const char *url_string,
                                  vlc_httpd_credential_t *crd,
                                  bool stream_mode)
{
    vlc_httpd_url_t *url;

    assert(url_string);

    vlc_mutex_lock(&host->lock);
    vlc_list_foreach (url, &host->urls, node)
    {
        if (!strcmp(url_string, url->url))
        {
            msg_Warn(host, "cannot add '%s' (url already defined)", url_string);
            vlc_mutex_unlock(&host->lock);
            return NULL;
        }
    }

    url = malloc(sizeof(*url));
    if (unlikely(url == NULL))
    {
        vlc_mutex_unlock(&host->lock);
        return NULL;
    }

    url->url = NULL;
    url->user = NULL;
    url->password = NULL;
    url->stream_mode = stream_mode;
    url->host = host;

    vlc_mutex_init(&url->lock);

    url->url = strdup(url_string);
    if (url->url == NULL)
        goto error;

    if (crd != NULL)
    {
        url->user = strdup(crd->user ? crd->user : "");
        if (url->user == NULL)
            goto error;

        url->password = strdup(crd->password ? crd->password : "");
        if (url->password == NULL)
            goto error;
    }

    for (int i = 0; i < VLC_HTTPD_MSG_MAX; i++)
    {
        url->catch[i].cb = NULL;
        url->catch[i].sys = NULL;
    }

    vlc_list_append(&url->node, &host->urls);
    vlc_mutex_unlock(&host->lock);

    return url;

error:
    free(url->password);
    free(url->user);
    free(url->url);

    free(url);
    vlc_mutex_unlock(&host->lock);
    return NULL;
}

int vlc_httpd_UrlCatch(vlc_httpd_url_t *url,
                       vlc_httpd_method_t method,
                       vlc_httpd_callback_t cb,
                       vlc_httpd_callback_sys_t *sys)
{
    if (method >= VLC_HTTPD_MSG_MAX)
        return VLC_EGENERIC;

    vlc_mutex_lock(&url->lock);
    url->catch[method].cb = cb;
    url->catch[method].sys = sys;
    vlc_mutex_unlock(&url->lock);

    return VLC_SUCCESS;
}

void vlc_httpd_UrlDelete(vlc_httpd_url_t *url)
{
    vlc_httpd_host_t *host = url->host;

    vlc_mutex_lock(&host->lock);
    vlc_list_remove(&url->node);

    free(url->url);
    free(url->user);
    free(url->password);

    free(url);
    vlc_mutex_unlock(&host->lock);
}

/*****************************************************************************
 * High Level Functions
 *****************************************************************************/

struct vlc_httpd_file_t
{
    vlc_httpd_url_t *url;
    vlc_httpd_file_cbs *cbs;
    vlc_httpd_file_sys_t *sys;
    char *mime;
};

static int vlc_httpd_FileCallBack(vlc_httpd_callback_sys_t *sys,
                                  vlc_httpd_message_t *answer,
                                  const vlc_httpd_message_t *query)
{
    vlc_httpd_file_t *file = (vlc_httpd_file_t *)sys;

    if (!answer || !query)
        return VLC_EGENERIC; /* erroring out if either request or response isn't
                                defined */

    answer->proto = VLC_HTTPD_PROTO_HTTP;
    answer->version = 1;
    answer->method = VLC_HTTPD_MSG_ANSWER;

    answer->status = 200;

    ADD_HEADER_OR_FAIL(answer, "Content-type", "%s", file->mime);
    ADD_HEADER_OR_FAIL(answer, "Cache-Control", "%s", "no-cache");

    vlc_httpd_file_args file_args = {
        .sys = file->sys, .request = query, .response = answer};

    if (file->cbs->fill(&file_args))
    {
        msg_Err(file->url->host,
                "Failed to fill file response for URL %s",
                file->url->url);
        return VLC_EGENERIC;
    }

    ADD_HEADER_OR_FAIL(answer, "Content-Length", "%zu", answer->bodylen);

    return VLC_SUCCESS;
}

vlc_httpd_file_t *vlc_httpd_FileNew(vlc_httpd_host_t *host,
                                    const char *url,
                                    vlc_httpd_method_t method,
                                    const char *mime,
                                    vlc_httpd_credential_t *crd,
                                    vlc_httpd_file_cbs *cbs,
                                    vlc_httpd_file_sys_t *sys)
{
    if (method >= VLC_HTTPD_MSG_MAX)
    {
        msg_Err(host, "Invalid HTTP method %d", method);
        return NULL;
    }

    const char *vlc_mime = mime;
    if (mime == NULL || mime[0] == '\0')
        vlc_mime = vlc_mime_Ext2Mime(url);

    vlc_httpd_file_t *file = malloc(sizeof(*file));
    if (unlikely(file == NULL))
        return NULL;

    file->mime = strdup(vlc_mime);
    if (!file->mime)
    {
        free(file);
        return NULL;
    }

    file->url = vlc_httpd_UrlNew(host, url, crd, false);
    if (!file->url)
    {
        free(file->mime);
        free(file);
        return NULL;
    }

    file->cbs = cbs;
    file->sys = sys;

    vlc_httpd_UrlCatch(file->url,
                       method,
                       vlc_httpd_FileCallBack,
                       (vlc_httpd_callback_sys_t *)file);

    return file;
}

vlc_httpd_file_sys_t *vlc_httpd_FileDelete(vlc_httpd_file_t *file)
{
    vlc_httpd_url_t *url = file->url;
    vlc_httpd_file_sys_t *sys = file->sys;

    vlc_httpd_UrlDelete(url);
    free(file->mime);
    free(file);
    return sys;
}

struct vlc_httpd_redirect_t
{
    vlc_httpd_url_t *url;
    char dst[1];
};

static int vlc_httpd_RedirectCallBack(vlc_httpd_callback_sys_t *sys,
                                      vlc_httpd_message_t *answer,
                                      const vlc_httpd_message_t *query)
{
    vlc_httpd_redirect_t *rdir = (vlc_httpd_redirect_t *)sys;
    char *body;

    if (!answer || !query)
        return VLC_EGENERIC; /* erroring out if either request or response isn't
                                defined */

    answer->proto = VLC_HTTPD_PROTO_HTTP; /* not required by MHD, required by
                                             legacy httpd */
    answer->version = 1; /* not required by MHD, required by legacy httpd */
    answer->method = VLC_HTTPD_MSG_ANSWER;
    answer->status = 301;

    const char *url = rdir->dst;
    char *url_Encoded = vlc_xml_encode(url ? url : "");

    int res =
        asprintf(&body,
                 "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                 "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\""
                 " \"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
                 "<html lang=\"en\">\n"
                 "<head>\n"
                 "<title>%s</title>\n"
                 "</head>\n"
                 "<body>\n"
                 "<h1>%d %s%s%s%s</h1>\n"
                 "<hr />\n"
                 "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
                 "</body>\n"
                 "</html>\n",
                 "Internal server error",
                 301,
                 "Internal server error",
                 (url_Encoded ? " (" : ""),
                 (url_Encoded ? url_Encoded : ""),
                 (url_Encoded ? ")" : ""));

    free(url_Encoded);

    if (res == -1)
        return VLC_EGENERIC;

    answer->body = (unsigned char *)body;
    answer->bodylen = (size_t)res;

    ADD_HEADER_OR_FAIL(answer, "Location", "%s", rdir->dst);
    ADD_HEADER_OR_FAIL(answer, "Content-Length", "%zu", answer->bodylen);

    return VLC_SUCCESS;
}

vlc_httpd_redirect_t *vlc_httpd_RedirectNew(vlc_httpd_host_t *host,
                                            const char *url_dst,
                                            const char *url_src)
{
    size_t dstlen = strlen(url_dst);

    vlc_httpd_redirect_t *rdir = malloc(sizeof(*rdir) + dstlen);
    if (unlikely(rdir == NULL))
        return NULL;

    rdir->url = vlc_httpd_UrlNew(host, url_src, NULL, false);
    if (!rdir->url)
    {
        free(rdir);
        return NULL;
    }
    memcpy(rdir->dst, url_dst, dstlen + 1);

    vlc_httpd_UrlCatch(rdir->url,
                       VLC_HTTPD_MSG_HEAD,
                       vlc_httpd_RedirectCallBack,
                       (vlc_httpd_callback_sys_t *)rdir);
    vlc_httpd_UrlCatch(rdir->url,
                       VLC_HTTPD_MSG_GET,
                       vlc_httpd_RedirectCallBack,
                       (vlc_httpd_callback_sys_t *)rdir);
    vlc_httpd_UrlCatch(rdir->url,
                       VLC_HTTPD_MSG_POST,
                       vlc_httpd_RedirectCallBack,
                       (vlc_httpd_callback_sys_t *)rdir);

    return rdir;
}

void vlc_httpd_RedirectDelete(vlc_httpd_redirect_t *rdir)
{
    vlc_httpd_UrlDelete(rdir->url);
    free(rdir);
}

struct vlc_httpd_stream_t
{
    vlc_mutex_t lock;
    vlc_httpd_url_t *url;

    char *mime;

    /* Header to send as first packet */
    uint8_t *header;
    size_t headerlen;

    /* Some muxes, in particular the avformat mux, can mark given blocks
     * as keyframes, to ensure that the stream starts with one.
     * (This is particularly important for WebM streaming to certain
     * browsers.) Store if we've ever seen any such keyframe blocks,
     * and if so, the byte position of the start of the last one. */
    bool has_keyframes;
    int64_t last_keyframe_seen_pos;

    /* circular buffer */
    size_t buffer_size;
    uint8_t *buffer;        /* buffer */
    uint64_t abs_write_pos; /* absolute write position */
    uint64_t abs_read_pos;  /* absolute read position */

    /* custom headers */
    size_t http_headers_len;
    vlc_httpd_header *http_headers;
};

static int vlc_httpd_StreamCallBack(vlc_httpd_callback_sys_t *sys,
                                    vlc_httpd_message_t *answer,
                                    const vlc_httpd_message_t *query)
{
    vlc_httpd_stream_t *stream = (vlc_httpd_stream_t *)sys;

    if (!answer || !query)
        return VLC_SUCCESS;

    if (answer->body_offset > 0)
    {
        uint64_t pos;

        vlc_mutex_lock(&stream->lock);
        if (answer->body_offset >= stream->abs_write_pos)
        {
            vlc_mutex_unlock(&stream->lock);
            return VLC_EGENERIC; /* wait, no data available */
        }

        if (answer->keyframe_wait_to_pass >= 0)
        {
            if (stream->last_keyframe_seen_pos <= answer->keyframe_wait_to_pass)
            {
                /* still waiting for the next keyframe */
                vlc_mutex_unlock(&stream->lock);
                return VLC_EGENERIC;
            }

            /* seek to the new keyframe */
            answer->body_offset = stream->last_keyframe_seen_pos;
            answer->keyframe_wait_to_pass = -1;
        }

        if (answer->body_offset + stream->buffer_size < stream->abs_write_pos)
            answer->body_offset =
                stream->abs_read_pos; /* this client isn't fast enough */

        pos = answer->body_offset % stream->buffer_size;
        size_t write = stream->abs_write_pos - answer->body_offset;

        /* TODO: Temporary workaround, as the zero-copy buffer transfer
           mechanism has not yet been implemented by MHD. This will be removed
           later, since Chunked Transfer Encoding does not impose an upper limit
           on the chunk size and should send as much data as is available from
           the stream. */
        if (write > answer->max_chunk_size)
            write = answer->max_chunk_size;
        else if (write <= 0)
        {
            vlc_mutex_unlock(&stream->lock);
            return VLC_EGENERIC; /* wait, no data available */
        }

        answer->proto = VLC_HTTPD_PROTO_HTTP;
        answer->version = 1;
        answer->method = VLC_HTTPD_MSG_ANSWER;

        answer->bodylen = write;
        answer->body = malloc(write);
        if (answer->body == NULL)
        {
            vlc_mutex_unlock(&stream->lock);
            return VLC_ENOMEM;
        }

        size_t written = 0;
        uint64_t read_pos = pos;
        for (int i = 0; i < 2 && written < write; i++)
        {
            size_t chunk =
                __MIN(write - written, stream->buffer_size - read_pos);
            memcpy((uint8_t *)answer->body + written,
                   &stream->buffer[read_pos],
                   chunk);
            written += chunk;
            read_pos = 0; /* wrap to start of buffer for second iteration */
        }

        answer->body_offset += write;
        vlc_mutex_unlock(&stream->lock);

        return VLC_SUCCESS;
    }
    else
    {
        answer->proto = VLC_HTTPD_PROTO_HTTP;
        answer->version = 1;
        answer->method = VLC_HTTPD_MSG_ANSWER;

        answer->status = 200;

        bool has_content_type = false;
        bool has_cache_control = false;

        vlc_mutex_lock(&stream->lock);
        for (size_t i = 0; i < stream->http_headers_len; i++)
            if (strcasecmp(stream->http_headers[i].name, "Content-Length"))
            {
                if (vlc_httpd_MsgAddHeader(answer,
                                           stream->http_headers[i].name,
                                           "%s",
                                           stream->http_headers[i].value))
                {
                    vlc_mutex_unlock(&stream->lock);
                    return VLC_ENOMEM;
                }

                if (!strcasecmp(stream->http_headers[i].name, "Content-Type"))
                    has_content_type = true;
                else if (!strcasecmp(stream->http_headers[i].name,
                                     "Cache-Control"))
                    has_cache_control = true;
            }
        vlc_mutex_unlock(&stream->lock);

        if (query->method != VLC_HTTPD_MSG_HEAD)
        {
            vlc_mutex_lock(&stream->lock);
            /* Send the header */
            if (stream->headerlen > 0)
            {
                answer->bodylen = stream->headerlen;
                answer->body = malloc(stream->headerlen);
                if (answer->body == NULL)
                {
                    vlc_mutex_unlock(&stream->lock);
                    return VLC_ENOMEM;
                }
                memcpy(answer->body, stream->header, stream->headerlen);
            }
            answer->body_offset = stream->abs_read_pos;
            if (stream->has_keyframes)
                answer->keyframe_wait_to_pass = stream->last_keyframe_seen_pos;
            else
                answer->keyframe_wait_to_pass = -1;
            vlc_mutex_unlock(&stream->lock);
        }
        else
        {
            ADD_HEADER_OR_FAIL(answer, "Content-Length", "0");
            answer->body_offset = 0;
        }

        /* FIXME: move to http access_output */
        if (!strcmp(stream->mime, "video/x-ms-asf-stream"))
        {
            bool xplaystream = false;

            ADD_HEADER_OR_FAIL(
                answer, "Content-type", "application/octet-stream");
            ADD_HEADER_OR_FAIL(answer, "Server", "Cougar 4.1.0.3921");
            ADD_HEADER_OR_FAIL(answer, "Pragma", "no-cache");
            ADD_HEADER_OR_FAIL(
                answer, "Pragma", "client-id=%lu", vlc_mrand48() & 0x7fff);
            ADD_HEADER_OR_FAIL(answer, "Pragma", "features=\"broadcast\"");

            /* Check if there is a xPlayStrm=1 */
            for (size_t i = 0; i < query->headerslen; i++)
                if (!strcasecmp(query->headers[i].name, "Pragma") &&
                    strstr(query->headers[i].value, "xPlayStrm=1"))
                    xplaystream = true;

            if (!xplaystream)
                answer->body_offset = 0;
        }
        else if (!has_content_type)
            ADD_HEADER_OR_FAIL(answer, "Content-type", "%s", stream->mime);

        if (!has_cache_control)
            ADD_HEADER_OR_FAIL(answer, "Cache-Control", "no-cache");

        ADD_HEADER_OR_FAIL(answer, "Connection", "close");

        return VLC_SUCCESS;
    }
}

vlc_httpd_stream_t *vlc_httpd_StreamNew(vlc_httpd_host_t *host,
                                        const char *url,
                                        const char *mime,
                                        vlc_httpd_credential_t *crd)
{
    vlc_httpd_stream_t *stream = malloc(sizeof(*stream));
    if (!stream)
        return NULL;

    stream->mime = NULL;
    stream->buffer = NULL;

    stream->url = vlc_httpd_UrlNew(host, url, crd, true);
    if (!stream->url)
        goto error;

    vlc_mutex_init(&stream->lock);
    if (mime == NULL || mime[0] == '\0')
        mime = vlc_mime_Ext2Mime(url);

    stream->mime = strdup(mime);
    if (stream->mime == NULL)
        goto error;

    stream->headerlen = 0;
    stream->header = NULL;
    stream->buffer_size = VLC_HTTPD_STREAM_BUFFER_SIZE;

    stream->buffer = malloc(stream->buffer_size);
    if (stream->buffer == NULL)
        goto error;

    /* We set to 1 to make life simpler
     * (this way body_offset can never be 0) */
    stream->abs_write_pos = 1;
    stream->abs_read_pos = 1;
    stream->has_keyframes = false;
    stream->last_keyframe_seen_pos = 0;
    stream->http_headers_len = 0;
    stream->http_headers = NULL;

    vlc_httpd_UrlCatch(stream->url,
                       VLC_HTTPD_MSG_HEAD,
                       vlc_httpd_StreamCallBack,
                       (vlc_httpd_callback_sys_t *)stream);
    vlc_httpd_UrlCatch(stream->url,
                       VLC_HTTPD_MSG_GET,
                       vlc_httpd_StreamCallBack,
                       (vlc_httpd_callback_sys_t *)stream);
    vlc_httpd_UrlCatch(stream->url,
                       VLC_HTTPD_MSG_POST,
                       vlc_httpd_StreamCallBack,
                       (vlc_httpd_callback_sys_t *)stream);

    return stream;

error:
    free(stream->mime);

    if (stream->url)
        vlc_httpd_UrlDelete(stream->url);

    free(stream);

    return NULL;
}

int vlc_httpd_StreamHeader(vlc_httpd_stream_t *stream,
                           uint8_t *data,
                           size_t datalen)
{
    vlc_mutex_lock(&stream->lock);
    free(stream->header);
    stream->header = NULL;

    stream->headerlen = datalen;
    if (datalen > 0)
    {
        stream->header = malloc(datalen);
        if (stream->header == NULL)
        {
            vlc_mutex_unlock(&stream->lock);
            return VLC_ENOMEM;
        }
        memcpy(stream->header, data, datalen);
    }
    vlc_mutex_unlock(&stream->lock);

    return VLC_SUCCESS;
}

static void
vlc_httpd_AppendData(vlc_httpd_stream_t *stream, uint8_t *data, size_t datalen)
{
    /* if datalen >= buffer_size, only keep the last buffer_size bytes */
    if (datalen >= stream->buffer_size)
    {
        data += (datalen - stream->buffer_size);
        datalen = stream->buffer_size;
        /* all old data is overwritten */
        stream->abs_read_pos =
            stream->abs_write_pos + datalen - stream->buffer_size;
    }
    else
    {
        /* Calculate used space */
        uint64_t used = stream->abs_write_pos - stream->abs_read_pos;
        if (datalen > stream->buffer_size - used)
        {
            /* Not enough space, advance abs_read_pos to discard oldest data */
            stream->abs_read_pos += (datalen - (stream->buffer_size - used));
        }
    }

    uint64_t pos = stream->abs_write_pos % stream->buffer_size;
    size_t count = datalen;
    while (count > 0)
    {
        size_t copy = __MIN(count, stream->buffer_size - pos);

        memcpy(&stream->buffer[pos], data, copy);

        pos = (pos + copy) % stream->buffer_size;
        count -= copy;
        data += copy;
    }

    stream->abs_write_pos += datalen;
}

int vlc_httpd_StreamSend(vlc_httpd_stream_t *stream, const block_t *block)
{
    if (!block || !block->p_buffer)
        return VLC_SUCCESS;

    vlc_mutex_lock(&stream->lock);

    stream->abs_read_pos = stream->abs_write_pos;

    if (block->i_flags & BLOCK_FLAG_TYPE_I)
    {
        stream->has_keyframes = true;
        stream->last_keyframe_seen_pos = stream->abs_write_pos;
    }

    vlc_httpd_AppendData(stream, block->p_buffer, block->i_buffer);

    vlc_mutex_unlock(&stream->lock);
    return VLC_SUCCESS;
}

int vlc_httpd_StreamSetHTTPHeaders(vlc_httpd_stream_t *stream,
                                   const vlc_httpd_header *headers,
                                   size_t headerslen)
{
    if (!stream)
        return VLC_EGENERIC;

    vlc_mutex_lock(&stream->lock);
    if (stream->http_headers)
    {
        for (size_t i = 0; i < stream->http_headers_len; i++)
        {
            free(stream->http_headers[i].name);
            free(stream->http_headers[i].value);
        }
        free(stream->http_headers);
        stream->http_headers = NULL;
        stream->http_headers_len = 0;
    }

    if (!headers || !headerslen)
    {
        vlc_mutex_unlock(&stream->lock);
        return VLC_SUCCESS;
    }

    stream->http_headers = vlc_alloc(headerslen, sizeof(vlc_httpd_header));
    if (!stream->http_headers)
    {
        vlc_mutex_unlock(&stream->lock);
        return VLC_ENOMEM;
    }

    size_t j = 0;
    for (size_t i = 0; i < headerslen; i++)
    {
        if (unlikely(!headers[i].name || !headers[i].value))
            continue;

        stream->http_headers[j].name = strdup(headers[i].name);
        stream->http_headers[j].value = strdup(headers[i].value);

        if (unlikely(!stream->http_headers[j].name ||
                     !stream->http_headers[j].value))
        {
            free(stream->http_headers[j].name);
            free(stream->http_headers[j].value);
            for (size_t k = 0; k < j; k++)
            {
                free(stream->http_headers[k].name);
                free(stream->http_headers[k].value);
            }
            free(stream->http_headers);
            stream->http_headers = NULL;
            stream->http_headers_len = 0;
            vlc_mutex_unlock(&stream->lock);
            return VLC_ENOMEM;
        }
        j++;
    }
    stream->http_headers_len = j;
    vlc_mutex_unlock(&stream->lock);
    return VLC_SUCCESS;
}

void vlc_httpd_StreamDelete(vlc_httpd_stream_t *stream)
{
    vlc_httpd_UrlDelete(stream->url);
    for (size_t i = 0; i < stream->http_headers_len; i++)
    {
        free(stream->http_headers[i].name);
        free(stream->http_headers[i].value);
    }
    free(stream->http_headers);
    free(stream->mime);
    free(stream->header);
    free(stream->buffer);
    free(stream);
}

/*****************************************************************************
 * Low level
 *****************************************************************************/

static struct vlc_httpd
{
    vlc_mutex_t mutex;
    struct vlc_list hosts;
} httpd = {VLC_STATIC_MUTEX, VLC_LIST_INITIALIZER(&httpd.hosts)};

static vlc_httpd_host_t *vlc_httpd_HostCreate(vlc_object_t *obj,
                                              const char *hostvar,
                                              const char *portvar,
                                              const char *cert,
                                              const char *key)
{
    vlc_httpd_host_t *host;
    unsigned port = var_InheritInteger(obj, portvar);

    /* to be sure to avoid multiple creation */
    vlc_mutex_lock(&httpd.mutex);

    /* verify if it already exist */
    vlc_list_foreach (host, &httpd.hosts, node)
    {
        /* cannot mix TLS and non-TLS hosts */
        if (host->port != port || (host->cert != NULL) != (cert != NULL))
            continue;

        /* Increase existing matching host reference count. */
        atomic_fetch_add_explicit(&host->ref, 1, memory_order_relaxed);

        vlc_mutex_unlock(&httpd.mutex);
        return host;
    }

    host =
        (vlc_httpd_host_t *)vlc_custom_create(obj, sizeof(*host), "http host");
    if (host == NULL)
        goto error;

    atomic_init(&host->ref, 1);

    char *hostname = var_InheritString(obj, hostvar);

    host->fds = net_ListenTCP(obj, hostname, port);
    free(hostname);

    vlc_mutex_init(&host->lock);
    vlc_list_init(&host->urls);

    if (host->fds == NULL)
    {
        msg_Err(obj, "cannot create socket(s) for HTTP host");
        goto error;
    }
    for (host->nfd = 0; host->fds[host->nfd] != -1; host->nfd++)
        ;

    host->port = port;
    host->timeout_sec = var_InheritInteger(obj, "http-timeout");
    host->cert = NULL;
    host->key = NULL;

    if (cert != NULL)
    {
        host->cert = strdup(cert);
        if (host->cert == NULL)
        {
            msg_Err(obj, "cannot allocate memory for TLS certificate");
            goto error;
        }
    }

    if (key != NULL)
    {
        host->key = strdup(key);
        if (host->key == NULL)
        {
            msg_Err(obj, "cannot allocate memory for TLS key");
            goto error;
        }
    }

    module_t **mods;
    ssize_t total = vlc_module_match("httpd", NULL, false, &mods, NULL);

    for (ssize_t i = 0; i < total; i++)
    {
        int (*probe)(vlc_httpd_host_t *);

        probe = vlc_module_map(obj->logger, mods[i]);

        if (probe != NULL && probe(host) == VLC_SUCCESS)
        {
            free(mods);
            vlc_list_append(&host->node, &httpd.hosts);
            vlc_mutex_unlock(&httpd.mutex);

            msg_Info(host, "Host created");

            return host;
        }

        vlc_objres_clear(VLC_OBJECT(host));
    }

    free(mods);
    msg_Err(host, "no suitable httpd module");

error:
    if (host != NULL)
    {
        free((void *)host->cert);
        free((void *)host->key);
        net_ListenClose(host->fds);
        vlc_object_delete(host);
    }

    vlc_mutex_unlock(&httpd.mutex);

    return NULL;
}

vlc_httpd_host_t *vlc_httpd_HostNew(vlc_object_t *obj)
{
    return vlc_httpd_HostCreate(obj, "http-host", "http-port", NULL, NULL);
}

vlc_httpd_host_t *vlc_httpd_HostNewTLS(vlc_object_t *obj)
{
    char *cert = var_InheritString(obj, "http-cert");
    if (cert == NULL)
    {
        msg_Err(obj, "HTTP/TLS certificate not specified!");
        return NULL;
    }

    char *key = var_InheritString(obj, "http-key");
    if (key == NULL)
    {
        msg_Err(obj, "HTTP/TLS key not specified!");
        free(cert);
        return NULL;
    }

    vlc_httpd_host_t *host =
        vlc_httpd_HostCreate(obj, "http-host", "https-port", cert, key);
    free(cert);
    free(key);
    return host;
}

void vlc_httpd_HostDelete(vlc_httpd_host_t *host)
{
    vlc_mutex_lock(&httpd.mutex);

    if (atomic_fetch_sub_explicit(&host->ref, 1, memory_order_relaxed) > 1)
    {
        vlc_mutex_unlock(&httpd.mutex);
        msg_Dbg(host, "vlc_httpd_HostDelete: host still in use");
        return;
    }

    assert(vlc_list_is_empty(&host->urls));
    vlc_list_remove(&host->node);

    if (host->ops->close)
        host->ops->close(host);

    vlc_objres_clear(VLC_OBJECT(host));

    free((void *)host->cert);
    free((void *)host->key);

    msg_Info(host, "HTTP host removed");

    vlc_object_delete(host);
    vlc_mutex_unlock(&httpd.mutex);
}
