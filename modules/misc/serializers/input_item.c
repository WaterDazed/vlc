/*****************************************************************************
 * input_item.c: serializer and deserializer for input_item_t
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
#include <vlc_memstream.h>
#include <vlc_serializer.h>
#include <vlc_input_item.h>
#include <vlc_meta.h>
#include <vlc_es.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int serialize_input_item(type_serializer_t *serializer);
int deserialize_input_item(type_serializer_t *serializer);

int serialize_input_item(type_serializer_t *serializer)
{
    input_item_t *item = (input_item_t *)serializer->type;
    struct vlc_memstream *serialized_item = serializer->serialized_type;

    /* psz_uri */
    if (item->psz_uri)
    {
        const size_t uri_len = strlen(item->psz_uri) + 1;
        vlc_memstream_write(serialized_item, &uri_len, sizeof(uri_len));
        vlc_memstream_write(serialized_item, item->psz_uri, uri_len);
    } else {
        const size_t uri_len = 0;
        vlc_memstream_write(serialized_item, &uri_len, sizeof(uri_len));
    }

    /* psz_name */
    if (item->psz_name)
    {
        const size_t name_len = strlen(item->psz_name) + 1;
        vlc_memstream_write(serialized_item, &name_len, sizeof(name_len));
        vlc_memstream_write(serialized_item, item->psz_name, name_len);
    } else {
        const size_t name_len = 0;
        vlc_memstream_write(serialized_item, &name_len, sizeof(name_len));
    }

    /* i_options */
    {
        vlc_memstream_write(serialized_item, &item->i_options, sizeof(item->i_options));
        for (int i = 0; i < item->i_options; ++i) {
            size_t opt_len = strlen(item->ppsz_options[i]) + 1;
            vlc_memstream_write(serialized_item, &opt_len, sizeof(opt_len));
            vlc_memstream_write(serialized_item, item->ppsz_options[i], opt_len);
        }
    }

    /* i_duration */
    {
        vlc_tick_t duration = input_item_GetDuration(item);
        vlc_memstream_write(serialized_item, &duration, sizeof(duration));
    }

    /* es_format_t */
    vlc_memstream_write(serialized_item, &item->i_es, sizeof(item->i_es));

    if (item->i_es) {
        /* load es_format_t serializer */
        type_serializer_t *serialize_es = NULL;
        serialize_es = vlc_object_create( serializer, sizeof(*serialize_es) );
        if (unlikely(!serialize_es)) {
            goto error;
        }

        module_t *module = module_need(serialize_es, "serialize type", "serialize-es", true);
        if (!module) {
            msg_Err(serialize_es, "Could not find serialize-es module");
            goto error;
        }

        /* serialize es_format_t */
        for (int i = 0; i < item->i_es; ++i)  {
            if (vlc_memstream_flush(serialized_item))
                goto error;

            unsigned int *format_size = (unsigned int *)(serialized_item->ptr + serialized_item->length);
            unsigned int old_stream_length = serialized_item->length;
            vlc_memstream_write(serialized_item, "format size", sizeof(unsigned int));
            serialize_es->serialized_type = serialized_item;
            serialize_es->type = item->es[i];
            if (serialize_es->pf_serialize(serialize_es)) {
                msg_Err(serialize_es, "Could not serialize es_format");
                goto error;
            }

            if (vlc_memstream_flush(serialized_item)) {
                msg_Err(serialize_es, "Cannot flush memstream");
                goto error;
            }

            *format_size = serialized_item->length - old_stream_length - sizeof(unsigned int);
        }

        module_unneed(serialize_es, module);
    }

    /* p_meta */
    for (vlc_meta_type_t i = vlc_meta_Title; i <= vlc_meta_DiscTotal; ++i) {
        char *psz_meta = input_item_GetMeta(item, i);
        if (psz_meta == NULL) {
            vlc_memstream_write(serialized_item, "\0\0\0\0", 4);
            continue;
        }

        unsigned int len = (unsigned int)strlen(psz_meta) + 1;
        vlc_memstream_write(serialized_item, &len, sizeof(len));

        vlc_memstream_write(serialized_item, psz_meta, len);
        free(psz_meta);
    }
    {
        int i_status = vlc_meta_GetStatus(item->p_meta);
        vlc_memstream_write(serialized_item, &i_status, sizeof(i_status));
    }

    /* b_error_when_reading */
    vlc_memstream_write(serialized_item, &item->b_error_when_reading, sizeof(item->b_error_when_reading));

    /* b_preparse_depth */
    vlc_memstream_write(serialized_item, &item->i_preparse_depth, sizeof(item->i_preparse_depth));

    return VLC_SUCCESS;

error:
    return VLC_ENOMEM;
}


#define BOUNDARY_CHECK(offset)                             \
    do {                                                   \
        if (cur_pos + offset >= (serialized_item + size))  \
            goto error;                                    \
    } while (0);

int deserialize_input_item(type_serializer_t *serializer)
{
    char *serialized_item = serializer->serialized;
    char *cur_pos = serialized_item;
    input_item_t *item = (input_item_t *)serializer->type;
    const unsigned int size = serializer->buffer_size;

    /* get psz_uri*/
    {
        BOUNDARY_CHECK(sizeof(size_t));
        item->psz_uri = NULL;
        size_t len;
        memcpy(&len, cur_pos, sizeof(len));
        cur_pos += sizeof(len);
        if (len) {
            input_item_SetURI(item, cur_pos);
            BOUNDARY_CHECK(len);
            cur_pos += len;
        }
    }

    /* get psz_name*/
    {
        BOUNDARY_CHECK(sizeof(size_t));
        item->psz_name = NULL;
        size_t len;
        memcpy(&len, cur_pos, sizeof(len));
        cur_pos += sizeof(len);
        if (len) {
            input_item_SetName(item, cur_pos);
            BOUNDARY_CHECK(len);
            cur_pos += len;
        }
    }

    /* get ppsz_options */
    {
        BOUNDARY_CHECK(sizeof(int));
        item->i_options = *(int *)cur_pos;
        cur_pos += sizeof(item->i_options);

        for (int i = 0; i < item->i_options; ++i) {
            BOUNDARY_CHECK(sizeof(size_t));
            size_t opt_len;
            memcpy(&opt_len, cur_pos, sizeof(size_t));
            cur_pos += sizeof(opt_len);

            input_item_AddOption(item, cur_pos, 0);
            BOUNDARY_CHECK(opt_len);
            cur_pos += opt_len;
        }
    }

    /* get i_duration */
    {
        BOUNDARY_CHECK(sizeof(vlc_tick_t));
        vlc_tick_t duration = *(vlc_tick_t *)cur_pos;
        input_item_SetDuration(item, duration);
        cur_pos += sizeof(duration);
    }

    /* get es_format_t */
    BOUNDARY_CHECK(sizeof(int));
    memcpy(&item->i_es, cur_pos, sizeof(item->i_es));
    cur_pos += sizeof(item->i_es);

    if (item->i_es) {
        type_serializer_t *deserialize_es = NULL;
        deserialize_es = vlc_object_create( serializer, sizeof(*deserialize_es) );
        if (unlikely(!deserialize_es)) {
            goto error;
        }

        module_t *module = module_need(deserialize_es, "serialize type", "serialize-es", true);
        if (!module) {
            msg_Err(deserialize_es, "Could not find serialize-es module for deserialization");
            goto error;
        }

        item->es = vlc_alloc(item->i_es, sizeof(es_format_t *));
        if (unlikely(!item->es))
            goto error;

        for (int i = 0; i < item->i_es; ++i) {
            BOUNDARY_CHECK(sizeof(unsigned int));
            unsigned int buffer_size;
            memcpy(&buffer_size, cur_pos, sizeof(buffer_size));
            cur_pos += sizeof(unsigned int);
            item->es[i] = malloc(sizeof(es_format_t));
            if (unlikely(!item->es[i]))
                goto error;

            BOUNDARY_CHECK(buffer_size);
            deserialize_es->buffer_size = buffer_size;
            deserialize_es->type = item->es[i];
            deserialize_es->serialized = cur_pos;
            if (deserialize_es->pf_deserialize(deserialize_es))
                goto error;

            cur_pos += buffer_size;
        }

        module_unneed(deserialize_es, module);
    }

    /* get meta */
    for (vlc_meta_type_t i = vlc_meta_Title; i <= vlc_meta_DiscTotal; ++i) {
        BOUNDARY_CHECK(sizeof(unsigned int));
        unsigned int meta_len;
        memcpy(&meta_len, cur_pos, sizeof(meta_len));
        cur_pos += sizeof(meta_len);
        if (!meta_len)
            continue;

        BOUNDARY_CHECK(meta_len);
        input_item_SetMeta(item, i, cur_pos);
        cur_pos += meta_len;
    }

    BOUNDARY_CHECK(sizeof(int));
    vlc_meta_SetStatus(item->p_meta, *(int *)cur_pos);
    cur_pos += sizeof(int);

    /* get b_error_when_reading */
    {
        BOUNDARY_CHECK(sizeof(bool));
        bool reading_error;
        memcpy(&reading_error, cur_pos, sizeof(reading_error));
        item->b_error_when_reading = reading_error;
        cur_pos += sizeof(reading_error);
    }

    /* get b_preparse_depth */
    {
        BOUNDARY_CHECK(sizeof(int));
        int i_preparse_depth;
        memcpy(&i_preparse_depth, cur_pos, sizeof(item->i_preparse_depth));
        item->i_preparse_depth = i_preparse_depth;
        cur_pos += sizeof(i_preparse_depth);
    }

    return VLC_SUCCESS;

error:
    for (int i = 0; i < item->i_es; ++i) {
        es_format_t *const format = item->es[i];
        es_format_Clean(format);
    }
    free(item->es);
    return VLC_ENOMEM;
}

#undef BOUNDARY_CHECK
