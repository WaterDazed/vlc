/*****************************************************************************
 * test.c
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

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

#include "client.h"
#include "server.h"
#include "test_ipc.h"

const char vlc_module_name[] = "test_ipc";

struct ipc_test_ctx {
    struct vlc_ipc_server *server;
    struct vlc_ipc_client client;
};

enum cmd {
    TEST_SIMPLE,
    TEST_FIXED,
    TEST_VARIABLE_ARRAY
};

/* Mirror received data */
static int server_handle_command(struct vlc_ipc_server *server, uint32_t command, void *opaque) {
    VLC_UNUSED(opaque);
    int ret;
    switch (command) {
    case TEST_SIMPLE: {
        struct vlc_ipc_test_simple simple;
        ret = vlc_ipc_recv_test_simple(server->recv_fd, &simple);
        assert(ret == VLC_SUCCESS);
        ret = vlc_ipc_send_test_simple(server->send_fd, &simple);
        assert(ret == VLC_SUCCESS);
        vlc_ipc_cleanup_test_simple(&simple);
        break;
    }
    case TEST_FIXED: {
        struct vlc_ipc_test_fixed_arrays fixed_arrays = {};
        ret = vlc_ipc_recv_test_fixed_arrays(server->recv_fd, &fixed_arrays);
        assert(ret == VLC_SUCCESS);
        ret = vlc_ipc_send_test_fixed_arrays(server->send_fd, &fixed_arrays);
        assert(ret == VLC_SUCCESS);
        vlc_ipc_cleanup_test_fixed_arrays(&fixed_arrays);
        break;
    }
    case TEST_VARIABLE_ARRAY: {
        struct vlc_ipc_test_var_arrays var_arrays;
        ret = vlc_ipc_recv_test_var_arrays(server->recv_fd, &var_arrays);
        assert(ret == VLC_SUCCESS);
        ret = vlc_ipc_send_test_var_arrays(server->send_fd, &var_arrays);
        assert(ret == VLC_SUCCESS);
        vlc_ipc_cleanup_test_var_arrays(&var_arrays);
        break;
    }
    default:
        vlc_assert_unreachable();
    }
    return VLC_SUCCESS;
}

static void on_server_error(int error, void *opaque) {
    VLC_UNUSED(error);
    VLC_UNUSED(opaque);
    vlc_assert_unreachable();
}

int main(int argc, char *argv[]) {
    VLC_UNUSED(argc);
    VLC_UNUSED(argv);
    struct ipc_test_ctx ctx = {};

    int srv_cmd[2] = {-1, -1};
    int client_cmd[2] = {-1, -1};

    assert(vlc_pipe(srv_cmd) == 0);
    assert(vlc_pipe(client_cmd) == 0);

    ctx.server = vlc_ipc_server_new(client_cmd[0], srv_cmd[1], NULL);
    assert(ctx.server != NULL);

    ctx.server->handlers[VLC_IPC_CATEGORY_CUSTOM].handle_command = server_handle_command;
    ctx.server->handlers[VLC_IPC_CATEGORY_CUSTOM].opaque = NULL;

    int ret = vlc_ipc_client_init(&ctx.client, srv_cmd[0], client_cmd[1], -1,
                                     NULL, NULL, NULL);

    assert(ret == VLC_SUCCESS);

    ret = vlc_ipc_server_start(ctx.server, &on_server_error, NULL);
    assert(ret == VLC_SUCCESS);

    struct vlc_ipc_test_fixed_arrays rfar, sfar = {
        .i8 = {-1},
        .u8 = {2, 3},
        .i16 = {-4, 5, -6},
        .u16 = {7, 8, 9, 10},
        .i32 = {-11, 12, -13, 14, -15},
        .u32 = {16, 17, 18, 19, 20, 21},
        .i64 = {-22, 23, -24, 25, -26, 27, -28},
        .u64 = {29, 30, 31, 32, 33, 34, 35, 36},
        .fl = {37.1, 38.2, 39.3, 40.4, 41.5, 42.6, 43.7, 44.8, 45.9},
        .db = {46.1, 47.2, 48.3, 49.4, 50.5, 51.6, 52.7, 53.8, 54.9},
        .str = {
            (char *) "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
            (char *) "Curabitur sollicitudin malesuada dictum.",
            (char *) "Praesent porttitor ex ut mauris gravida rutrum.",
            (char *) "Nullam venenatis ipsum vitae tempus pretium.",
            (char *) "Ut ultrices orci in velit consectetur egestas.",
            (char *) "In molestie ipsum eget erat blandit faucibus.",
            (char *) "Vivamus ut ante eget tellus mattis commodo.",
            (char *) "Ut eleifend ipsum leo, non efficitur nunc condimentum et.",
            (char *) "Vivamus tempus neque a laoreet aliquet.",
            (char *) "Fusce cursus odio tempus lorem tempus consectetur.",
            (char *) "Vivamus elementum mattis tincidunt."
        },
        .smp = {
            { .str = (char *) "foo", .b = true },
            { .str = (char *) "bar", .b = false },
            { .str = (char *) NULL, .b = true }
        }
    };

    struct vlc_ipc_test_simple rsimple, ssimple = {
        .i8 = 1,
        .u8 = 2,
        .i16 = 3,
        .u16 = 4,
        .i32 = 5,
        .u32 = 6,
        .i64 = 7,
        .u64 = 8,
        .fl = 9.4212,
        .db = 10.1337,
        .null_string = NULL,
        .buf_len = strlen(sfar.str[0]),
        .buf = sfar.str[0],
        .smsg = { .str = sfar.str[1], .b = true }
    };

    struct vlc_ipc_test_var_arrays rvar, svar = {
        .i8_len = ARRAY_SIZE(sfar.i8),
        .i8 = sfar.i8,
        .u8_len = ARRAY_SIZE(sfar.u8),
        .u8 = sfar.u8,
        .i16_len = ARRAY_SIZE(sfar.i16),
        .i16 = sfar.i16,
        .u16_len = ARRAY_SIZE(sfar.u16),
        .u16 = sfar.u16,
        .i32_len = ARRAY_SIZE(sfar.i32),
        .i32 = sfar.i32,
        .u32_len = ARRAY_SIZE(sfar.u32),
        .u32 = sfar.u32,
        .i64_len = ARRAY_SIZE(sfar.i64),
        .i64 = sfar.i64,
        .u64_len = ARRAY_SIZE(sfar.u64),
        .u64 = sfar.u64,
        .fl_len = ARRAY_SIZE(sfar.fl),
        .fl = sfar.fl,
        .db_len = ARRAY_SIZE(sfar.db),
        .db = sfar.db,
        .str_len = ARRAY_SIZE(sfar.str),
        .str = (char **) sfar.str,
        .smp_len = ARRAY_SIZE(sfar.smp),
        .smp = sfar.smp
    };
    struct vlc_ipc_test_var_arrays empty = {
        .i8_len = 0,
        .i8 = NULL,
        .u8_len = 0,
        .u8 = NULL,
        .i16_len = 0,
        .i16 = NULL,
        .u16_len = 0,
        .u16 = NULL,
        .i32_len = 0,
        .i32 = NULL,
        .u32_len = 0,
        .u32 = NULL,
        .i64_len = 0,
        .i64 = NULL,
        .u64_len = 0,
        .u64 = NULL,
        .fl_len = 0,
        .fl = NULL,
        .db_len = 0,
        .db = NULL,
        .str_len = 0,
        .str = NULL,
        .smp_len = 0,
        .smp = NULL
    };

    ret = vlc_ipc_execute_cmd(&ctx.client, VLC_IPC_CATEGORY_CUSTOM, TEST_SIMPLE,
                                 vlc_ipc_cmd_send_test_simple, &ssimple,
                                 vlc_ipc_cmd_recv_test_simple, &rsimple);
    assert(ret == VLC_SUCCESS);

    assert(ssimple.i8 == rsimple.i8);
    assert(ssimple.u8 == rsimple.u8);
    assert(ssimple.i16 == rsimple.i16);
    assert(ssimple.u16 == rsimple.u16);
    assert(ssimple.i32 == rsimple.i32);
    assert(ssimple.u32 == rsimple.u32);
    assert(ssimple.i64 == rsimple.i64);
    assert(ssimple.u64 == rsimple.u64);
    assert(ssimple.fl == rsimple.fl);
    assert(ssimple.db == rsimple.db);
    assert(ssimple.null_string == rsimple.null_string );
    assert(ssimple.buf_len == rsimple.buf_len );
    assert(memcmp(ssimple.buf, rsimple.buf, ssimple.buf_len) == 0);
    assert(strcmp(ssimple.smsg.str, rsimple.smsg.str) == 0);
    assert(ssimple.smsg.b == rsimple.smsg.b);


    ret = vlc_ipc_execute_cmd(&ctx.client, VLC_IPC_CATEGORY_CUSTOM, TEST_FIXED,
                                 vlc_ipc_cmd_send_test_fixed_arrays, &sfar,
                                 vlc_ipc_cmd_recv_test_fixed_arrays, &rfar);
    assert(ret == VLC_SUCCESS);

    assert(memcmp(sfar.i8, rfar.i8, sizeof(sfar.i8)) == 0);
    assert(memcmp(sfar.u8, rfar.u8, sizeof(sfar.u8)) == 0);
    assert(memcmp(sfar.i16, rfar.i16, sizeof(sfar.i16)) == 0);
    assert(memcmp(sfar.u16, rfar.u16, sizeof(sfar.u16)) == 0);
    assert(memcmp(sfar.i32, rfar.i32, sizeof(sfar.i32)) == 0);
    assert(memcmp(sfar.u32, rfar.u32, sizeof(sfar.u32)) == 0);
    assert(memcmp(sfar.i64, rfar.i64, sizeof(sfar.i64)) == 0);
    assert(memcmp(sfar.u64, rfar.u64, sizeof(sfar.u64)) == 0);
    assert(memcmp(sfar.fl, rfar.fl, sizeof(sfar.fl)) == 0);
    assert(memcmp(sfar.db, rfar.db, sizeof(sfar.db)) == 0);
    size_t i;
    for (i = 0; i < ARRAY_SIZE(sfar.str); i++) {
        assert(strcmp(sfar.str[i], rfar.str[i]) == 0);
    }

    for (i = 0; i < ARRAY_SIZE(sfar.smp); i++) {
        if (sfar.smp[i].str != NULL)
            assert(strcmp(sfar.smp[i].str, rfar.smp[i].str) == 0);
        else
            assert(rfar.smp[i].str == NULL);
        assert(sfar.smp[i].b == rfar.smp[i].b);
    }


    ret = vlc_ipc_execute_cmd(&ctx.client, VLC_IPC_CATEGORY_CUSTOM, TEST_VARIABLE_ARRAY,
                                 vlc_ipc_cmd_send_test_var_arrays, &svar,
                                 vlc_ipc_cmd_recv_test_var_arrays, &rvar);
    assert(ret == VLC_SUCCESS);

    assert(svar.i8_len == rvar.i8_len);
    assert(memcmp(svar.i8, rvar.i8, svar.i8_len * sizeof(*svar.i8)) == 0);

    assert(svar.u8_len == rvar.u8_len);
    assert(memcmp(svar.u8, rvar.u8, svar.u8_len * sizeof(*svar.u8)) == 0);

    assert(svar.i16_len == rvar.i16_len);
    assert(memcmp(svar.i16, rvar.i16, svar.i16_len * sizeof(*svar.i16)) == 0);

    assert(svar.u16_len == rvar.u16_len);
    assert(memcmp(svar.u16, rvar.u16, svar.u16_len * sizeof(*svar.u16)) == 0);

    assert(svar.i32_len == rvar.i32_len);
    assert(memcmp(svar.i32, rvar.i32, svar.i32_len * sizeof(*svar.i32)) == 0);

    assert(svar.u32_len == rvar.u32_len);
    assert(memcmp(svar.u32, rvar.u32, svar.u32_len * sizeof(*svar.u32)) == 0);

    assert(svar.i64_len == rvar.i64_len);
    assert(memcmp(svar.i64, rvar.i64, svar.i64_len * sizeof(*svar.i64)) == 0);

    assert(svar.u64_len == rvar.u64_len);
    assert(memcmp(svar.u64, rvar.u64, svar.u64_len * sizeof(*svar.u64)) == 0);

    assert(svar.fl_len == rvar.fl_len);
    assert(memcmp(svar.fl, rvar.fl, svar.fl_len * sizeof(*svar.fl)) == 0);

    assert(svar.db_len == rvar.db_len);
    assert(memcmp(svar.db, rvar.db, svar.db_len * sizeof(*svar.db)) == 0);

    assert(svar.str_len == rvar.str_len);
    for (i = 0; i < svar.str_len; i++) {
        assert(strcmp(svar.str[i], rvar.str[i]) == 0);
    }

    assert(svar.smp_len == rvar.smp_len);
    for (i = 0; i < svar.smp_len; i++) {
        if (svar.smp[i].str != NULL)
            assert(strcmp(svar.smp[i].str, rvar.smp[i].str) == 0);
        else
            assert(rvar.smp[i].str == NULL);
        assert(svar.smp[i].b == rvar.smp[i].b);
    }


    vlc_ipc_cleanup_test_simple(&rsimple);
    vlc_ipc_cleanup_test_fixed_arrays(&rfar);
    vlc_ipc_cleanup_test_var_arrays(&rvar);
    rvar = svar;
    ret = vlc_ipc_execute_cmd(&ctx.client, VLC_IPC_CATEGORY_CUSTOM, TEST_VARIABLE_ARRAY,
                              vlc_ipc_cmd_send_test_var_arrays, &empty,
                              vlc_ipc_cmd_recv_test_var_arrays, &rvar);
    assert(ret == VLC_SUCCESS);
    assert(empty.i8_len == rvar.i8_len);
    assert(empty.i8 == rvar.i8);
    assert(empty.u8_len == rvar.u8_len);
    assert(empty.u8 == rvar.u8);

    assert(empty.i16_len == rvar.i16_len);
    assert(empty.i16 == rvar.i16);

    assert(empty.u16_len == rvar.u16_len);
    assert(empty.u16 == rvar.u16);

    assert(empty.i32_len == rvar.i32_len);
    assert(empty.i32 == rvar.i32);

    assert(empty.u32_len == rvar.u32_len);
    assert(empty.u32 == rvar.u32);

    assert(empty.i64_len == rvar.i64_len);
    assert(empty.i64 == rvar.i64);

    assert(empty.u64_len == rvar.u64_len);
    assert(empty.u64 == rvar.u64);

    assert(empty.fl_len == rvar.fl_len);
    assert(empty.fl == rvar.fl);

    assert(empty.db_len == rvar.db_len);
    assert(empty.db == rvar.db);

    assert(empty.str_len == rvar.str_len);
    assert(empty.str == rvar.str);

    assert(empty.smp_len == rvar.smp_len);
    assert(empty.smp == rvar.smp);

    vlc_ipc_client_term(&ctx.client);
    vlc_ipc_server_del(ctx.server);

    return 0;
}
