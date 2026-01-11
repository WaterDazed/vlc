/*****************************************************************************
 * mediainfo.c
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

#include <vlc/vlc.h>
#include <vlc_mediainfo.h>
#include <vlc_fs.h>
#include <vlc_modules.h>
#include <vlc_memstream.h>
#include <vlc_serializer.h>

#include "../lib/media_internal.h"
#include "../lib/picture_internal.h"

static type_serializer_t *init_serializer(vlc_object_t *parent)
{
    type_serializer_t *serializer = vlc_object_create( parent, sizeof(type_serializer_t));
    if (unlikely(!serializer))
        return NULL;

    module_t *module = module_need(serializer, "serialize type", "serialize-item", true);
    if (unlikely(!module)) {
        msg_Err(parent, "cannot find serializer module for input_item_t");
        free(serializer);
        return NULL;
    }

    return serializer;
}

static int serialize_item_node_list(mediainfo_internal_t *mediainfo, libvlc_media_list_t *list,
                                     struct vlc_memstream *answer)
{
    libvlc_int_t *const libvlc = mediainfo->vlc->p_libvlc_int;
    type_serializer_t *serializer = vlc_object_create( libvlc, sizeof(type_serializer_t));
    if (unlikely(!serializer))
        return VLC_ENOMEM;

    module_t *module = module_need(serializer, "serialize type", "serialize-item", true);
    if (unlikely(!module)) {
        msg_Err(libvlc, "cannot find a serializer module for input_item_t");
        free(serializer);
        goto error;
    }

    const int size = libvlc_media_list_count(list);
    for (int i = 0; i < size; ++i) {
        libvlc_media_t *media = libvlc_media_list_item_at_index(list, i);
        input_item_t *item = media->p_input_item;

        if (vlc_memstream_flush(answer))
            return VLC_EGENERIC;

        unsigned int *item_size = (unsigned int *)(answer->ptr + answer->length);
        unsigned int old_stream_length = answer->length;
        vlc_memstream_write(answer, "itemsize", sizeof(unsigned int));

        serializer->type = item;
        serializer->serialized_type = answer;

        /* serialize item in answer */
        if (serializer->pf_serialize(serializer))
            return VLC_EGENERIC;

        if (vlc_memstream_flush(answer))
            return VLC_EGENERIC;

        *item_size = answer->length - old_stream_length - sizeof(unsigned int);
    }

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

static void media_subitemtree_added(const libvlc_event_t *event, void *user_data)
{
    mediainfo_internal_t *mediainfo = (mediainfo_internal_t *)user_data;
    libvlc_int_t *const libvlc = mediainfo->vlc->p_libvlc_int;

    libvlc_media_t *media = event->u.media_subitem_added.new_child;
    struct vlc_memstream answer;
    if (vlc_memstream_open(&answer)) {
        msg_Err(libvlc, "vlc_memstream_open() failed");
        goto error;
    }

    enum libvlc_event_e answer_type = libvlc_MediaSubItemTreeAdded;
    size_t ret = vlc_memstream_write(&answer, &answer_type, sizeof(answer_type));
    assert(ret != sizeof(answer));

    libvlc_media_list_t *list_subitems = libvlc_media_subitems(media);

    unsigned int nb_subitems = libvlc_media_list_count(list_subitems);
    vlc_memstream_write(&answer, &nb_subitems, sizeof(nb_subitems));

    if (serialize_item_node_list(mediainfo, list_subitems, &answer)) {
        msg_Err(libvlc, "unable to serialize subitems");
        goto error;
    }

    if (vlc_memstream_close(&answer)) {
        msg_Err(libvlc, "vlc_memstream_close() failed");
        goto error;
    }

    vlc_write(1, answer.ptr, answer.length);

    return ;

error:
    answer_type = libvlc_MediaPlayerEncounteredError;
    vlc_write(1, &answer_type, sizeof(answer_type));
}

static void media_parse_changed(const libvlc_event_t *event, void *user_data)
{
    mediainfo_internal_t *mediainfo = (mediainfo_internal_t *)user_data;
    libvlc_int_t *const libvlc = mediainfo->vlc->p_libvlc_int;
    mediainfo_request_t *cur_req = mediainfo->current_request;

    libvlc_media_parsed_status_t status = event->u.media_parsed_changed.new_status;
    if (status != libvlc_media_parsed_status_done)
        return ;

    type_serializer_t *serializer = vlc_object_create( libvlc, sizeof(type_serializer_t));
    if (unlikely(!serializer))
        goto error;

    module_t *module = module_need(serializer, "serialize type", "serialize-item", true);
    if (unlikely(!module)) {
        msg_Err(libvlc, "cannot find a serializer for input_item_t");
        free(serializer);
        goto error;
    }

    struct vlc_memstream answer;
    if (vlc_memstream_open(&answer)) {
        msg_Err(libvlc, "vlc_memstream_open() failed");
        goto error;
    }

    enum libvlc_event_e answer_type = libvlc_MediaParsedChanged;
    vlc_memstream_write(&answer, &answer_type, sizeof(answer_type));

    if (vlc_memstream_flush(&answer)) {
        msg_Err(libvlc, "unable to serialize item: memstream flush failed");
        goto error;
    }

    unsigned int *item_size;
    memcpy(&item_size, answer.ptr + answer.length, sizeof(unsigned int *));
    unsigned int old_stream_length = answer.length;
    vlc_memstream_write(&answer, "itemsize", sizeof(unsigned int));

    /* serialize input_item_t parent */
    serializer->type = cur_req->item;
    serializer->serialized_type = &answer;
    if (serializer->pf_serialize(serializer)) {
        msg_Err(libvlc, "unable to serialize input_item_t: serialization failed");
        goto error;
    }

    if (vlc_memstream_close(&answer)) {
        msg_Err(libvlc, "unable to serialize input_item_t: vlc_memstream_close() failed");
        goto error;
    }

    *item_size = answer.length - old_stream_length - sizeof(*item_size);

    ssize_t ret = vlc_write(1, answer.ptr, answer.length);
    if ((size_t)ret != answer.length)
        msg_Err(libvlc, "unable to send serialized input_item_t");

    vlc_sem_post(&cur_req->sem);
    return ;

error:
    answer_type = libvlc_MediaPlayerEncounteredError;
    vlc_write(1, &answer_type, sizeof(answer_type));
    vlc_sem_post(&cur_req->sem);
}

static void media_attachment_found(const libvlc_event_t *event, void *user_data)
{
    mediainfo_internal_t *mediainfo = (mediainfo_internal_t *)user_data;
    libvlc_int_t *libvlc = mediainfo->vlc->p_libvlc_int;

    type_serializer_t *serializer = vlc_object_create( libvlc, sizeof(type_serializer_t));
    if (unlikely(!serializer))
        return ;

    struct vlc_memstream answer;
    if (vlc_memstream_open(&answer)) {
        msg_Dbg(libvlc, "vlc_memstream_open() failed");
        return;
    }

    module_t *module = module_need(serializer, "serialize type", "serialize-attachment", true);
    if (unlikely(!module)) {
        msg_Err(libvlc, "cannot find serializer module for input_attachment_t");
        return;
    }

    libvlc_picture_list_t* thumbnails = event->u.media_attached_thumbnails_found.thumbnails;
    unsigned int nb_pictures = libvlc_picture_list_count(thumbnails);

    vlc_memstream_write(&answer, &nb_pictures, sizeof(nb_pictures));

    serializer->serialized_type = &answer;
    for (unsigned int i = 0; i < nb_pictures; ++i) {
        libvlc_picture_t *picture = libvlc_picture_list_at(thumbnails, i);
        const unsigned int attachment_size = 0;
        if (vlc_memstream_flush(&answer)) {
            goto error;
        }

        const unsigned int old_stream_length = answer.length;

        unsigned int *serialized_buffer_size = (unsigned int *)answer.ptr;

        vlc_memstream_write(&answer, &attachment_size, sizeof(attachment_size));

        serializer->type = picture->attachment;
        if (serializer->pf_serialize(serializer)) {
            msg_Err(libvlc, "cannot serialize input_attachment_t");
            goto error;
        }

        if (vlc_memstream_flush(&answer)) {
            goto error;
        }

        *serialized_buffer_size = answer.length - old_stream_length - sizeof(unsigned int);
    }

    if (vlc_memstream_close(&answer))
        goto error;

    vlc_write(1, answer.ptr, answer.length);

error:
    //TODO free resources
    return;
}

static void implementation_not_found()
{
}

static unsigned int events[VLC_EVENT_TYPE_COUNT] = {
    libvlc_RendererDiscovererItemDeleted + 1,
    libvlc_RendererDiscovererItemDeleted + 1,
    libvlc_RendererDiscovererItemDeleted + 1,
    libvlc_RendererDiscovererItemDeleted + 1,
    libvlc_RendererDiscovererItemDeleted + 1,
    libvlc_RendererDiscovererItemDeleted + 1,
    libvlc_MediaAttachedThumbnailsFound,
};

static void (*supported_events[VLC_EVENT_TYPE_COUNT])() = {
    /* vlc_InputItemMetaChanged */
    &implementation_not_found,
    /* vlc_InputItemDurationChanged */
    &implementation_not_found,
    /* vlc_InputItemPreparsedChanged */
    &implementation_not_found,
    /* vlc_InputItemNameChanged */
    &implementation_not_found,
    /* vlc_InputItemInfoChanged */
    &implementation_not_found,
    /* vlc_InputItemErrorWhenReadingChanged */
    &implementation_not_found,
    /* vlc_InputItemAttachmentsFound */
    &media_attachment_found,
};

static int init_serializer_cbs(mediainfo_internal_t *mediainfo, libvlc_media_t *media)
{
    libvlc_event_manager_t *const em = libvlc_media_event_manager(media);

    int attach_status;
    attach_status = libvlc_event_attach(em, libvlc_MediaSubItemTreeAdded,
                                        media_subitemtree_added, mediainfo);
    if (attach_status)
        return attach_status;

    attach_status = libvlc_event_attach(em, libvlc_MediaParsedChanged,
                                        media_parse_changed, mediainfo);
    if (attach_status)
        return attach_status;

    attach_status = libvlc_event_attach(em, libvlc_MediaAttachedThumbnailsFound,
                                        media_attachment_found, mediainfo);
    if (attach_status)
        return attach_status;

    return VLC_SUCCESS;

    for (unsigned int i = vlc_InputItemMetaChanged; i < VLC_EVENT_TYPE_COUNT; ++i) {
        void (*func)() = supported_events[i];
        if (func == implementation_not_found)
            continue;
        attach_status = libvlc_event_attach(em, events[i], func, mediainfo);
        if (attach_status)
            return attach_status;
    }

    return VLC_SUCCESS;
}

static void deinit_serializer_cbs(mediainfo_internal_t *mediainfo, libvlc_media_t *media)
{
    libvlc_event_manager_t *const em = libvlc_media_event_manager(media);

    libvlc_event_detach(em, libvlc_MediaSubItemTreeAdded,
                                        media_subitemtree_added, mediainfo);

    libvlc_event_detach(em, libvlc_MediaParsedChanged,
                                        media_parse_changed, mediainfo);

    libvlc_event_detach(em, libvlc_MediaAttachedThumbnailsFound,
                                        media_attachment_found, mediainfo);

}

static int read_nbytes(int fd, void *dest, size_t size)
{
    unsigned int received_bytes = 0;
    do {
        ssize_t nb_bytes = read(fd, (char *)dest + received_bytes, size - received_bytes);
        if (nb_bytes < 1)
            return -1;
        received_bytes += nb_bytes;
    } while (received_bytes != size);

    return 0;
}

static mediainfo_request_t *get_request(libvlc_instance_t *vlc)
{
    libvlc_int_t *const libvlc = vlc->p_libvlc_int;
    mediainfo_request_t *req = malloc(sizeof(*req));
    if (unlikely(!req))
        return NULL;

    *req = (struct mediainfo_request_t) {
        .events_to_listen = {-1},
        .mrl_size = 0,
        .mrl = NULL,
    };

    unsigned int request_size;
    if (read_nbytes(0, &request_size, sizeof(request_size))) {
        msg_Err(libvlc, "unable to read vlc preparse request: request size not found");
        goto error;
    }

    unsigned int nb_events;
    if (read_nbytes(0, &nb_events, sizeof(nb_events))) {
        msg_Err(libvlc, "unable to read vlc preparse request: number of events not found");
        goto error;
    }

    while (nb_events--) {
        vlc_event_type_t event;
        if (read_nbytes(0, &event, sizeof(event))) {
            msg_Err(libvlc, "unable to read vlc preparse request: event incomplete");
            goto error;
        }
        req->events_to_listen[nb_events] = event;
        msg_Dbg(libvlc, "received event : %d", event);
    }

    if (read_nbytes(0, &req->mrl_size, sizeof(req->mrl_size))) {
        msg_Err(libvlc, "unable to read vlc preparse request: mrl size not found");
        goto error;
    }

    req->mrl = malloc(req->mrl_size);
    if (unlikely(!req->mrl))
        goto error;

    if (read_nbytes(0, req->mrl, req->mrl_size)) {
        msg_Err(libvlc, "unable to read vlc preparse request: mrl not found");
        goto error;
    }

    if (read_nbytes(0, &req->i_preparse_depth, sizeof(req->i_preparse_depth))) {
        msg_Err(libvlc, "unable to read vlc preparse request: preparse depth not found");
        goto error;
    }

    return req;

error:
    free(req->mrl);
    free(req);
    return NULL;
}

static void request_handler(mediainfo_internal_t *mediainfo)
{
    libvlc_instance_t *vlc = mediainfo->vlc;
    libvlc_int_t *libvlc = vlc->p_libvlc_int;

    for (;;) {
        /* receive mrl to parse */
        msg_Dbg(libvlc, "wait for a preparse request ...");
        mediainfo_request_t *req = get_request(mediainfo->vlc);
        if (unlikely(!req))
            break;

        msg_Dbg(libvlc, "received request %s", req->mrl);

        /* create media to parse based on mrl */
        libvlc_media_t *media = libvlc_media_new_location(req->mrl);
        if (unlikely(!media)) {
            msg_Dbg(libvlc, "unable to create media for \"%s\"", req->mrl);
            goto next;
        }
        free(req->mrl);

        /* create request associated to input_item */
        req->item = media->p_input_item;
        req->item->i_preparse_depth = req->i_preparse_depth;
        msg_Dbg(libvlc, "preparse depth %d", req->i_preparse_depth);
        mediainfo->current_request = req;

        /* setup callbacks to serialize items and subitems */
        if (unlikely(init_serializer_cbs(mediainfo, media) != VLC_SUCCESS)) {
            msg_Err(libvlc, "unable to attach parsing events");
            goto next;
        }

        vlc_sem_init(&req->sem, 0);

        /* begin parsing */
        libvlc_media_parse_flag_t flag = libvlc_media_parse_local   |
                                         libvlc_media_parse_network |
                                         libvlc_media_fetch_local   |
                                         libvlc_media_fetch_network;
        if (unlikely(libvlc_media_parse_request(vlc, media, flag, 0))) {
            msg_Err(libvlc, "parse request failed for %s", req->mrl);
            goto next;
        }

        /* wait for parsing end */
        vlc_sem_wait(&req->sem);

        /* detach events */
        deinit_serializer_cbs(mediainfo, media);

    next:
        free(req);
        libvlc_media_release(media);
    }
    msg_Dbg(libvlc, "request handler exit");
}

static mediainfo_internal_t *init_mediainfo(libvlc_instance_t *vlc)
{
    mediainfo_internal_t *mediainfo = malloc(sizeof(*mediainfo));
    if (unlikely(!mediainfo))
        return NULL;

    *mediainfo = (mediainfo_internal_t) {
        .vlc = vlc,
        .parser.serializer = init_serializer(VLC_OBJECT(vlc->p_libvlc_int)),
        .current_request = NULL,
    };

    if (!mediainfo->parser.serializer) {
        free(mediainfo);
        return NULL;
    }

    return mediainfo;
}

const char vlc_module_name[] = "mediainfo";

int main(void)
{
#ifdef TOP_BUILDDIR
    setenv ("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv ("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
#endif
    const char *args[] = {
        "--verbose=0", "--vout=vdummy", "--aout=adummy", "--text-renderer=tdummy",
    };
    const int nargs = sizeof (args) / sizeof (args[0]);

    libvlc_instance_t *vlc = libvlc_new(nargs, args);
    if (unlikely(!vlc))
        return -1;

    mediainfo_internal_t *mediainfo = init_mediainfo(vlc);
    if (unlikely(!mediainfo)) {
        goto end;
    }

    request_handler(mediainfo);

end:
    libvlc_release(vlc);
    return 0;
}
