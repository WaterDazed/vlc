/*****************************************************************************
 * vlc_serializer.h
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

#ifndef VLC_SERIALIZER_H
# define VLC_SERIALIZER_H

struct vlc_memstream;

/**
 * Structure received by serializer/deserializer modules.
 *
 * The caller has to fill the structure following:
 *   - serialization:
 *     1. type to serialize.
 *     2. valid vlc_memstream, which will be filled with type content as raw bytes
 *     by the serializer.
 *
 *   - deserialization:
 *     1. type which will initialized from the content of the raw type buffer by
 *        the deserializer.
 *     2. buffer of the type serialized.
 *     3. buffer size
 */
typedef struct type_serializer_t
{
    struct vlc_object_t obj;

    void *type;

    int ( * pf_serialize )( struct type_serializer_t * );
    int ( * pf_deserialize )( struct type_serializer_t * );

    /* serialization */
    struct vlc_memstream *serialized_type;

    /* deserialization */
    char *serialized;
    unsigned int buffer_size;
} type_serializer_t;

#endif
