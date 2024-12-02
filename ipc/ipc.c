/*****************************************************************************
 * ipc.c
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
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
 ****************************************************************************/

#include <unistd.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include <vlc_interrupt.h>
#include "ipc.h"

const char *vlc_ipc_get_signature() {
  return VLC_IPC_VERSION
#if (defined(_WIN64 ) || (defined(__WORDSIZE) &&  __WORDSIZE == 64))
    ".64"
#elif (defined(_WIN32) || (defined(__WORDSIZE) && __WORDSIZE == 32))
    ".32"
#else
#error "Unexpected or missing wordsize"
#endif
#ifdef WORDS_BIGENDIAN
    ".be"
#else
    ".le"
#endif
    ;
}

static size_t update_iovec(size_t at, struct iovec *vec, size_t nmemb, size_t updated) {
    for (; updated != 0 && at < nmemb;) {
        size_t min = vec[at].iov_len;
        size_t rm = (min < updated)? min: updated;
        vec[at].iov_len -= rm;
        updated -= rm;
        if (vec[at].iov_len != 0) {
            vec[at].iov_base = &((char*) vec[at].iov_base)[rm];
        } else {
            at++;
        }
    }
    return at;
}

int vlc_ipc_send_data(int fd, struct iovec *vec, int nmemb) {
    bool killed = false;
    int at = 0;
    while (at < nmemb && (killed = vlc_killed()) == false) {
        ssize_t wr = vlc_writev_i11e(fd, &vec[at], (int) nmemb - at);
        if (wr < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        } else if (wr == 0) {
            size_t len = 0;
            int i;
            for (i = at; i < nmemb && len != 0; i++)
                len += vec[at].iov_len;
            return len != 0? -EPIPE: VLC_SUCCESS;
        }
        at = update_iovec(at, vec, nmemb, wr);
    }

    return killed? VLC_EGENERIC: VLC_SUCCESS;
}

int vlc_ipc_recv_data(int fd, struct iovec *vec, int nmemb) {
    bool killed = false;
    int at = 0;
    while (at < nmemb && (killed = vlc_killed()) == false) {
        ssize_t wr = vlc_readv_i11e(fd, &vec[at], nmemb - at);
        if (wr < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        } else if (wr == 0) {
            size_t len = 0;
            int i;
            for (i = at; i < nmemb && len != 0; i++)
                len += vec[at].iov_len;
            return len != 0? -EPIPE: VLC_SUCCESS;
        }
        at = update_iovec(at, vec, nmemb, wr);
    }

    return killed? VLC_EGENERIC: VLC_SUCCESS;
}

int vlc_ipc_send_string(int fd, const char *str) {
    size_t len;
    struct iovec vec[2] = {{.iov_base = &len, .iov_len = sizeof(len)}};
    if (str == NULL) {
        /* It is impossible to have a SIZE_MAX strlen string since the trailing
         * null character would overflow size_t so let's use this value to
         * "send" NULL strings */
        len = SIZE_MAX;
        return vlc_ipc_send_data(fd, vec, 1);
    }

    len = strlen(str);
    vec[1].iov_base = (void *) str;
    vec[1].iov_len = len;
    return vlc_ipc_send_data(fd, vec, 2);
}

int vlc_ipc_recv_string(int fd, char **string) {
    size_t len;
    struct iovec vec = {.iov_base = &len, .iov_len = sizeof(len)};
    int ret = vlc_ipc_recv_data(fd, &vec, 1);

    if (len == SIZE_MAX) {
        *string = NULL;
        return VLC_SUCCESS;
    }

    char *str = malloc(len + 1);
    if (str == NULL)
        return VLC_ENOMEM;

    vec.iov_base = str;
    vec.iov_len = len;
    ret = vlc_ipc_recv_data(fd, &vec, 1);
    if (ret != VLC_SUCCESS) {
        free(str);
        return ret;
    }
    str[len] = '\0';
    *string = str;
    return VLC_SUCCESS;
}
