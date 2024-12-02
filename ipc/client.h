/*****************************************************************************
 * client.h
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
#ifndef VLC_IPC_CLIENT_H
#define VLC_IPC_CLIENT_H

#include <vlc_threads.h>
#include "ipc.h"

typedef struct vlc_object_t vlc_object_t;
struct vlc_ipc_client;

typedef int (*vlc_ipc_client_evt_handler)(struct vlc_ipc_client *, void* opaque);

struct vlc_ipc_client {
  int send_fd;
  int recv_fd;
  int event_fd; /* Also used to pass commands */
  int interrupt_fd[2];

  vlc_mutex_t lock; /* Used to lock the fd */

  vlc_object_t *instance; /* libvlc instance to serve as parent */

  vlc_ipc_client_evt_handler handle_event;
  void *opaque;

  vlc_thread_t evt_thread;
  bool abort_on_io_error; /* default to true in init */
};

int vlc_ipc_client_init(struct vlc_ipc_client *client, int recv_fd,
                           int send_fd, int event_fd, vlc_object_t *instance,
                           vlc_ipc_client_evt_handler evt_handler,
                           void* opaque);

int vlc_ipc_client_start_event_thread(struct vlc_ipc_client *client);
int vlc_ipc_client_stop_event_thread(struct vlc_ipc_client *client);
void vlc_ipc_client_term(struct vlc_ipc_client *client);

int vlc_ipc_execute_cmd(struct vlc_ipc_client *client, uint32_t category, uint32_t command,
                           int (*send_data)(struct vlc_ipc_client *client, const void *data), const void *command_data,
                           int (*recv_data)(struct vlc_ipc_client *client, void *data), void *response);

#endif /*VLC_IPC_CLIENT_H */
