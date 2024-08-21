/*****************************************************************************
 * runnerlib.h
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <vlc_common.h>
#include <vlc_spawn.h>

#include "runner_internal.h"

int vlc_ipc_runner_push_command(struct vlc_ipc_runner_interface *intf, uint32_t command, void *param, void (*free_param)(void *)) {
    struct vlc_ipc_runner_cmd *cmd = malloc(sizeof(struct vlc_ipc_runner_cmd));
    if (cmd == NULL)
        return VLC_ENOMEM;

    struct vlc_ipc_runner_intf_owner *owner = container_of(intf, struct vlc_ipc_runner_intf_owner, intf);

    cmd->command = command;
    cmd->param = param;
    cmd->free = free_param;
    vlc_mutex_lock(&owner->lock);
    vlc_list_append(&cmd->fifo, &owner->commands);
    vlc_cond_signal(&owner->cond);
    vlc_mutex_unlock(&owner->lock);
    return 0;
}

pid_t vlc_ipc_spawn_runner(int recv_fd, int send_fd, int event_fd, const char *runner_interface_name) {
    pid_t runner_pid;
    const char *argv[4] = {
#ifdef _WIN32
        "vlc-ipc-runner.exe",
#else
        "vlc-ipc-runner",
#endif
        vlc_ipc_get_signature(),
        runner_interface_name,
        NULL
    };

    const int fdv[4] = {recv_fd, send_fd, event_fd, -1};
    int ret = vlc_spawnp(&runner_pid, argv[0], fdv, argv);
    if (ret != 0) {
        runner_pid = -1;
    }
    return runner_pid;
}

