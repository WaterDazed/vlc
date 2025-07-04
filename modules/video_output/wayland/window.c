/**
 * @file window.c
 * @brief Wayland window provider module for VLC media player
 */
/*****************************************************************************
 * Copyright (C) 2025 the VideoLAN team
 * 
 * Authors: Abdulrahman Saber <abdsaber000@gmail.com>
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
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <poll.h>
#include <stdarg.h>
#include <assert.h>


#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_poll.h>
#include <vlc_plugin.h>
#include <vlc_actions.h>
#include <vlc_window.h>
#include <vlc_ancillary.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include "viewporter-client-protocol.h"


typedef struct
{
    struct wl_event_queue *queue;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;
    struct wl_subsurface *subsurface;
    struct wl_surface *parent_surface;
    struct wp_viewport *viewport;
    struct wp_viewporter *viewporter;

    vlc_thread_t thread;
    vlc_sem_t configured;
    vlc_mutex_t lock; 
    unsigned width; 
    unsigned height;

    libvlc_video_output_set_window_cb setWindowCb;
    void *opaque;
} vout_window_sys_t;


static void cleanup_wl_display_read(void *data)
{
    struct wl_display *display = data;

    wl_display_cancel_read(display);
}


static void *Thread (void *data)
{
    vlc_thread_set_name("vlc-wayland-evt");

    vlc_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;
    struct wl_display *display = wnd->display.wl;
    struct wl_event_queue *queue = sys->queue;
    struct pollfd ufd[1];

    int canc = vlc_savecancel();
    vlc_cleanup_push(cleanup_wl_display_read, display);

    ufd[0].fd = wl_display_get_fd(display);
    ufd[0].events = POLLIN;

    for (;;)
    {
        int timeout = -1;

        while (wl_display_prepare_read_queue(display, queue) != 0)
            wl_display_dispatch_queue_pending(display, queue);

        wl_display_flush(display);


        vlc_restorecancel(canc);

        int val = poll(ufd, 1, timeout);

        canc = vlc_savecancel();

        wl_display_read_events(display);
        wl_display_dispatch_queue_pending(display, queue);
    }
    vlc_assert_unreachable();
    vlc_cleanup_pop();
}

static void ResizeAck(vlc_window_t *wnd, unsigned width, unsigned height,
                      void *data)
{
    vout_window_sys_t *sys = wnd->sys;
    uint32_t *serial = data;
    VLC_UNUSED(width);
    VLC_UNUSED(height);
}

static void ReportSize(vlc_window_t *wnd, void *data)
{
    vout_window_sys_t *sys = wnd->sys;
    

    wnd->owner.cbs->resized(wnd, sys->width, sys->height, ResizeAck, data);
}


static void Resize(vlc_window_t *wnd, unsigned width, unsigned height)
{
    vout_window_sys_t *sys = wnd->sys;
    vlc_mutex_lock(&sys->lock);
    sys->width = width;
    sys->height = height;
    wp_viewport_set_destination(sys->viewport, sys->width, sys->height);
    wl_surface_commit(wnd->handle.wl);
    wl_surface_commit(sys->parent_surface);
    msg_Info(wnd, "resize to %ux%u", width, height);
    ReportSize(wnd, NULL);
    wl_display_flush(wnd->display.wl);
    vlc_mutex_unlock(&sys->lock);
    vlc_sem_post(&sys->configured);
}


static void registry_global_cb(void* data, struct wl_registry* registry,
                               uint32_t id, const char* iface, uint32_t version)
{
    vlc_window_t* wnd = (vlc_window_t*)data;
    vout_window_sys_t* sys = (vout_window_sys_t*)wnd->sys;

    if (!strcmp(iface, wl_compositor_interface.name))
    {

  

  
        sys->compositor = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, version);
    }
    else if (!strcmp(iface, wl_subcompositor_interface.name))
    {

  
        sys->subcompositor = (struct wl_subcompositor*)wl_registry_bind(registry, id, &wl_subcompositor_interface, version);
    }
    else if (!strcmp(iface, wp_viewporter_interface.name))
    {
        sys->viewporter = (struct wp_viewporter*)wl_registry_bind(registry, id, &wp_viewporter_interface, version);
    }
}

static void registry_global_remove_cb(void* data, struct wl_registry* registry, uint32_t id)
{
    vlc_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;
    
    VLC_UNUSED(id);
    VLC_UNUSED(registry);
}

static const struct wl_registry_listener registry_cbs = {
    registry_global_cb,
    registry_global_remove_cb,
};


static void WindowResize(void *opaque, unsigned width, unsigned height)
{
    vlc_window_t *wnd = opaque;
    vout_window_sys_t *sys = wnd->sys;
    vlc_mutex_lock(&sys->lock);
    bool is_same_dims = sys->width == width && sys->height == height;
    vlc_mutex_unlock(&sys->lock);
    if (is_same_dims)
        return;
    Resize(wnd, width, height);
    vlc_sem_wait(&sys->configured);
    vlc_window_ReportSize(wnd, width, height);
}

static void WindowMouseMoved(void *opaque, int x, int y)
{
    vlc_window_t *wnd = opaque;
    vlc_window_ReportMouseMoved(wnd, x, y);
}

static void WindowMousePress(void *opaque, libvlc_video_output_mouse_button_t button)
{
    vlc_window_t *wnd = opaque;
    vlc_window_ReportMousePressed(wnd, button);
}

static void WindowMouseRelease(void *opaque, libvlc_video_output_mouse_button_t button)
{
    vlc_window_t *wnd = opaque;
    vlc_window_ReportMouseReleased(wnd, button);
}


static int EmEnable(vlc_window_t *wnd, const vlc_window_cfg_t *restrict cfg)
{
    vout_window_sys_t *sys = wnd->sys;

  
    sys->setWindowCb(wnd, WindowResize, 
        WindowMouseMoved, WindowMousePress, WindowMouseRelease, wnd);

  
    Resize(wnd, sys->width, sys->height);
    wl_subsurface_place_above(sys->subsurface, sys->parent_surface);
    wl_subsurface_set_desync(sys->subsurface);
    struct wl_region *region = wl_compositor_create_region(sys->compositor);
    wl_region_add(region, 0, 0, 0, 0);
    wl_surface_set_input_region(wnd->handle.wl, region);
    wl_region_destroy(region);
    wl_surface_commit(wnd->handle.wl);
    wl_display_flush(wnd->display.wl);
    vlc_sem_wait(&sys->configured);
    return VLC_SUCCESS;
}


static void EmClose(vlc_window_t *);

static const struct vlc_window_operations em_ops = {
    .enable = EmEnable,
    .destroy = EmClose
};


static int EmOpen (vlc_window_t *wnd)
{
    
    struct wl_display *display = var_InheritAddress(wnd, "wl-parent-display");
    if (display == NULL)
    {
        msg_Err(wnd, "No Wayland Display.");
        return VLC_EGENERIC;
    }

    struct wl_surface *parent_surface =  var_InheritAddress(wnd, "wl-parent-surface");
    if (parent_surface == NULL)
    {
        msg_Err(wnd, "No Parent surface provided.");
        return VLC_EGENERIC;
    }

    vout_window_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    
    wnd->sys = sys;
    vlc_sem_init(&sys->configured, 0);
    vlc_mutex_init(&sys->lock);
    sys->queue = wl_display_create_queue(display);
    if (sys->queue == NULL)
        goto error;
    
    sys->registry = wl_display_get_registry(display);
    if (sys->registry == NULL)
        goto error;
    sys->viewporter = NULL;
    wl_proxy_set_queue((struct wl_proxy*)sys->registry, sys->queue);
    wl_registry_add_listener(sys->registry, &registry_cbs, wnd);
    wl_display_roundtrip_queue(display, sys->queue);
    if (!sys->compositor || !sys->subcompositor )
        goto error;
    struct wl_surface *surface =
        wl_compositor_create_surface(sys->compositor);
    sys->viewport = wp_viewporter_get_viewport(sys->viewporter, surface);
    sys->subsurface = 
        wl_subcompositor_get_subsurface(sys->subcompositor, surface, parent_surface);
    
    sys->width = 0;
    sys->height = 0;

    sys->parent_surface = parent_surface;
    sys->setWindowCb = var_InheritAddress( wnd, "vout-cb-window-cb" );
    sys->opaque = var_InheritAddress( wnd, "vout-cb-opaque" );
    wnd->type = VLC_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = surface;
    wnd->display.wl = display;
    wnd->ops = &em_ops;

    if (vlc_clone(&sys->thread, Thread, wnd))
        goto error;

    return VLC_SUCCESS;

error:

    if (sys->viewporter != NULL)
        wp_viewporter_destroy(sys->viewporter); 
    if (wnd->handle.wl != NULL)
        wl_surface_destroy(wnd->handle.wl);
    if (sys->compositor != NULL)
        wl_compositor_destroy(sys->compositor);
    if (sys->registry != NULL)
        wl_registry_destroy(sys->registry);
    free(sys);
    return VLC_EGENERIC;
}

static void EmClose (vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    wp_viewporter_destroy(sys->viewporter);
    wl_surface_destroy(wnd->handle.wl);
    wl_compositor_destroy(sys->compositor);
    wl_registry_destroy(sys->registry);
    free(sys);  
}


/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Wayland-Embed"))
    set_description (N_("Embedded window video"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window", 0)
    set_callback(EmOpen)
    add_shortcut ("embed-wayland")
vlc_module_end ()