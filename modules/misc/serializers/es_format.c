/*****************************************************************************
 * es_format.c: serializer and deserializer for es_format_t
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
#include <vlc_serializer.h>
#include <vlc_memstream.h>
#include <vlc_es.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int serialize_es_format(type_serializer_t *serializer);
int deserialize_es_format(type_serializer_t *serializer);

int serialize_es_format(type_serializer_t *serializer)
{
    es_format_t *es = (es_format_t *)serializer->type;
    struct vlc_memstream *serialized_format = serializer->serialized_type;

    vlc_memstream_write(serialized_format, &es->i_cat, sizeof(es->i_cat));
    vlc_memstream_write(serialized_format, &es->i_codec, sizeof(es->i_codec));
    vlc_memstream_write(serialized_format, &es->i_original_fourcc, sizeof(es->i_original_fourcc));
    vlc_memstream_write(serialized_format, &es->i_id, sizeof(es->i_id));
    vlc_memstream_write(serialized_format, &es->i_group, sizeof(es->i_group));
    vlc_memstream_write(serialized_format, &es->i_priority, sizeof(es->i_priority));

    {
        unsigned int language_length = 0;
        if (!es->psz_language) {
            vlc_memstream_write(serialized_format, &language_length, sizeof(language_length));
        } else {
            language_length = strlen(es->psz_language) + 1;

            vlc_memstream_write(serialized_format, &language_length, sizeof(language_length));
            vlc_memstream_write(serialized_format, &es->psz_language, language_length);
        }
    }

    {
        unsigned int description_length = 0;
        if (!es->psz_description) {
            vlc_memstream_write(serialized_format, &description_length, sizeof(description_length));
        } else {
            description_length = strlen(es->psz_language) + 1;

            vlc_memstream_write(serialized_format, &description_length, sizeof(description_length));
            vlc_memstream_write(serialized_format, &es->psz_description, description_length);
        }
    }

    vlc_memstream_write(serialized_format, &es->i_extra_languages, sizeof(es->i_extra_languages));
    for (unsigned j = 0; j < es->i_extra_languages; ++j) {
        const size_t language_len = strlen(es->p_extra_languages[j].psz_language) + 1;
        vlc_memstream_write(serialized_format, &language_len, sizeof(language_len));

        char *const language = es->p_extra_languages[j].psz_language;
        vlc_memstream_write(serialized_format, language, language_len);

        const size_t desc_len = strlen(es->p_extra_languages[j].psz_description) + 1;
        vlc_memstream_write(serialized_format, &desc_len, sizeof(desc_len));

        char *const description = es->p_extra_languages[j].psz_description;
        vlc_memstream_write(serialized_format, description, desc_len);
    }

    switch (es->i_cat) {
        case UNKNOWN_ES:
            break;
        case VIDEO_ES:
            vlc_memstream_write(serialized_format, &es->video, sizeof(es->video));
            break;

        case AUDIO_ES:
            vlc_memstream_write(serialized_format, &es->audio, sizeof(es->audio));
            vlc_memstream_write(serialized_format, &es->audio_replay_gain, sizeof(es->audio_replay_gain));
            break;

        case SPU_ES:
        case DATA_ES:
        default:
            break;
    }

    vlc_memstream_write(serialized_format, &es->i_bitrate, sizeof(es->i_bitrate));
    vlc_memstream_write(serialized_format, &es->i_profile, sizeof(es->i_profile));
    vlc_memstream_write(serialized_format, &es->i_level, sizeof(es->i_level));
    vlc_memstream_write(serialized_format, &es->b_packetized, sizeof(es->b_packetized));
    vlc_memstream_write(serialized_format, &es->i_extra, sizeof(es->i_extra));
    if (es->i_extra)
        vlc_memstream_write(serialized_format, es->p_extra, es->i_extra);

    return VLC_SUCCESS;
}

#define BOUNDARY_CHECK(offset)                               \
    do {                                                     \
        if (cur_pos + offset >= (serialized_format + size))  \
            goto error;                                      \
    } while (0);

int deserialize_es_format(type_serializer_t *serializer)
{
    const char *serialized_format = serializer->serialized;
    const char *cur_pos = serialized_format;
    es_format_t *es = (es_format_t *)serializer->type;
    const unsigned int size = serializer->buffer_size;

    BOUNDARY_CHECK(sizeof(enum es_format_category_e));
    memcpy(&es->i_cat, cur_pos, sizeof(es->i_cat));
    cur_pos += sizeof(es->i_cat);

    BOUNDARY_CHECK(sizeof(vlc_fourcc_t));
    memcpy(&es->i_codec, cur_pos, sizeof(es->i_codec));
    cur_pos += sizeof(es->i_codec);

    BOUNDARY_CHECK(sizeof(vlc_fourcc_t));
    memcpy(&es->i_original_fourcc, cur_pos, sizeof(es->i_original_fourcc));
    cur_pos += sizeof(es->i_original_fourcc);

    BOUNDARY_CHECK(sizeof(int));
    memcpy(&es->i_id, cur_pos, sizeof(es->i_id));
    cur_pos += sizeof(es->i_id);

    BOUNDARY_CHECK(sizeof(int));
    memcpy(&es->i_group, cur_pos, sizeof(es->i_group));
    cur_pos += sizeof(es->i_group);

    BOUNDARY_CHECK(sizeof(int));
    memcpy(&es->i_priority, cur_pos, sizeof(es->i_priority));
    cur_pos += sizeof(es->i_priority);

    {
        BOUNDARY_CHECK(sizeof(unsigned int));
        unsigned int language_length;
        memcpy(&language_length, cur_pos, sizeof(language_length));
        cur_pos += sizeof(language_length);

        char *psz_language = NULL;
        BOUNDARY_CHECK(language_length);
        if (language_length) {
            psz_language = strndup(cur_pos, language_length);
            if (unlikely(!psz_language))
                goto error;
        }

        es->psz_language = psz_language;
        cur_pos += language_length;
    }

    {
        BOUNDARY_CHECK(sizeof(unsigned int));
        unsigned int description_length;
        memcpy(&description_length, cur_pos, sizeof(description_length));
        cur_pos += sizeof(description_length);
        char *psz_description = NULL;

        BOUNDARY_CHECK(description_length);
        if (description_length) {
            psz_description = strndup(cur_pos, description_length);
            if (unlikely(!psz_description))
                goto error;
        }

        es->psz_description = psz_description;
        cur_pos += description_length;
    }

    BOUNDARY_CHECK(sizeof(unsigned int));
    memcpy(&es->i_extra_languages, cur_pos, sizeof(es->i_extra_languages));
    cur_pos += sizeof(es->i_extra_languages);

    es->p_extra_languages = 0;
    if (es->i_extra_languages) {
        es->p_extra_languages = vlc_alloc(es->i_extra_languages, sizeof(*es->p_extra_languages));
        if (unlikely(!es->p_extra_languages))
            goto error;

        for (unsigned i = 0; i < es->i_extra_languages; ++i) {
            BOUNDARY_CHECK(sizeof(size_t));
            size_t language_len;
            memcpy(&language_len, cur_pos, sizeof(language_len));
            cur_pos += sizeof(size_t);

            BOUNDARY_CHECK(sizeof(language_len));
            es->p_extra_languages[i].psz_language = strndup(cur_pos, language_len);
            if (unlikely(!es->p_extra_languages[i].psz_language))
                goto error;
            cur_pos += language_len;

            BOUNDARY_CHECK(sizeof(size_t));
            size_t desc_len;
            memcpy(&desc_len, cur_pos, sizeof(desc_len));
            cur_pos += sizeof(size_t);

            BOUNDARY_CHECK(sizeof(desc_len));
            es->p_extra_languages[i].psz_description = strndup(cur_pos, desc_len);
            if (unlikely(!es->p_extra_languages[i].psz_description))
                goto error;

            cur_pos += desc_len;
        }
    }

    switch (es->i_cat) {
        case VIDEO_ES:
            BOUNDARY_CHECK(sizeof(video_format_t));
            memcpy(&es->video, cur_pos, sizeof(es->video));
            cur_pos += sizeof(es->video);
            break;

        case AUDIO_ES:
            BOUNDARY_CHECK(sizeof(audio_format_t));
            memcpy(&es->audio, cur_pos, sizeof(es->audio));
            cur_pos += sizeof(es->audio);

            BOUNDARY_CHECK(sizeof(audio_replay_gain_t));
            memcpy(&es->audio_replay_gain, cur_pos, sizeof(es->audio_replay_gain));
            cur_pos += sizeof(es->audio_replay_gain);
            break;

        case SPU_ES:
        case DATA_ES:
        default:
            break;
    }

    BOUNDARY_CHECK(sizeof(unsigned int));
    memcpy(&es->i_bitrate, cur_pos, sizeof(es->i_bitrate));
    cur_pos += sizeof(es->i_bitrate);

    BOUNDARY_CHECK(sizeof(int));
    memcpy(&es->i_profile, cur_pos, sizeof(es->i_profile));
    cur_pos += sizeof(es->i_profile);

    BOUNDARY_CHECK(sizeof(int));
    memcpy(&es->i_level, cur_pos, sizeof(es->i_level));
    cur_pos += sizeof(es->i_level);

    BOUNDARY_CHECK(sizeof(bool));
    memcpy(&es->b_packetized, cur_pos, sizeof(es->b_packetized));
    cur_pos += sizeof(es->b_packetized);

    BOUNDARY_CHECK(sizeof(int));
    memcpy(&es->i_extra, cur_pos, sizeof(es->i_extra));
    cur_pos += sizeof(es->i_extra);

    es->p_extra = NULL;
    if (es->i_extra) {
        BOUNDARY_CHECK(sizeof(es->i_extra));
        es->p_extra = malloc(es->i_extra);
        if (unlikely(!es->p_extra))
            goto error;

        memcpy(es->p_extra, cur_pos, es->i_extra);
    }

    return VLC_SUCCESS;

error:
    es_format_Clean(es);
    return VLC_ENOMEM;
}

#undef BOUNDARY_CHECK
