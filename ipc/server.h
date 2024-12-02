/*****************************************************************************
 * server.h
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
#ifndef VLC_IPC_SERVER_H
#define VLC_IPC_SERVER_H

#include <stdint.h>
#include <vlc_threads.h>

#include "ipc.h"

typedef struct vlc_object_t vlc_object_t;
struct vlc_ipc_server;

struct vlc_ipc_server_category_handler {
  int (*handle_command)(struct vlc_ipc_server *server, uint32_t command, void *opaque);
  void *opaque;
};

typedef void (*vlc_ipc_server_on_error)(int error, void *opaque);

struct vlc_ipc_server {
  int send_fd;
  int recv_fd;
  int interrupt_fd[2];

  vlc_mutex_t lock;
  bool running;
  vlc_thread_t processing_thread;

  vlc_ipc_server_on_error on_error;
  void *error_opaque;

  vlc_object_t *obj_this;
  struct vlc_ipc_server_category_handler handlers[VLC_IPC_CATEGORY_MAX];
};

struct vlc_ipc_server *vlc_ipc_server_new(int recv_fd, int send_fd, vlc_object_t *obj);

typedef int (*vlc_ipc_server_send_event_param)(int fd, void *param);

static inline int vlc_ipc_server_send_event(int event_fd, uint32_t event,
                                            vlc_ipc_server_send_event_param send_param,
                                            void *param) {
    int ret = vlc_ipc_send_uint32(event_fd, event);
    if (ret != VLC_SUCCESS)
        return ret;

    if (send_param != NULL)
        return send_param(event_fd, param);

    return VLC_SUCCESS;
}

int vlc_ipc_server_start(struct vlc_ipc_server *srv,
                         vlc_ipc_server_on_error on_error, void *opaque);


/* Must *NOT* be called from vlc_ipc_server_on_error callback */
void vlc_ipc_server_stop(struct vlc_ipc_server *srv);
void vlc_ipc_server_del(struct vlc_ipc_server *srv);

#endif /* VLC_IPC_SERVER_H */
