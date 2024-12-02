/*****************************************************************************
 * server.c
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
# include "config.h"
#endif

#include <assert.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_poll.h>
#include <vlc_interrupt.h>
#include "ipc.h"
#include "server.h"

static void *process(void *data) {
    struct vlc_ipc_server *server = data;
    vlc_thread_set_name("ipc_srv_cmd");

    int notify_error = 0;

    vlc_mutex_lock(&server->lock);
    while (server->running) {

        struct pollfd pfds[2] = {
            { .fd = server->recv_fd, .events = POLLIN },
            { .fd = server->interrupt_fd[0], .events = POLLIN }
        };

        vlc_mutex_unlock(&server->lock);

        int ret = vlc_poll_i11e(pfds, 2, -1);

        vlc_mutex_lock(&server->lock);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                notify_error = errno;
                break;
            }
        } else if (ret > 0) {
            if (pfds[1].revents & POLLIN) {
                /* wait was interrupted */
                uint8_t rd;
                notify_error = (read(server->interrupt_fd[0], &rd, 1) != 1)? EIO: 0;
                break;
            }

            if (pfds[0].revents & (POLLERR|POLLHUP)) {
                notify_error = EIO;
                break;
            } else if (pfds[0].revents & POLLIN) {
                uint32_t category, command;

                if (vlc_ipc_recv_uint32(server->recv_fd, &category) < 0) {
                    notify_error = EIO;
                    break;
                }

                if (category >= VLC_IPC_CATEGORY_MAX) {
                    /* Unexpected category breaking */
                    notify_error = EINVAL;
                    break;
                }
                if (vlc_ipc_recv_uint32(server->recv_fd, &command) < 0) {
                    notify_error = EIO;
                    break;
                }

                if (server->handlers[category].handle_command == NULL) {
                    /* No handler for this command */
                    notify_error = ENOENT;
                    break;
                }

                ret = server->handlers[category].handle_command(server, command,
                                                                server->handlers[category].opaque);
                if (ret != 0) {
                    notify_error = ret;
                    break;
                }
            }
        }
    }

    if (notify_error && server->on_error) {
        /* Report error so the server owner can terminate the client process */
        server->on_error(notify_error, server->error_opaque);
    }
    vlc_mutex_unlock(&server->lock);
    return NULL;
}

struct vlc_ipc_server *vlc_ipc_server_new(int recv_fd, int send_fd, vlc_object_t *obj) {

    if (recv_fd < 0 || send_fd < 0)
        return NULL;

    struct vlc_ipc_server *srv = calloc(1, sizeof(struct vlc_ipc_server));
    if (srv == NULL)
        return NULL;

    if (vlc_pipe(srv->interrupt_fd) == -1) {
        free(srv);
        return NULL;
    }

    srv->send_fd = send_fd;
    srv->recv_fd = recv_fd;
    srv->obj_this = obj;

    srv->running = false;
    vlc_mutex_init(&srv->lock);
    return srv;
}

int vlc_ipc_server_start(struct vlc_ipc_server *srv,
                            vlc_ipc_server_on_error on_error, void *opaque) {
    vlc_mutex_lock(&srv->lock);
    assert(srv->running == false);
    srv->running = true;
    srv->on_error = on_error;
    srv->error_opaque = opaque;
    if (vlc_clone(&srv->processing_thread, process, srv) != 0) {
        msg_Err(srv->obj_this, "Failed to start processing thread");
        srv->running = false;
        srv->on_error = NULL;
        srv->error_opaque = NULL;
        vlc_mutex_unlock(&srv->lock);
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock(&srv->lock);
    return VLC_SUCCESS;
}

void vlc_ipc_server_stop(struct vlc_ipc_server *srv) {
    vlc_mutex_lock(&srv->lock);
    if (srv->running == false) {
        vlc_mutex_unlock(&srv->lock);
        return;
    }
    srv->on_error = NULL;
    srv->error_opaque = NULL;
    srv->running = false;
    struct iovec interrupt = {.iov_base = &(char){1}, .iov_len = 1};
    if (vlc_ipc_send_data(srv->interrupt_fd[1], &interrupt, 1) != VLC_SUCCESS) {
        msg_Err(srv->obj_this, "Something went horribly wrong...");
    }
    vlc_mutex_unlock(&srv->lock);
    vlc_join(srv->processing_thread, NULL);
}

void vlc_ipc_server_del(struct vlc_ipc_server *srv) {
    vlc_ipc_server_stop(srv);
    close(srv->interrupt_fd[0]);
    close(srv->interrupt_fd[1]);
    free(srv);
}
