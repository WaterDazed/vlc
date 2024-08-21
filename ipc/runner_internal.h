/*****************************************************************************
 * runner_internal.h
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
#ifndef VLC_IPC_RUNNER_INTERNAL_H
#define VLC_IPC_RUNNER_INTERNAL_H

#include <vlc_list.h>
#include <vlc_threads.h>
#include <vlc_modules.h>

#include "runner.h"

struct vlc_ipc_runner_cmd {
    uint32_t command;
    void *param;
    void (*free)(void *param);
    struct vlc_list fifo;
};

struct vlc_ipc_runner_intf_owner {
    struct vlc_ipc_runner_interface intf;
    vlc_mutex_t lock;
    vlc_cond_t cond;
    struct vlc_list commands;
    vlc_thread_t thread;
    bool run;
    module_t *module;
};

#endif /* VLC_IPC_RUNNER_INTERNAL_H */
