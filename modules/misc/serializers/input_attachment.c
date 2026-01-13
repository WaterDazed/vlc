/*****************************************************************************
 * input_attachment.c: serializer and deserializer for input_attachment_t
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc/libvlc.h>
#include <vlc_serializer.h>
#include <vlc_memstream.h>
#include <vlc_input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int serialize_attachment     ( type_serializer_t * );
int deserialize_attachment   ( type_serializer_t * );

int serialize_attachment(type_serializer_t *serializer)
{
    input_attachment_t *attach = (input_attachment_t *)serializer->type;
    struct vlc_memstream *serialize_attachment = serializer->serialized_type;

    {
        unsigned int name_length = strlen(attach->psz_name) + 1;
        vlc_memstream_write(serialize_attachment, &name_length, sizeof(name_length));
        vlc_memstream_write(serialize_attachment, attach->psz_name, name_length);
    }

    {
        unsigned int mime_length = strlen(attach->psz_mime) + 1;
        vlc_memstream_write(serialize_attachment, &mime_length, sizeof(mime_length));
        vlc_memstream_write(serialize_attachment, attach->psz_mime, mime_length);
    }

    {
        unsigned int description_length = strlen(attach->psz_description) + 1;
        vlc_memstream_write(serialize_attachment, &description_length, sizeof(description_length));
        vlc_memstream_write(serialize_attachment, attach->psz_description, description_length);
    }

    vlc_memstream_write(serialize_attachment, &attach->i_data, sizeof(attach->i_data));
    vlc_memstream_write(serialize_attachment, attach->p_data, attach->i_data);

    return VLC_SUCCESS;
}

int deserialize_attachment(type_serializer_t *serializer)
{
    const char *serialized_attachment = serializer->serialized;
    const char *cur_pos = serialized_attachment;

    input_attachment_t *attach = (input_attachment_t *)serializer->type;
    {
        unsigned int name_length = *(unsigned int *)cur_pos;
        cur_pos += sizeof(name_length);
        attach->psz_name = strndup(cur_pos, name_length);
        cur_pos += name_length;
    }

    {
        unsigned int mime_length = *(unsigned int *)cur_pos;
        cur_pos += sizeof(mime_length);
        attach->psz_mime = strndup(cur_pos, mime_length);
        cur_pos += mime_length;
    }

    {
        unsigned int description_length = *(unsigned int *)cur_pos;
        cur_pos += sizeof(description_length);
        attach->psz_mime = strndup(cur_pos, description_length);
        cur_pos += description_length;
    }

    attach->i_data = *(int *)cur_pos;
    cur_pos += sizeof(attach->i_data);

    attach->p_data = malloc(attach->i_data);
    if (unlikely(!attach->p_data)) {
        goto error;
    }
    memcpy(attach->p_data, cur_pos, attach->i_data);

    return VLC_SUCCESS;

error:
    return VLC_ENOMEM;
}
