/*****************************************************************************
 * client.c
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
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_interrupt.h>

#include "ipc.h"
#include "client.h"

static void *vlc_ipc_client_event_process(void *data) {
    vlc_thread_set_name("ipc_clt_evt");
    struct vlc_ipc_client *client = data;
    for (;;) {
        struct pollfd pfds[2] = {
            { .fd = client->event_fd, .events = POLLIN },
            { .fd = client->interrupt_fd[0], .events = POLLIN }
        };

        int ret = vlc_poll_i11e(pfds, 2, -1);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        } else if (ret > 0) {
            if (pfds[1].revents & POLLIN) {
                uint8_t rd;
                (void) read(client->interrupt_fd[0], &rd, 1);
                break;
            }
            if (pfds[0].revents & (POLLERR|POLLHUP)) {
                break;
            } else if (pfds[0].revents & POLLIN) {
                if (client->handle_event(client, client->opaque) < 0)
                    break;
            }
        }
    }
    return NULL;
}

/* This function doesn't log anything on purpose to avoid chicken and egg
 * problem with traces */
int vlc_ipc_client_init(struct vlc_ipc_client *client, int recv_fd,
                           int send_fd, int event_fd, vlc_object_t *instance,
                           vlc_ipc_client_evt_handler evt_handler,
                           void* opaque) {
    if (recv_fd < 0 || send_fd < 0 ||
        (event_fd >= 0 && evt_handler == NULL)) {
        return VLC_EINVAL;
    }
    client->recv_fd = recv_fd;
    client->send_fd = send_fd;
    client->event_fd = event_fd;
    client->interrupt_fd[0] = client->interrupt_fd[1] = -1;
    client->abort_on_io_error = true;

    vlc_mutex_init(&client->lock);

    client->instance = instance;
    client->handle_event = evt_handler;
    client->opaque = opaque;

    return VLC_SUCCESS;
}

int vlc_ipc_client_start_event_thread(struct vlc_ipc_client *client) {
    if (client->event_fd < 0 || client->handle_event == NULL || client->interrupt_fd[1] >= 0)
        return VLC_EINVAL;

    int ret = vlc_pipe(client->interrupt_fd);
    if (ret != VLC_SUCCESS)
        return ret;

    ret = vlc_clone(&client->evt_thread, vlc_ipc_client_event_process, client);
    if (ret != VLC_SUCCESS) {
        vlc_close(client->interrupt_fd[0]);
        vlc_close(client->interrupt_fd[1]);
        client->interrupt_fd[0] = client->interrupt_fd[1] = -1;
        return ret;
    }

    return VLC_SUCCESS;
}

int vlc_ipc_client_stop_event_thread(struct vlc_ipc_client *client) {
    if (client->interrupt_fd[1] >= 0) {
        ssize_t wr;
        do {
            uint8_t stop = 1;
            wr = write(client->interrupt_fd[1], &stop, sizeof(stop));
            if (wr < 0 && errno != EINTR)
                return errno;
        } while (wr != 1);

        vlc_join(client->evt_thread, NULL);
        vlc_close(client->interrupt_fd[0]);
        vlc_close(client->interrupt_fd[1]);
        client->interrupt_fd[0] = client->interrupt_fd[1] = -1;
    }
    return VLC_SUCCESS;
}

void vlc_ipc_client_term(struct vlc_ipc_client *client) {
    if (vlc_ipc_client_stop_event_thread(client)) {
        abort();
    }
    client->handle_event = NULL;
}

int vlc_ipc_execute_cmd(struct vlc_ipc_client *client, uint32_t category, uint32_t command,
                           int (*send_data)(struct vlc_ipc_client *client, const void *), const void *command_data,
                           int (*recv_data)(struct vlc_ipc_client *client, void *), void *response_data) {

    /* Locking here ensure that in case of coroutines in the external language
     * thread there will be no data interleaving */
    vlc_mutex_lock(&client->lock);
    if (vlc_ipc_send_uint32(client->send_fd, category) < 0 ||
        vlc_ipc_send_uint32(client->send_fd, command) < 0 ||
        (send_data != NULL && send_data(client, command_data) < 0) ||
        (recv_data != NULL && recv_data(client, response_data) < 0))
        goto abort;
    vlc_mutex_unlock(&client->lock);

    return 0;

abort:
    /* Easy way out if anything is suspicious with the IPC pipes it's easier
     * to abort the whole client process rather than deal with state recovery */
    if (client->abort_on_io_error) {
        vlc_mutex_unlock(&client->lock);
        abort();
    }

    vlc_mutex_unlock(&client->lock);
    return -1;
}

