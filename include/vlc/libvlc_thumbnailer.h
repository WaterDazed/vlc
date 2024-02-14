/*****************************************************************************
 * libvlc_thumbnailer.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#ifndef VLC_LIBVLC_THUMBNAILER_H
#define VLC_LIBVLC_THUMBNAILER_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

/** \defgroup libvlc_thumbnailer LibVLC thumbnailer
 * \ingroup libvlc
 * @ref libvlc_thumbnailer_t is an abstract representation of a thumbnailer
 * @{
 * \file
 * LibVLC thumbnailer
 */

typedef struct libvlc_thumbnailer_t libvlc_thumbnailer_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_picture_t libvlc_picture_t;
typedef enum libvlc_picture_type_t libvlc_picture_type_t;

/**
 * \brief libvlc_thumbnailer_request_t An opaque thumbnail request object
 */
typedef struct libvlc_thumbnailer_request_t libvlc_thumbnailer_request_t;

/**
 * Initial version of the struct defining callbacks for
 * libvlc_thumbnailer_new()
 */
#define LIBVLC_THUMBNAILER_CBS_VER_0 0

/**
 * Last version of the struct defining callbacks for
 * libvlc_thumbnailer_new()
 */
#define LIBVLC_THUMBNAILER_CBS_VER_LATEST \
    LIBVLC_THUMBNAILER_CBS_VER_0

/**
 * struct defining callbacks for libvlc_thumbnailer_new()
 */
struct libvlc_thumbnailer_cbs
{
    /**
     * Callback prototype that notify when a thumbnailer request finishes
     *
     * \note Mandatory (can't be NULL),
     * available since \ref LIBVLC_THUMBNAILER_CBS_VER_0
     *
     * \param opaque opaque pointer set by libvlc_thumbnailer_new()
     * \param req request associated with the thumbnail. It is safe to destroy
     * it with libvlc_thumbnailer_request_destroy() from the callback. It is
     * possible to get the associated media with
     * libvlc_thumbnailer_request_get_media().
     * \param picture a valid picture or NULL in case of error/cancellation. The
     * user should hold the picture with libvlc_picture_retain() in order to keep
     * using this picture after the callback.
     */
    void (*on_picture)( void *opaque, libvlc_thumbnailer_request_t *req,
                        libvlc_picture_t *picture );
};

/**
 * \brief Create a thumbnailer
 *
 * \param inst LibVLC instance to create thumbnailer with
 * \param cbs_version version of the struct defining callbacks, should be
 * \ref LIBVLC_THUMBNAILER_CBS_VER_LATEST
 * \param cbs callback to listen to events (can't be NULL)
 * \param cbs_opaque opaque pointer used by the callbacks
 * \return a new thumbnailer object, or NULL on error.
 * It must be released by libvlc_thumbnailer_destroy().
 *
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API libvlc_thumbnailer_t *
libvlc_thumbnailer_new( libvlc_instance_t *inst,
                        unsigned cbs_version,
                        const struct libvlc_thumbnailer_cbs *cbs,
                        void *cbs_opaque );

/**
 * \brief Destroy a thumbnailer
 *
 * \warning all queued requests must be destroyed with
 * libvlc_thumbnailer_destroy_request() before calling this function.
 *
 * \param thumbnailer the thumbnailer to destroy
 *
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_destroy( libvlc_thumbnailer_t *thumbnailer );

/**
 * \brief Start an asynchronous thumbnail generation
 *
 * If the request is successfully queued, the \ref
 * libvlc_thumbnailer_cbs.on_picture callback is guaranteed to be called
 * (except if the request is destroyed early by the user).
 *
 * \note It is possible to cancel the request by destroying the request with
 * libvlc_thumbnailer_destroy_request().
 *
 * \param thumbnailer the thumbnailer
 * \param req a valid media
 *
 * \return 0 on success, -1 on error.
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API int
libvlc_thumbnailer_queue( libvlc_thumbnailer_t *thumbnailer,
                          libvlc_thumbnailer_request_t *req );

/**
 * \brief Create a thumbnailer request
 *
 * \param media the media used to take a thumbnail
 * \return a new thumbnailer request, or NULL on error.
 *
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API libvlc_thumbnailer_request_t *
libvlc_thumbnailer_request_new( libvlc_media_t *media );

/**
 * @brief Destroys a thumbnail request
 *
 * This will also cancel the thumbnail request if queued,
 * no events will be emitted after this call.
 *
 * \param req An opaque thumbnail request object.
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_request_destroy( libvlc_thumbnailer_request_t *req );

/**
 * \brief set the position for the request
 *
 * \note the default position (if this function is not called) is 0%
 *
 * \param req the request
 * \param pos The position at which the thumbnail should be generated
 * \param fast_seek true for fast seek, false for precise seek
 *
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_request_set_position( libvlc_thumbnailer_request_t *req,
                                         double pos, bool fast_seek );

/**
 * \brief set the time for the request
 *
 * \note the default time (if this function is not called) is 0 ms
 *
 * \param req the request
 * \param time The time at which the thumbnail should be generated
 * \param fast_seek true for fast seek, false for precise seek
 *
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_request_set_time( libvlc_thumbnailer_request_t *req,
                                     libvlc_time_t time, bool fast_seek );

/**
 * \brief set the output video size for the request
 *
 * \note the default size (if this function is not called) is 0, 0, false
 * (original picture size)
 *
 * The resulting thumbnail size can either be:
 * - Hardcoded by providing both width & height. In which case, the image will
 *   be stretched to match the provided aspect ratio, or cropped if crop is true.
 * - Derived from the media aspect ratio if only width or height is provided and
 *   the other one is set to 0.
 *
 * \param req the request
 * \param width the output picture width, 0 for original size
 * \param height the output picture height, 0 for original size
 * \param crop true if the picture should be cropped to preserve source aspect
 * ratio
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_request_set_picture_size( libvlc_thumbnailer_request_t *req,
                                             unsigned int width, unsigned int height,
                                             bool crop );

/**
 * \brief set the picture type for the request
 *
 * \note the default type (if this function is not called) is Argb
 *
 * \param req the request
 * \param type the type of the output picture, cf. \ref libvlc_picture_type_t
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_request_set_picture_type( libvlc_thumbnailer_request_t *req,
                                             libvlc_picture_type_t picture_type );

/**
 * \brief Set the timeout for the request
 *
 * \note the default behavior (if this function is not called) is to wait
 * indefinitely
 *
 * \param req the request
 * \param timeout a timeout value in ms, or 0 to disable timeout
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_thumbnailer_request_set_timeout( libvlc_thumbnailer_request_t *req,
                                        libvlc_time_t timeout );

/**
 * Get the media associated with the request
 *
 * \param request A parser request object
 * \return the media passed in libvlc_thumbnailer_request(), already held by
 * the request (don't release it)
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_media_t *
libvlc_thumbnailer_request_get_media( libvlc_thumbnailer_request_t *request );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_THUMBNAILER_H */
