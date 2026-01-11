/*****************************************************************************
 * vlc_mediainfo.h: definition of the requests structures for the preparse process
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#ifndef VLC_MEDIAINFO_H
#define VLC_MEDIAINFO_H

#include "../lib/libvlc_internal.h"
#include "vlc_serializer.h"
#include <vlc_memstream.h>
#include <vlc_events.h>

/* preparse process
 *
 * request format:
 *       1. request size   : unsigned int
 *       2. mrl length     : unsigned int
 *       3. mrl            : char[]
 *       4. preparse depth : int
 *
 * answers format:
 *    - subitem_tree_added:
 *       1. event type                 : libvlc_event_e
 *       2. size of serialized subitem : unsigned int
 *       3. serialized subitem         : char[]
 *
 *    - media_parse_changed:
 *       1. event type                 : libvlc_event_e
 *       2. number of subitems         : unsigned int
 *       3. size of serialized subitem : unsigned int
 *       4. serialized subitem         : char[]
 *       ...
 */

typedef struct mediainfo_request_t {
    /* request */
    vlc_event_type_t events_to_listen[vlc_InputItemAttachmentsFound + 1];
    unsigned int mrl_size;
    char *mrl;
    int i_preparse_depth;

    /* answer */
    vlc_sem_t sem;
    input_item_t *item;
} mediainfo_request_t;

typedef struct mediainfo_parser_t {
    type_serializer_t *serializer;
} mediainfo_parser_t;

typedef struct mediainfo_internal_t {
    libvlc_instance_t *vlc;
    mediainfo_parser_t parser;
    mediainfo_request_t *current_request;
} mediainfo_internal_t;

#endif
