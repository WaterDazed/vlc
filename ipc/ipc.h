/*****************************************************************************
 * ipc.h
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
#ifndef VLC_IPC_H
#define VLC_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#define VLC_IPC_VERSION "0.0.0"

enum vlc_ipc_category {
    VLC_IPC_CATEGORY_CUSTOM,
    VLC_IPC_CATEGORY_MAX
};

const char *vlc_ipc_get_signature(void);

int vlc_ipc_recv_data(int fd, struct iovec *iovec, int nmemb);
int vlc_ipc_send_data(int fd, struct iovec *iovec, int nmemb);

static inline int vlc_ipc_recv_uint32(int fd, uint32_t *u32) {
  struct iovec iov = { .iov_base = u32, .iov_len = sizeof(uint32_t) };
  return vlc_ipc_recv_data(fd, &iov, 1);
}

static inline int vlc_ipc_send_uint32(int fd, uint32_t u32) {
  struct iovec iov = { .iov_base = &u32, .iov_len = sizeof(uint32_t) };
  return vlc_ipc_send_data(fd, &iov, 1);
}

int vlc_ipc_send_string(int fd, const char *str);
int vlc_ipc_recv_string(int fd, char **string);


#endif /*VLC_IPC_H */
