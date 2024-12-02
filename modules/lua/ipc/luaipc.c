/*****************************************************************************
 * luaipc.c
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
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_extensions.h>
#include <vlc_fs.h>
#include <vlc_spawn.h>
#include <vlc_charset.h>

#include "runner.h"
#include "server.h"
#include "client.h"
#include "logs.h"
#include "lua_ipc.h"

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define VLC_LUAIPC_SCAN 1
/*****************************************************************************
 * Lua runner interface
 ****************************************************************************/

static int luaipcrunner_DummyRequire(lua_State *L) {
    VLC_UNUSED(L);
    return 0;
}

/* TODO support .vle */
static int luaipcrunner_DoFile(struct vlc_ipc_runner_interface *intf, lua_State *L, const char *filename) {
    char *lname = ToLocaleDup(filename);
    if (lname == NULL)
        return VLC_ENOMEM;

    int ret = luaL_dofile(L, lname);
    free(lname);
    return ret;
}

static int luaipcrunner_SendScanResult(struct vlc_ipc_runner_interface *intf, struct vlc_ipc_lua_descriptor *desc) {
    int ret = vlc_ipc_execute_cmd(&intf->client, VLC_IPC_CATEGORY_CUSTOM, VLC_LUAIPC_SCAN,
                                  vlc_ipc_cmd_send_lua_descriptor, desc, NULL, NULL);
    vlc_ipc_cleanup_lua_descriptor(desc);
    return ret;
}

static int luaipc_GetStrField(lua_State *L, const char *name, char **ret) {
    lua_getfield(L, -1, name);
    if (lua_isstring(L, -1) != 0) {
        *ret = strdup(luaL_checkstring(L, -1));
        lua_pop(L, 1);
        return (*ret != NULL)? VLC_SUCCESS: VLC_ENOMEM;
    }
    lua_pop(L, 1);
    *ret = NULL;
    return VLC_SUCCESS;
}

static int luaipcrunner_Scan(struct vlc_ipc_runner_interface *intf, const char *filename) {
    /* Create our own lua_State with no require */
    lua_State *L = luaL_newstate();
    if (L == NULL)
        return VLC_EGENERIC;

    struct vlc_ipc_lua_descriptor desc = {.filename = strdup(filename)};
    if (desc.filename == NULL)
        return VLC_ENOMEM;

    /* dummy require prevents the use of lua libs*/
    lua_register(L, "require", &luaipcrunner_DummyRequire);

    if (luaipcrunner_DoFile(intf, L, filename) != VLC_SUCCESS) {
        msg_Warn(intf, "Error loading script %s: %s", filename, lua_tostring(L, lua_gettop(L)));
        lua_pop(L, 1);
        goto discard;
    }

    /* Scan script descriptor */
    lua_getglobal(L, "descriptor");

    if (lua_isfunction(L, -1) == 0) {
        msg_Warn(intf, "Error while running script %s, descriptor() not found",
                 filename);
        goto discard;
    }

    if (lua_pcall(L, 0, 1, 0) != 0) {
        msg_Warn(intf, "Error while running script %s in descriptor(): %s",
                  filename, lua_tostring(L, lua_gettop(L)));
        goto discard;
    }

    if (lua_gettop(L) == 0) {
        msg_Err(intf, "Script %s execution failed", filename);
        goto discard;
    }

    if (lua_istable(L, -1) == 0) {
        msg_Warn(intf, "In script %s, descriptor() did not return a table!",
                 filename);
        goto discard;
    }

    /* Get caps */
    lua_getfield(L, -1, "capabilities");
    static const char *caps[] = {
        "menu",
        "trigger",
        "input-listener",
        "meta-listener",
        "playing-listener",
    };

    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            /* Key is at index -2 and value at index -1. Discard key */
            const char *capability = luaL_checkstring(L, -1);
            size_t i;
            /* Find this capability's flag */
            for (i = 0; i < ARRAY_SIZE(caps); i++) {
                if (strcmp(caps[i], capability) == 0) {
                    desc.capabilities |= 1 << i;
                    break;
                }
            }
            if (i == ARRAY_SIZE(caps)) {
                msg_Warn(intf, "Unknown extension capability '%s' in %s",
                         capability, filename);
            }
            /* Removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }
    } else {
        msg_Warn(intf, "The descriptor() of %s lacks a table of capabilities.",
                 filename);
    }
    lua_pop(L, 1);

    /* Get the fields from the extension manifest. */
    if (luaipc_GetStrField(L, "title", &desc.title) != VLC_SUCCESS)
        goto discard;
    if (luaipc_GetStrField(L, "author", &desc.author) != VLC_SUCCESS)
        goto discard;
    if (luaipc_GetStrField(L, "version", &desc.version) != VLC_SUCCESS)
        goto discard;
    if (luaipc_GetStrField(L, "url", &desc.url) != VLC_SUCCESS)
        goto discard;
    if (luaipc_GetStrField(L, "description", &desc.description) != VLC_SUCCESS)
        goto discard;
    if (luaipc_GetStrField(L, "shortdesc", &desc.short_description) != VLC_SUCCESS)
        goto discard;
    if (luaipc_GetStrField(L, "icon", (char**) &desc.icon) != VLC_SUCCESS)
        goto discard;

    desc.icon_len = (desc.icon != NULL)? strlen(desc.icon): 0;
    msg_Dbg(intf, "Script %s has the following capability flags: 0x%x",
            filename, desc.capabilities);

    desc.valid = true;
discard:
    lua_close(L);
    return luaipcrunner_SendScanResult(intf, &desc);
}

static int luaipcrunner_EventReader(struct vlc_ipc_runner_interface *intf, uint32_t command) {
    switch (command) {
    case VLC_LUAIPC_SCAN: {
        struct vlc_ipc_lua_scan scan;
        int ret = vlc_ipc_recv_lua_scan(intf->client.event_fd, &scan);
        if (ret != VLC_SUCCESS)
            return ret;
        ret = vlc_ipc_runner_push_command(intf, command, scan.filename, free);
        if (ret != VLC_SUCCESS)
            return ret;
        break;
    }
    default:
        msg_Err(intf, "Unhandled command %"PRIu32, command);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int luaipcrunner_CommandHandler(struct vlc_ipc_runner_interface *intf, uint32_t command, const void *param) {
    switch (command) {
    case VLC_LUAIPC_SCAN:
        return luaipcrunner_Scan(intf, (const char*) param);
    }
    return VLC_EGENERIC;
}

static const struct vlc_ipc_runner_ops luaipcrunner_ops = {
    .event_reader = luaipcrunner_EventReader,
    .command_handler = luaipcrunner_CommandHandler,
};

static int OpenRunnerIntf(vlc_object_t *obj) {
    struct vlc_ipc_runner_interface *intf =  (struct vlc_ipc_runner_interface *) obj;
    intf->sys = NULL;
    intf->ops = &luaipcrunner_ops;
    return VLC_SUCCESS;
}

static void CloseRunnerIntf(vlc_object_t *obj) {
}


/*****************************************************************************
 * Extension Manager
 ****************************************************************************/
struct extension_sys {
    extensions_manager_t *mgr;
    int recv_pipe[2];
    int send_pipe[2];
    int evt_pipe[2];
    struct vlc_ipc_server *server;
    pid_t runner_pid;
    vlc_mutex_t lock;
    vlc_sem_t sem;
    bool valid;
    int error;
    uint32_t capabilities;
};

static int luaipc_Activate(extensions_manager_t* mgr, extension_t *ext) {
    return VLC_EGENERIC;
}

static int luaipc_Deactivate(extensions_manager_t *mgr, extension_t *ext) {
    return VLC_EGENERIC;
}

static bool luaipc_IsActivated(extensions_manager_t *mgr, extension_t *ext) {
    return false;
}

static bool luaipc_HasMenu(extensions_manager_t *mgr, extension_t *ext) {
    return false;
}

static int luaipc_GetMenu(extensions_manager_t *mgr, extension_t *ext, char ***menu_strings, uint16_t **len) {
    return VLC_EGENERIC;
}

static bool luaipc_TriggerOnly(extensions_manager_t *mgr, extension_t *ext) {
    return false;
}

static int luaipc_Trigger(extensions_manager_t *mgr, extension_t *ext) {
    return VLC_EGENERIC;
}

static int luaipc_TriggerMenu(extensions_manager_t *mgr, extension_t *ext, int idx) {
    return VLC_EGENERIC;
}

static int luaipc_SetInput(extensions_manager_t *mgr, extension_t *ext, input_item_t *item) {
    return VLC_EGENERIC;
}

static int luaipc_PlayingChanged(extensions_manager_t *mgr, extension_t *ext, int state) {
    return VLC_EGENERIC;
}

static int luaipc_MetaChanged(extensions_manager_t *mgr, extension_t *ext) {
    return VLC_EGENERIC;
}

static const struct vlc_extensions_manager_operations luaipc_ops = {
    .activate = luaipc_Activate,
    .deactivate = luaipc_Deactivate,
    .is_activated = luaipc_IsActivated,
    .has_menu = luaipc_HasMenu,
    .get_menu = luaipc_GetMenu,
    .trigger_only = luaipc_TriggerOnly,
    .trigger = luaipc_Trigger,
    .trigger_menu = luaipc_TriggerMenu,
    .set_input = luaipc_SetInput,
    .playing_changed = luaipc_PlayingChanged,
    .meta_changed = luaipc_MetaChanged
};

static int luaipc_ExtensionDialogCallback(vlc_object_t *obj,
                                          char const *var,
                                          vlc_value_t old,
                                          vlc_value_t new,
                                          void *data) {
    /* TODO */
    return VLC_EGENERIC;
}


static void luaipc_DelExtension(extension_t *ext) {
    struct extension_sys *sys = ext->p_sys;

    if (sys != NULL) {
        if (sys->recv_pipe[0] >= 0)
            vlc_close(sys->recv_pipe[0]);
        if (sys->recv_pipe[1] >= 0)
            vlc_close(sys->recv_pipe[1]);
        if (sys->send_pipe[0] >= 0)
            vlc_close(sys->send_pipe[0]);
        if (sys->send_pipe[1] >= 0)
            vlc_close(sys->send_pipe[1]);
        if (sys->evt_pipe[0] >= 0)
            vlc_close(sys->evt_pipe[0]);
        if (sys->evt_pipe[1] >= 0)
            vlc_close(sys->evt_pipe[1]);
        if (sys->server != NULL) {
            if (sys->runner_pid >= 0) {
                vlc_waitpid(sys->runner_pid);
                sys->runner_pid = -1;
            }
            vlc_ipc_server_unregister_logs(sys->server);
            vlc_ipc_server_del(sys->server);
        }
    }

    free(ext->psz_name);
    free(ext->psz_title);
    free(ext->psz_author);
    free(ext->psz_description);
    free(ext->psz_shortdescription);
    free(ext->psz_url);
    free(ext->psz_version);
    free(ext->p_icondata);
    free(sys);
    free(ext);
}

static void CloseExternalExtensionManager(vlc_object_t *obj) {
    extensions_manager_t *mgr = (extensions_manager_t *) obj;
    var_DelCallback(obj, "dialog-event",
                     luaipc_ExtensionDialogCallback, NULL );
    var_Destroy(obj, "dialog-event" );
    extension_t *ext = NULL;
    /* Free extensions' memory */
    ARRAY_FOREACH(ext, mgr->extensions) {
        luaipc_DelExtension(ext);
    }
    ARRAY_RESET(mgr->extensions);
}

static int luaipc_ListLuaDirs(const char *dir_name, char *dirs[3]) {
    size_t i, idx = 0;

    char *dir = config_GetUserDir(VLC_USERDATA_DIR);
    if (dir != NULL) {
        if (asprintf(&dirs[idx], "%s"DIR_SEP"lua"DIR_SEP"%s", dir, dir_name) == -1) {
            free(dir);
            return VLC_ENOMEM;
        }
        free(dir);
        idx++;
    }

    dir = config_GetSysPath(VLC_PKG_LIBEXEC_DIR, NULL);
    if (dir != NULL) {
        if (asprintf(&dirs[idx], "%s"DIR_SEP"lua"DIR_SEP"%s", dir, dir_name) == -1) {
            free(dir);
            if (idx != 0)
                free(dirs[0]);
            return VLC_ENOMEM;
        }
        idx++;
    }

    char *datadir = config_GetSysPath(VLC_PKG_DATA_DIR, NULL);
    if (datadir != NULL &&
        (dir == NULL || strcmp(dir, datadir) != 0)) {
        if (asprintf(&dirs[idx], "%s"DIR_SEP"lua"DIR_SEP"%s", datadir, dir_name) == -1) {
            free(dir);
            free(datadir);
            for (i = 0; i < idx; i++)
                free(dirs[i]);
            return VLC_ENOMEM;
        }
        idx++;
    }

    free(dir);
    free(datadir);

    for (i = idx; i < 3; i++)
        dirs[i] = NULL;

    return VLC_SUCCESS;
}

static int luaipc_ScanSelect(const char* name) {
#if 0
    static const char *lua_exts[] = {".luac", ".lua", ".vle"};
#else
    static const char *lua_exts[] = {".luac", ".lua"};
#endif
    size_t i, len = strlen(name);
    for (i = 0; i < ARRAY_SIZE(lua_exts); i++) {
        const char *ext = lua_exts[i];
        size_t extlen = strlen(ext);
        if (len > extlen && strcmp(&name[len-extlen], ext) == 0)
            return 1;
    }
    return 0;
}

static int luaipc_ScriptBatchExecute(vlc_object_t *obj, const char *dir_name,
                                     int (*func)(vlc_object_t *, const char *, void *),
                                     void *data) {
    char *dirs[3];
    int ret = luaipc_ListLuaDirs(dir_name, dirs);
    if (ret != VLC_SUCCESS)
        return ret;

    size_t i;
    for (i = 0; i < ARRAY_SIZE(dirs) && dirs[i] != NULL; i++) {
        char *dir = dirs[i];
        msg_Dbg(obj, "Looking for lua scrips in %s", dir);

        char **filelist;

        int j, num = vlc_scandir(dir, &filelist, luaipc_ScanSelect, NULL);
        for (j = 0; j < num; j++) {

            char *filepath;
            if (asprintf(&filepath, "%s"DIR_SEP"%s", dir, filelist[j]) == -1) {
                ret = VLC_ENOMEM;
                for (; j < num; j++)
                    free(filelist[j]);

                free(filelist);
                goto error;
            }
            ret = func(obj, filepath, data);

            free(filepath);

            if (ret != VLC_SUCCESS) {
                for (; j < num; j++)
                    free(filelist[j]);

                free(filelist);
                goto error;
            }
            free(filelist[j]);
        }
        if (num > 0)
            free(filelist);

        free(dir);
    }

    return VLC_SUCCESS;

error:
    for (; i < ARRAY_SIZE(dirs) && dirs[i] != NULL; i++)
        free(dirs[i]);
    return ret;
}

static void luaipc_OnServerError(int error, void *data) {
    extension_t *ext = data;
    struct extension_sys *sys = ext->p_sys;
    sys->error = error;
    msg_Err(sys->mgr, "Server reported the error %d. %s is dead", error, ext->psz_name);
    /* TODO stop on another thread */
    vlc_sem_post(&sys->sem); //FIXME
}

static int luaipc_RecvScan(int fd, extension_t *ext) {
    int ret;
    struct extension_sys * sys = ext->p_sys;
    struct vlc_ipc_lua_descriptor desc = {};
    ret = vlc_ipc_recv_lua_descriptor(fd, &desc);
    if (ret != VLC_SUCCESS) {
        vlc_ipc_cleanup_lua_descriptor(&desc);
        sys->valid = false;
        vlc_sem_post(&sys->sem);
        return ret;
    }
    if (desc.filename != NULL && strcmp(desc.filename, ext->psz_name) != 0) {
        free(ext->psz_name);
        ext->psz_name = desc.filename;
    } else {
        free(desc.filename);
    }

    sys->valid = desc.valid;
    sys->capabilities = desc.capabilities;
    ext->psz_title = desc.title;
    ext->psz_author = desc.author;
    ext->psz_version = desc.version;
    ext->psz_url = desc.url;
    ext->psz_description = desc.description;
    ext->psz_shortdescription = desc.short_description;
    ext->p_icondata = desc.icon;
    ext->i_icondata_size = desc.icon_len;

    msg_Info(sys->mgr, "Scan of %s successful. Title:%s - Author:%s - Version: "
             "%s - Url: %s - Description: %s - Short Description: %s",
             ext->psz_name, ext->psz_title, ext->psz_author, ext->psz_version,
             ext->psz_url, ext->psz_description, ext->psz_shortdescription);

    vlc_sem_post(&sys->sem);
    return VLC_SUCCESS;
}

static int luaipc_HandleServerCommand(struct vlc_ipc_server *server, uint32_t command, void *opaque) {
    extension_t *ext = opaque;
    switch (command) {
    case VLC_LUAIPC_SCAN:
        return luaipc_RecvScan(server->recv_fd, ext);
    }
    return VLC_EGENERIC;
}

static int luaipc_StartExtensionRunner(extension_t *ext) {
    struct extension_sys *sys = ext->p_sys;
    extensions_manager_t *mgr = sys->mgr;
    int ret = VLC_EGENERIC;

    vlc_mutex_lock(&sys->lock);
    if (sys->server != NULL) {
        msg_Err(mgr, "A server is already running for this extension");
        goto out;
    }

    sys->server = vlc_ipc_server_new(sys->recv_pipe[0], sys->send_pipe[1], VLC_OBJECT(mgr));
    if(sys->server == NULL) {
        goto out;
    }

    ret = vlc_ipc_server_register_logs(sys->server);
    if (ret != VLC_SUCCESS) {
        vlc_ipc_server_del(sys->server);
        sys->server = NULL;
    }

    sys->server->handlers[VLC_IPC_CATEGORY_CUSTOM].handle_command = luaipc_HandleServerCommand;
    sys->server->handlers[VLC_IPC_CATEGORY_CUSTOM].opaque = ext;

    /* Start server */
    if (vlc_ipc_server_start(sys->server, luaipc_OnServerError, ext) != VLC_SUCCESS) {
        msg_Err(mgr, "Failed to start IPC server");
        vlc_ipc_server_unregister_logs(sys->server);
        vlc_ipc_server_del(sys->server);
        sys->server = NULL;
        goto out;
    }

    /* Spawn runner */
    sys->runner_pid = vlc_ipc_spawn_runner(sys->recv_pipe[1], sys->send_pipe[0], sys->evt_pipe[0], "luaipcrunner");
    if (sys->runner_pid == -1) {
        msg_Err(mgr, "Cannot execute lua runner");
        vlc_ipc_server_unregister_logs(sys->server);
        vlc_ipc_server_del(sys->server);
        sys->server = NULL;
        goto out;
    }
    vlc_close(sys->recv_pipe[1]);
    sys->recv_pipe[1] = -1;
    vlc_close(sys->send_pipe[0]);
    sys->send_pipe[0] = -1;
    vlc_close(sys->evt_pipe[0]);
    sys->evt_pipe[0] = -1;

    ret = VLC_SUCCESS;
out:
    vlc_mutex_unlock(&sys->lock);
    return ret;
}

static int luaipc_Scan(vlc_object_t *obj, const char *filename,
                       void *data) {
    VLC_UNUSED(data);
    bool try_next = false;
    struct vlc_ipc_lua_scan scan;
    extensions_manager_t *mgr = (extensions_manager_t *) obj;
    extension_t *ext = NULL;
    struct extension_sys *sys = NULL;

    msg_Dbg(mgr, "Scanning %s", filename);

    char *luascript = NULL;
    char *file_extension = strrchr(filename, '.');
#if 0
    /* Experimental: read .vle packages (Zip archives) */
    if (strcmp(file_extension, ".vle") == 0) {
        msg_Dbg(mgr, "Reading Lua script from a zip archive" );
        if (asprintf(&luascript, "zip://%s!/script.lua", filename) == -1)
            goto error;
    } else {
        luascript = strdup(filename);
        if (luascript == NULL)
            goto error;
    }
#else
    luascript = strdup(filename);
    if (luascript == NULL)
        goto error;
#endif

    sys = malloc(sizeof(struct extension_sys));
    if (sys == NULL) {
        goto error;
    }
    sys->recv_pipe[0] = sys->recv_pipe[1] = -1;
    sys->send_pipe[0] = sys->send_pipe[1] = -1;
    sys->evt_pipe[0] = sys->evt_pipe[1] = -1;
    sys->server = NULL;
    sys->runner_pid = -1;
    sys->mgr = mgr;
    vlc_mutex_init(&sys->lock);
    if (vlc_pipe(sys->recv_pipe) != 0 ||
        vlc_pipe(sys->send_pipe) != 0 ||
        vlc_pipe(sys->evt_pipe) != 0) {
        msg_Err(obj, "Failed to create IPC pipes");
        goto error;
    }

    vlc_sem_init(&sys->sem, 0);

    ext = calloc(1, sizeof(extension_t));
    if (ext == NULL)
        goto error;

    ext->p_sys = sys;
    ext->logger = vlc_object_logger(mgr);
    ext->psz_name = luascript;
    scan.filename = luascript;

    ARRAY_APPEND(mgr->extensions, ext);

    if (luaipc_StartExtensionRunner(ext)) {
        goto error;
    }

    if (vlc_ipc_server_send_event(sys->evt_pipe[1], VLC_LUAIPC_SCAN,
                                  (vlc_ipc_server_send_event_param) vlc_ipc_send_lua_scan,
                                  &scan)) {
        msg_Err(mgr, "Failed to send scan request event");
        try_next = true;
        goto error;
    }

    vlc_sem_wait(&sys->sem);

    return VLC_SUCCESS;
error:
    if (sys != NULL) {
        if (sys->recv_pipe[0] >= 0)
            vlc_close(sys->recv_pipe[0]);
        if (sys->recv_pipe[1] >= 0)
            vlc_close(sys->recv_pipe[1]);
        if (sys->send_pipe[0] >= 0)
            vlc_close(sys->send_pipe[0]);
        if (sys->send_pipe[1] >= 0)
            vlc_close(sys->send_pipe[1]);
        if (sys->evt_pipe[0] >= 0)
            vlc_close(sys->evt_pipe[0]);
        if (sys->evt_pipe[1] >= 0)
            vlc_close(sys->evt_pipe[1]);
        if (sys->server != NULL) {
            vlc_waitpid(sys->runner_pid);
            sys->runner_pid = -1;
            vlc_ipc_server_unregister_logs(sys->server);
            vlc_ipc_server_del(sys->server);
            sys->server = NULL;
        }
        free(sys);
        if (ext != NULL)
            ext->p_sys = NULL;
    }
    return try_next?VLC_SUCCESS: VLC_EGENERIC;
}

static int OpenExternalExtensionManager(vlc_object_t *obj) {
    bool enabled = var_InheritBool(obj, "lua");
    if (enabled == false)
        return VLC_EGENERIC;

    msg_Dbg(obj, "Opening Lua IPC extension manager module");
    extensions_manager_t *mgr = (extensions_manager_t *) obj;

    mgr->ops = &luaipc_ops;
    mgr->pf_control = NULL;

    mgr->p_sys = NULL;
    ARRAY_INIT(mgr->extensions);
    vlc_mutex_init(&mgr->lock);

    int ret = luaipc_ScriptBatchExecute(obj, "extensions", luaipc_Scan, NULL);
    if (ret != VLC_SUCCESS) {
        CloseExternalExtensionManager(obj);
        return VLC_EGENERIC;
    }

    var_Create(mgr, "dialog-event", VLC_VAR_ADDRESS);
    var_AddCallback(mgr, "dialog-event", luaipc_ExtensionDialogCallback, NULL);

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("Lua IPC")
    set_description(N_("Lua IPC interpreter"))
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    set_capability("ipc runner intf", 0)
    set_callbacks(OpenRunnerIntf, CloseRunnerIntf)
    add_shortcut("luaipcrunner")

    add_submodule()
        set_shortname(N_("Lua IPC Extension") )
        set_description(N_("Lua IPC Extension"))
        add_shortcut("luaipcextension")
        set_capability("extension", 100)
        set_callbacks(OpenExternalExtensionManager, CloseExternalExtensionManager)
vlc_module_end()
