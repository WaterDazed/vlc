/*****************************************************************************
 * runner.h
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
#ifndef VLC_IPC_RUNNER_H
#define VLC_IPC_RUNNER_H

#include "client.h"

struct vlc_ipc_runner_interface;

struct vlc_ipc_runner_ops {
    /* Interpret on the fly command and should push blocking command to the queue with vlc_ipc_runner_push_command() */
    int (*event_reader)(struct vlc_ipc_runner_interface *intf, uint32_t command);
    /* Interpret command pushed using vlc_ipc_runner_push_command() returning anything but VLC_SUCCESS will exit the command thread and runner */
    int (*command_handler)(struct vlc_ipc_runner_interface *intf, uint32_t command, const void *param);
};

struct vlc_ipc_runner_interface {
    struct vlc_object_t obj;
    struct vlc_ipc_client client; /* started by the runner */
    void *sys;
    const struct vlc_ipc_runner_ops *ops;
};

/* called from the event_reader to execute commands in the background */
int vlc_ipc_runner_push_command(struct vlc_ipc_runner_interface *intf, uint32_t command, void *param, void (*free_param)(void *));


pid_t vlc_ipc_spawn_runner(int recv_fd, int send_fd, int event_fd, const char *runner_interface_name);

#endif /* VLC_IPC_RUNNER_H */
