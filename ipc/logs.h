/*****************************************************************************
 * logs.h
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
#ifndef VLC_IPC_LOGS_H
#define VLC_IPC_LOGS_H

#include <vlc_messages.h>

struct vlc_ipc_client;
struct vlc_ipc_server;

int vlc_ipc_client_log(struct vlc_ipc_client *client, enum vlc_log_type type, const char *msg);

int vlc_ipc_server_register_logs(struct vlc_ipc_server *server);
void vlc_ipc_server_unregister_logs(struct vlc_ipc_server *server);

#endif /* VLC_IPC_LOGS_H */
