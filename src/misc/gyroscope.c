/*****************************************************************************
 * gyro.c: Gyroscopic feature support
 *****************************************************************************
 * Copyright © 2022 Videolabs
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
#include "config.h"
#endif

#include <vlc_gyroscope.h>
#include <vlc_modules.h>
#include <vlc_viewpoint.h>

#include "../libvlc.h"

struct vlc_gyroscope *
vlc_gyroscope_New(vlc_object_t *parent, const char *name,
                  const struct vlc_gyroscope_callbacks *cbs,
                  void *owner)
{
    module_t **mods;
    size_t strict;
    ssize_t n = vlc_module_match("gyroscope", name, true, &mods, &strict);
 
    msg_Dbg(parent, "looking for %s module matching \"%s\": %zd candidates",
            "gyroscope provider", name, n);

    if (n <= 0)
        return NULL;

    struct vlc_gyroscope *device =
        vlc_custom_create(parent, sizeof *device, "gyro");
    device->owner.cbs = cbs;
    device->owner.sys = owner;
  
    for (size_t i=0; i<(size_t)n; ++i)
    {
        int (*activate)(struct vlc_gyroscope *device)
            = vlc_module_map(vlc_object_logger(parent), mods[i]);
        if (activate == NULL)
            continue;
 
        if ((*activate)(device) != VLC_SUCCESS)
        {
            vlc_objres_clear(&device->obj);
            continue;
        }

        return device;
    }

    vlc_object_delete(&device->obj);
    return NULL;
}

