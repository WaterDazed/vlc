/*****************************************************************************
 * thumbnailer.c: Libvlc thumbnailer API
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#include <vlc/libvlc_thumbnailer.h>

#include <vlc_common.h>
#include <vlc_thumbnailer.h>

#include "../src/libvlc.h"

#include "libvlc_internal.h"
#include "media_internal.h"
#include "picture_internal.h"

struct libvlc_thumbnailer_t
{
    libvlc_instance_t *instance;
    vlc_thumbnailer_t *thumb;

    const struct libvlc_thumbnailer_cbs *cbs;
    void *cbs_opaque;
};

struct libvlc_thumbnailer_request_t
{
    vlc_atomic_rc_t rc;
    libvlc_thumbnailer_t *libthumb;
    libvlc_media_t *md;
    vlc_thumbnailer_request_t* _Atomic req;

    vlc_tick_t timeout;

    libvlc_picture_type_t picture_type;

    double pos;
    vlc_tick_t time;
    bool fast_seek;

    unsigned int width;
    unsigned int height;
    bool crop;
};

static void media_on_thumbnail_ready( void* data, picture_t* thumbnail )
{
    libvlc_thumbnailer_request_t *req = data;

    vlc_thumbnailer_request_t *creq = atomic_exchange( &req->req, NULL );
    if( creq != NULL )
        vlc_thumbnailer_request_Destroy( creq );

    libvlc_picture_t* pic = NULL;
    if ( thumbnail != NULL )
        pic = libvlc_picture_new( VLC_OBJECT(req->libthumb->instance->p_libvlc_int),
                                  thumbnail, req->picture_type,
                                  req->width, req->height, req->crop );

    req->libthumb->cbs->on_picture( req->libthumb->cbs_opaque, req, pic );

    if ( pic != NULL )
        libvlc_picture_release( pic );
}

libvlc_thumbnailer_t *
libvlc_thumbnailer_new( libvlc_instance_t *inst,
                        unsigned cbs_version,
                        const struct libvlc_thumbnailer_cbs *cbs,
                        void *cbs_opaque )
{
    assert( inst != NULL );
    assert( cbs != NULL && cbs->on_picture != NULL );

    /* No different versions to handle for now */
    (void) cbs_version;

    libvlc_thumbnailer_t *libthumb = malloc( sizeof(*libthumb) );
    if( libthumb == NULL )
        return NULL;

    libthumb->thumb = vlc_thumbnailer_Create( VLC_OBJECT( inst->p_libvlc_int ) );
    if( libthumb->thumb == NULL )
    {
        free( libthumb );
        return NULL;
    }
    libvlc_retain( inst );
    libthumb->instance = inst;

    libthumb->cbs = cbs;
    libthumb->cbs_opaque = cbs_opaque;
    return libthumb;
}

void
libvlc_thumbnailer_destroy( libvlc_thumbnailer_t *libthumb )
{
    vlc_thumbnailer_Release( libthumb->thumb );
    libvlc_release( libthumb->instance );
    free( libthumb );
}

libvlc_thumbnailer_request_t *
libvlc_thumbnailer_request_new( libvlc_media_t *md )
{
    libvlc_thumbnailer_request_t *req = malloc( sizeof(*req) );
    if( unlikely(req == NULL) )
        return NULL;

    req->libthumb = NULL;
    atomic_init( &req->req, NULL );
    req->req = NULL;
    req->timeout = VLC_TICK_INVALID;
    req->md = libvlc_media_retain( md );

    req->picture_type = libvlc_picture_Argb;

    req->time = VLC_TICK_INVALID;
    req->pos = -1;
    req->fast_seek = true;

    req->width = 0;
    req->height = 0;
    req->crop = false;

    return req;
}

void
libvlc_thumbnailer_request_set_position( libvlc_thumbnailer_request_t *req,
                                         double pos, bool fast_seek )
{
    req->time = VLC_TICK_INVALID;
    req->pos = pos;
    req->fast_seek = fast_seek;
}

void
libvlc_thumbnailer_request_set_time( libvlc_thumbnailer_request_t *req,
                                     libvlc_time_t time, bool fast_seek )
{
    req->pos = -1;
    req->time = time >= 0 ? vlc_tick_from_libvlc_time( time ) : VLC_TICK_INVALID;
    req->fast_seek = fast_seek;
}

void
libvlc_thumbnailer_request_set_picture_size( libvlc_thumbnailer_request_t *req,
                                             unsigned int width, unsigned int height,
                                             bool crop )
{
    req->width = width;
    req->height = height;
    req->crop = crop;
}

void
libvlc_thumbnailer_request_set_picture_type( libvlc_thumbnailer_request_t *req,
                                             libvlc_picture_type_t picture_type )
{
    req->picture_type = picture_type;
}

void
libvlc_thumbnailer_request_set_timeout( libvlc_thumbnailer_request_t *req,
                                        libvlc_time_t timeout )
{
    req->timeout = timeout > 0 ? vlc_tick_from_libvlc_time( timeout )
                               : VLC_TICK_INVALID;
}

int
libvlc_thumbnailer_queue( libvlc_thumbnailer_t *libthumb,
                          libvlc_thumbnailer_request_t *req )
{
    assert( libthumb != NULL && req != NULL );

    if( unlikely( req->req != NULL ) )
        return -1;

    req->libthumb = libthumb;

    enum vlc_thumbnailer_seek_speed seek_speed = req->fast_seek ?
        VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE;

    vlc_thumbnailer_request_t *creq;
    if( req->pos >= 0 )
        creq = vlc_thumbnailer_RequestByPos( libthumb->thumb, req->pos,
            seek_speed, req->md->p_input_item, req->timeout,
            media_on_thumbnail_ready, req );
    else
        creq = vlc_thumbnailer_RequestByTime( libthumb->thumb, req->time,
            seek_speed, req->md->p_input_item, req->timeout,
            media_on_thumbnail_ready, req );
    if ( creq == NULL )
        return -1;

    /* Use an atomic only for req->req since this is the only var written after
     * the thread fence triggered by vlc_thumbnailer_RequestBy*(). This allows
     * libvlc_thumbnailer_request_destroy() to be called from any thread (even
     * from the callback). */
    atomic_store( &req->req, creq );
    return 0;
}

void libvlc_thumbnailer_request_destroy( libvlc_thumbnailer_request_t *req )
{
    vlc_thumbnailer_request_t *creq = atomic_exchange( &req->req, NULL );
    if( creq != NULL )
    {
        vlc_thumbnailer_CancelRequest( req->libthumb->thumb, creq );
        vlc_thumbnailer_request_Destroy( creq );
    }

    libvlc_media_release( req->md );
    free( req );
}

libvlc_media_t *
libvlc_thumbnailer_request_get_media( libvlc_thumbnailer_request_t *request )
{
    return request->md;
}
