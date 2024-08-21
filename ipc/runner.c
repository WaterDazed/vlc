/*****************************************************************************
 * runner.c
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
#include "ipc.h"
#include "client.h"
#include "logs.h"
#include "runner_internal.h"

#include "../lib/libvlc_internal.h" /* For libvlc instance object */

const char vlc_module_name[] = "vlc-ipc-runner";

static void libvlc_ipc_log_cb(void *data, int level, const libvlc_log_t *ctx,
                   const char *fmt, va_list args) {
    VLC_UNUSED(ctx);
    struct vlc_ipc_client *client = data;

    enum vlc_log_type lt;
    switch (level) {
    case LIBVLC_DEBUG:
        lt = VLC_MSG_DBG;
        break;
    case LIBVLC_NOTICE:
        lt = VLC_MSG_INFO;
        break;
    case LIBVLC_WARNING:
        lt = VLC_MSG_WARN;
        break;
    case LIBVLC_ERROR:
        lt = VLC_MSG_ERR;
        break;
    default:
        abort();
    }

    char *str;
    if (vasprintf(&str, fmt, args) == -1)
        abort();

    if (vlc_ipc_client_log(client, lt, str) != VLC_SUCCESS)
      abort();

    free(str);
}

static void *exec_commands(void *data) {
    struct vlc_ipc_runner_intf_owner *owner = data;
    vlc_mutex_lock(&owner->lock);
    while (owner->run) {
        if (vlc_list_is_empty(&owner->commands)) {
            vlc_cond_wait(&owner->cond, &owner->lock);
            continue;
        }

        struct vlc_ipc_runner_cmd *cmd = vlc_list_first_entry_or_null(&owner->commands, struct vlc_ipc_runner_cmd, fifo);
        assert(cmd != NULL);
        vlc_list_remove(&cmd->fifo);
        vlc_mutex_unlock(&owner->lock);

        if (owner->intf.ops->command_handler(&owner->intf, cmd->command, cmd->param) != VLC_SUCCESS) {
            vlc_mutex_lock(&owner->lock);
            owner->run = false;
            vlc_mutex_unlock(&owner->lock);
        }

        if (cmd->free != NULL)
            cmd->free(cmd->param);
        free(cmd);

        vlc_mutex_lock(&owner->lock);
    }
    vlc_mutex_unlock(&owner->lock);
    return NULL;
}

static int evt_handler(struct vlc_ipc_client *client, void *opaque) {
    struct vlc_ipc_runner_intf_owner *owner = opaque;
    uint32_t command;

    int ret = vlc_ipc_recv_uint32(client->event_fd, &command);
    if (ret != VLC_SUCCESS) {
        return ret;
    }

    return owner->intf.ops->event_reader(&owner->intf, command);
}

int main(int argc, char *argv[]) {
    /* the runner takes 2 argument, the ipc signature to check against its own and
     * the runner interface module name. There is no point in sending anything to
     * stdout or stderr since they come from pipes with the "wrong" direction
     * the runner requires stdin/stdout valid for IPC commands from the client */
    if (argc != 3 || STDOUT_FILENO < 0 || STDIN_FILENO < 0)
        return VLC_EINVAL;

    bool have_event = (STDERR_FILENO >= 0);

    if (strcmp(argv[1], vlc_ipc_get_signature()) != 0)
        return VLC_EINVAL;

    libvlc_instance_t *vlc = libvlc_new(0, NULL);
    if (vlc == NULL)
        return VLC_ENOMEM;

    struct vlc_ipc_runner_intf_owner *owner = vlc_object_create(VLC_OBJECT(vlc->p_libvlc_int),
                                                                sizeof(struct vlc_ipc_runner_intf_owner));

    if (owner == NULL) {
        libvlc_release(vlc);
        return VLC_ENOMEM;
    }

    vlc_list_init(&owner->commands);
    vlc_mutex_init(&owner->lock);
    vlc_cond_init(&owner->cond);

    int ret = vlc_ipc_client_init(&owner->intf.client, STDOUT_FILENO, STDIN_FILENO,
                                  STDERR_FILENO, VLC_OBJECT(&owner->intf), have_event? evt_handler: NULL,
                                  have_event? owner : NULL);

    if (ret != VLC_SUCCESS) {
        vlc_object_delete(&owner->intf);
        libvlc_release(vlc);
        return ret;
    }

    libvlc_log_set(vlc, libvlc_ipc_log_cb, &owner->intf.client);

    owner->module = module_need(&owner->intf, "ipc runner intf", argv[2], true);
    if (owner->module == NULL ||
        (have_event &&
         (owner->intf.ops == NULL ||
          owner->intf.ops->event_reader == NULL ||
          owner->intf.ops->command_handler == NULL))) {
        vlc_ipc_client_term(&owner->intf.client);
        vlc_object_delete(&owner->intf);
        libvlc_release(vlc);
        return -ENOENT;
    }

    if (have_event) {
        ret = vlc_ipc_client_start_event_thread(&owner->intf.client);
        if (ret != VLC_SUCCESS) {
            vlc_ipc_client_term(&owner->intf.client);
            module_unneed(&owner->intf, owner->module);
            vlc_object_delete(&owner->intf);
            libvlc_release(vlc);
            return ret;
        }
        owner->run = true;
        ret = vlc_clone(&owner->thread, exec_commands, owner);
        if (ret != 0) {
            vlc_ipc_client_term(&owner->intf.client);
            module_unneed(&owner->intf, owner->module);
            vlc_object_delete(&owner->intf);
            libvlc_release(vlc);
            return ret;
        }

        vlc_join(owner->thread, NULL);
    }

    struct vlc_ipc_runner_cmd *cmd;
    while ((cmd = vlc_list_first_entry_or_null(&owner->commands, struct vlc_ipc_runner_cmd, fifo)) != NULL) {
        vlc_list_remove(&cmd->fifo);
        if (cmd->free != NULL)
            cmd->free(cmd->param);

        free(cmd);
    }

    vlc_ipc_client_term(&owner->intf.client);
    module_unneed(&owner->intf, owner->module);
    vlc_object_delete(&owner->intf);
    libvlc_release(vlc);

    return 0;
}
