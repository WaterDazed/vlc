/*****************************************************************************
 * parser.c: Libvlc parser API
 *****************************************************************************
 * Copyright (C) 2007-2023 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
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

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_preparser.h>

#include "../src/libvlc.h"

#include "libvlc_internal.h"
#include "media_internal.h"
#include "picture_internal.h"

struct libvlc_parser_t {
    libvlc_instance_t *libvlc;
    vlc_preparser_t *preparser;

    const struct libvlc_parser_cbs *cbs;
    void *cbs_opaque;
};

struct libvlc_parser_request_t {
    libvlc_parser_t *parser;
    libvlc_media_t *media;
    int timeout_ms;
    input_item_meta_request_option_t parse_scope;
};

static void input_item_subtree_added(input_item_t *item,
                                     input_item_node_t *node,
                                     void *user_data)
{
    VLC_UNUSED(item);

    libvlc_parser_request_t *request = user_data;
    libvlc_media_add_subtree(request->media, node);
}

/**
 * \internal
 * input_item_preparse_ended (Private) (vlc event Callback)
 */
static void input_item_preparse_ended(input_item_t *item,
                                      enum input_item_preparse_status status,
                                      void *user_data)
{
    VLC_UNUSED(item);
    libvlc_parser_request_t *request = user_data;
    assert(request->media->p_input_item == item);

    libvlc_media_parsed_status_t new_status;
    switch( status )
    {
        case ITEM_PREPARSE_SKIPPED:
            new_status = libvlc_media_parsed_status_skipped;
            break;
        case ITEM_PREPARSE_FAILED:
            new_status = libvlc_media_parsed_status_failed;
            break;
        case ITEM_PREPARSE_TIMEOUT:
            new_status = libvlc_media_parsed_status_timeout;
            break;
        case ITEM_PREPARSE_DONE:
            new_status = libvlc_media_parsed_status_done;
            break;
        default:
            return;
    }

    atomic_store(&request->media->parsed_status, new_status);

    request->parser->cbs->on_parsed( request->parser->cbs_opaque,
                                     request, new_status );
}

static void input_item_attachments_added( input_item_t *item,
                                          input_attachment_t *const *array,
                                          size_t count, void *user_data )
{
    VLC_UNUSED(item);
    libvlc_parser_request_t *request = user_data;
    assert(request->media->p_input_item == item);

    if (request->parser->cbs->on_attachments_added == NULL)
        return;

    libvlc_picture_list_t* list =
        libvlc_picture_list_from_attachments(array, count);
    if( !list )
        return;
    if( !libvlc_picture_list_count(list) )
    {
        libvlc_picture_list_destroy( list );
        return;
    }

    request->parser->cbs->on_attachments_added( request->parser->cbs_opaque,
                                                request, list );
    libvlc_picture_list_destroy( list );
}

static const struct vlc_metadata_cbs preparser_callbacks = {
    .on_preparse_ended = input_item_preparse_ended,
    .on_subtree_added = input_item_subtree_added,
    .on_attachments_added = input_item_attachments_added,
};

int
libvlc_parser_queue( libvlc_parser_t *parser, libvlc_parser_request_t *req )
{
    assert( req != NULL );

    libvlc_media_t *media = req->media;
    libvlc_media_parsed_status_t expected = libvlc_media_parsed_status_none;

    while (!atomic_compare_exchange_weak(&media->parsed_status, &expected,
                                         libvlc_media_parsed_status_pending))
        if (expected == libvlc_media_parsed_status_pending
         || expected == libvlc_media_parsed_status_done)
            return -1;

    req->parser = parser;

    input_item_t *item = media->p_input_item;
    int ret;

    vlc_mutex_lock( &item->lock );
    if( item->i_preparse_depth == 0 )
        item->i_preparse_depth = 1;
    vlc_mutex_unlock( &item->lock );

    ret = vlc_preparser_Push( parser->preparser, item, req->parse_scope,
                              &preparser_callbacks, req, req->timeout_ms,
                              req );
    if (ret != VLC_SUCCESS)
        return -1;
    return 0;
}

libvlc_parser_request_t *
libvlc_parser_request_new( libvlc_media_t *md  )
{
    assert( md != NULL );

    libvlc_parser_request_t *req = malloc( sizeof(*req) );
    if( req == NULL )
        return NULL;

    req->media = libvlc_media_retain( md );
    req->parser = NULL;
    req->timeout_ms = -1;
    req->parse_scope = META_REQUEST_OPTION_SCOPE_LOCAL;

    return req;
}

void
libvlc_parser_request_destroy( libvlc_parser_request_t *req )
{
    libvlc_media_release( req->media );
    if( req->parser != NULL )
        vlc_preparser_Cancel( req->parser->preparser, req );
    free( req );
}

void
libvlc_parser_request_set_timeout( libvlc_parser_request_t *req,
                                   libvlc_time_t timeout )
{
    req->timeout_ms = timeout;
}

void
libvlc_parser_request_set_flags( libvlc_parser_request_t *req,
                                 libvlc_media_parse_flag_t parse_flag )
{
    input_item_meta_request_option_t parse_scope = 0;

    if (parse_flag & libvlc_media_parse_local)
        parse_scope |= META_REQUEST_OPTION_SCOPE_LOCAL;
    if (parse_flag & libvlc_media_parse_network)
        parse_scope |= META_REQUEST_OPTION_SCOPE_NETWORK;
    if (parse_flag & libvlc_media_parse_forced)
        parse_scope |= META_REQUEST_OPTION_SCOPE_FORCED;
    if (parse_flag & libvlc_media_fetch_local)
        parse_scope |= META_REQUEST_OPTION_FETCH_LOCAL;
    if (parse_flag & libvlc_media_fetch_network)
        parse_scope |= META_REQUEST_OPTION_FETCH_NETWORK;
    if (parse_flag & libvlc_media_do_interact)
        parse_scope |= META_REQUEST_OPTION_DO_INTERACT;

    req->parse_scope = parse_scope;
}

libvlc_media_t *
libvlc_parser_request_get_media( libvlc_parser_request_t *request )
{
    return request->media;
}

libvlc_parser_t *
libvlc_parser_new( libvlc_instance_t *inst, unsigned cbs_version,
                   const struct libvlc_parser_cbs *cbs,
                   void *cbs_opaque )
{
    assert( inst != NULL );
    assert( cbs != NULL && cbs->on_parsed != NULL );

    /* No different versions to handle for now */
    (void) cbs_version;

    libvlc_parser_t *parser = malloc( sizeof(*parser) );
    if( parser == NULL )
        return NULL;

    parser->preparser = vlc_preparser_New( VLC_OBJECT( inst->p_libvlc_int ) );
    if( parser->preparser == NULL)
    {
        free( parser );
        return NULL;
    }
    parser->libvlc = libvlc_retain(inst);
    parser->cbs = cbs;
    parser->cbs_opaque = cbs_opaque;

    return parser;
}

void
libvlc_parser_destroy( libvlc_parser_t *parser )
{
    vlc_preparser_Delete( parser->preparser );
    libvlc_release( parser->libvlc );
    free( parser );
}
