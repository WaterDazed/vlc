/*****************************************************************************
 * vlc_gyroscope.h: gyroscope support
 *****************************************************************************
 * Copyright (C) 2018-2025 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *          Adrien Maglo <magsoft@videolan.org>
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

#ifndef VLC_GYROSCOPE_H_
#define VLC_GYROSCOPE_H_ 1

#include <vlc_common.h>

/* vlc_viewpoint.h */
struct vlc_viewpoint_t;
struct vlc_gyroscope;

/**
 * Callback structure provided by the gyroscope owner.
 *
 * The implementation of a gyroscope instance will call the functions
 * from this table when its state changes to notify the client, for
 * instance the video output engine.
 */
struct vlc_gyroscope_callbacks {
    /**
     * Signal that the device has been lost, it might prompt the client
     * to re-create another vlc_gyroscope instance.
     *
     * \param opaque the client's userdata pointer
     */ 
    void (*device_lost)(void *opaque);

    /**
     * Signal that a new viewpoint position is available.
     *
     * The signal provides the new viewpoint position from the
     * vlc_gyroscope device, and can be used to mark the current
     * rendered frame as dirty and refresh it from the video output.
     *
     * \param opaque the client's userdata pointer
     * \param viewpoint the gyroscopic position
     */
    void (*notify_update)(void *opaque,
                          const struct vlc_viewpoint_t *viewpoint);
};

/**
 * Operation structure setup by the vlc_gyroscope implementor.
 *
 * An implementation providing vlc_gyroscope features must define the
 * callbacks in this structure when opening.
 */
struct vlc_gyroscope_operations {
    /**
     * Enable or device the vlc_gyroscope device features.
     *
     * This function signals the implementation that it can enable the
     * viewpoint reporting from the device, as well as the screen in the
     * case of an HMD device.
     *
     * \param gyroscope the instance itself
     * \param enabled whether the device reporting is enabled or disabled
     */ 
    void (*enable)(struct vlc_gyroscope *gyroscope, bool enabled);


    void (*get_viewpoint)(struct vlc_gyroscope *gyroscope,
                          struct vlc_viewpoint_t *vp_out);

    /**
     * Destroy the vlc_gyroscope instance.
     *
     * Destroy the internal resources in the implementation. This does not
     * destroy the vlc_gyroscope object itself. On the client side using the
     * gyroscope object, vlc_gyroscope_Delete should be called instead of
     * using this callbacks.
     *
     * \param gyroscope the instance being destroyed
     */
    void (*destroy)(struct vlc_gyroscope *gyroscope);
};

/**
 * Viewpoint supplier structure, providing interactions with HMD devices.
 *
 * The 
 */
struct vlc_gyroscope {
    /* VLC object integration for vlc_variable and logger access. */
    vlc_object_t obj;

    /** Pointer to internal data for implementor */
    void *sys;

    struct {
        void *sys;
        const struct vlc_gyroscope_callbacks *cbs;
    } owner;

    const struct vlc_gyroscope_operations *ops;
};

/**
 * Create and loads an instance of vlc_gyroscope from modules.
 *
 * This creates a new instance of vlc_gyroscope object, and tries to
 * find a module that can provide an implementation for it.
 *
 * The instance must be destroyed through vlc_gyroscope_Delete.
 *
 * \param obj       the VLC object parent for vlc_variable tree access 
 * \param module    the module name or shortcut, or NULL to load any implementation
 * \param cbs       the vlc_gyroscope client callbacks
 * \param owner     the vlc_gyroscope client userdata pointer for callbacks
 * \return          a new isntance of vlc_gyroscope
 *
 * \note The new instance must be released through vlc_gyroscope_Delete.
 */
VLC_API struct vlc_gyroscope *
vlc_gyroscope_New(vlc_object_t *obj,
                  const char *module,
                  const struct vlc_gyroscope_callbacks *cbs,
                  void *owner);

/**
 * Delete an instance previously created with vlc_gyroscope_New.
 *
 * \param gyroscope the gyroscope instance to delete, won't be usable
 *                  after the call
 */
static inline void
vlc_gyroscope_Delete(struct vlc_gyroscope *gyroscope)
{
    if (gyroscope->ops->destroy != NULL)
        gyroscope->ops->destroy(gyroscope);
    free(gyroscope);
}

static inline void
vlc_gyroscope_ReadViewpoint(struct vlc_gyroscope *gyroscope,
                            struct vlc_viewpoint_t *vp_out)
{
    gyroscope->ops->get_viewpoint(gyroscope, vp_out);
}

#endif
