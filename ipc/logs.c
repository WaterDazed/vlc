/*****************************************************************************
 * logs.c
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
#include <assert.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include <vlc_messages.h>

#include "ipc.h"
#include "client.h"
#include "server.h"
#include "logs.h"

#include "log_ipc.h"

int vlc_ipc_client_log(struct vlc_ipc_client *client, enum vlc_log_type type, const char *msg) {
    const struct vlc_ipc_log log = { .msg = (char *) msg };
    return vlc_ipc_execute_cmd(client, VLC_IPC_CATEGORY_LOGS, type,
                                  vlc_ipc_cmd_send_log, &log, NULL, NULL);
}

static int handle_msg_command(struct vlc_ipc_server *server, uint32_t command, void *opaque) {
    VLC_UNUSED(opaque);
    struct vlc_ipc_log log;
    int ret = vlc_ipc_recv_log(server->recv_fd, &log);
    if (ret != VLC_SUCCESS)
        return ret;

    switch(command) {
    case VLC_MSG_ERR:
        msg_Err(server->obj_this, "%s", log.msg);
        break;
    case VLC_MSG_WARN:
        msg_Warn(server->obj_this, "%s", log.msg);
        break;
    case VLC_MSG_INFO:
        msg_Info(server->obj_this, "%s", log.msg);
        break;
    case VLC_MSG_DBG:
        msg_Dbg(server->obj_this, "%s", log.msg);
        break;
    default:
        vlc_assert_unreachable();
    }
    vlc_ipc_cleanup_log(&log);
    return VLC_SUCCESS;
}

int vlc_ipc_server_register_logs(struct vlc_ipc_server *server) {
    if (server->obj_this == NULL)
        return VLC_EINVAL;

    server->handlers[VLC_IPC_CATEGORY_LOGS].handle_command = handle_msg_command;
    server->handlers[VLC_IPC_CATEGORY_LOGS].opaque = NULL;
    return VLC_SUCCESS;
}

void vlc_ipc_server_unregister_logs(struct vlc_ipc_server *server) {
    server->handlers[VLC_IPC_CATEGORY_LOGS].handle_command = NULL;
}
