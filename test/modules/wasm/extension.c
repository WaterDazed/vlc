/*****************************************************************************
 * extension.c: test for the wasm extension module
 *****************************************************************************
 * Copyright (C) 2024 Videolabs
 *
 * Authors: Vikram Kangotra <vikramkangotra8055@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define MODULE_NAME test_wasm_extension
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_interface.h>
#include <vlc_extensions.h>

#include "../lib/libvlc_internal.h"

const char vlc_module_name[] = MODULE_STRING;

static int exitcode = 0;

static int OnWasmEventTriggered(vlc_object_t *obj, const char *name,
        vlc_value_t oldv, vlc_value_t newv, void *opaque)
{
    (void)obj; (void)name; (void)oldv; (void)newv;
    vlc_sem_t *sem = opaque;
    vlc_sem_post(sem);
    return VLC_SUCCESS;
}

static int OpenIntf(vlc_object_t *root)
{
    vlc_object_t *libvlc = (vlc_object_t*) vlc_object_instance(root);

    var_Create(libvlc, "test-wasm-activate", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
    var_Create(libvlc, "test-wasm-deactivate", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);

    intf_thread_t *intf = (intf_thread_t*) root;
    extensions_manager_t *mgr = 
        vlc_object_create(root, sizeof(*mgr));
    assert(mgr);

    setenv("VLC_USERDATA_PATH", TOP_SRCDIR "/test/modules/", 1);

    mgr->p_module = module_need(mgr, "extension", "wasm-rs", true);

    if (mgr->p_module == NULL)
    {
        exitcode = 77;
        goto end;
    }

    vlc_sem_t sem_activate, sem_deactivate;
    vlc_sem_init(&sem_activate, 0);
    vlc_sem_init(&sem_deactivate, 0);

    var_AddCallback(libvlc, "test-wasm-activate", OnWasmEventTriggered, &sem_activate);
    var_AddCallback(libvlc, "test-wasm-deactivate", OnWasmEventTriggered, &sem_deactivate);

    assert(mgr->extensions.i_size == 1);
    extension_Activate(mgr, mgr->extensions.p_elems[0]);
    vlc_sem_wait(&sem_activate);
    
    extension_Deactivate(mgr, mgr->extensions.p_elems[0]);
    vlc_sem_wait(&sem_deactivate);
    
    var_DelCallback(libvlc, "test-wasm-activate", OnWasmEventTriggered, &sem_activate);
    var_DelCallback(libvlc, "test-wasm-deactivate", OnWasmEventTriggered, &sem_deactivate);

    module_unneed(mgr, mgr->p_module);
end:
    vlc_object_delete(mgr);
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

int main(void)
{
    test_init();

    const char * const args[] = {
        "-vvv"
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_InternalAddIntf(vlc->p_libvlc_int, MODULE_STRING);
    libvlc_InternalPlay(vlc->p_libvlc_int);

    libvlc_release(vlc);
    return 0;
}
