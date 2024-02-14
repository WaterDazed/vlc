/*****************************************************************************
 * libvlc_parser.h:  libvlc parser API
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

#ifndef VLC_LIBVLC_PARSER_H
#define VLC_LIBVLC_PARSER_H 1

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_parser LibVLC parser
 * \ingroup libvlc
 * @ref libvlc_parser_t is an abstract representation of a parser
 * @{
 * \file
 * LibVLC parser API
 */

typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_picture_list_t libvlc_picture_list_t;
typedef enum libvlc_media_parsed_status_t libvlc_media_parsed_status_t;

typedef struct libvlc_parser_t libvlc_parser_t;
/**
 * \brief libvlc_parser_request_t An opaque parser request object
 */
typedef struct libvlc_parser_request_t libvlc_parser_request_t;

/**
 * Parse flags used by libvlc_media_parse_request()
 */
typedef enum libvlc_media_parse_flag_t
{
    /**
     * Parse media if it's a local file
     */
    libvlc_media_parse_local    = 0x01,
    /**
     * Parse media even if it's a network file
     */
    libvlc_media_parse_network  = 0x02,
    /**
     * Force parsing the media even if it would be skipped.
     */
    libvlc_media_parse_forced   = 0x04,
    /**
     * Fetch meta and cover art using local resources
     */
    libvlc_media_fetch_local    = 0x08,
    /**
     * Fetch meta and cover art using network resources
     */
    libvlc_media_fetch_network  = 0x10,
    /**
     * Interact with the user (via libvlc_dialog_cbs) when preparsing this item
     * (and not its sub items). Set this flag in order to receive a callback
     * when the input is asking for credentials.
     */
    libvlc_media_do_interact    = 0x20,
} libvlc_media_parse_flag_t;

/**
 * Initial version of the struct defining callbacks for
 * libvlc_parser_new()
 */
#define LIBVLC_PARSER_CBS_VER_0 0

/**
 * Last version of the struct defining callbacks for
 * libvlc_parser_new()
 */
#define LIBVLC_PARSER_CBS_VER_LATEST \
    LIBVLC_PARSER_CBS_VER_0

/**
 * struct defining callbacks for libvlc_parser_new()
 */
struct libvlc_parser_cbs
{
    /**
     * Callback prototype that notify when a parser request finishes
     *
     * \note Mandatory (can't be NULL),
     * available since \ref LIBVLC_PARSER_CBS_VER_0
     *
     * \param opaque opaque pointer set by libvlc_parser_new()
     * \param req request associated with the parsing. It is safe to destroy it
     * with libvlc_parser_request_destroy() from the callback. It is possible
     * to get the associated media with libvlc_parser_request_get_media().
     * \param status Parsed status
     */
    void (*on_parsed)( void *opaque, libvlc_parser_request_t *req,
                       libvlc_media_parsed_status_t status );

    /**
     * Callback prototype that notify when the parser add new attachments to
     * the media.
     *
     * Called before on_parsed, if there are valid attachments.
     *
     * \param opaque opaque pointer set by libvlc_parser_new()
     * \param req request associated with the parsing
     * \param list list of pictures, the list is only valid from this callback,
     * each pictures can be held separatly with libvlc_picture_retain().
     */
    void (*on_attachments_added)( void *opaque, libvlc_parser_request_t *req,
                                  libvlc_picture_list_t *list );
};

/**
 * Create a parser
 *
 * \param inst LibVLC instance to create parser with
 * \param cbs_version version of the struct defining callbacks, should be
 * \ref LIBVLC_PARSER_CBS_VER_LATEST
 * \param cbs callback to listen to events (must not be NULL)
 * \param cbs_opaque opaque pointer used by the callbacks
 * \return a new parser object, or NULL on error.
 * It must be released by libvlc_parser_destroy().
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API libvlc_parser_t *
libvlc_parser_new( libvlc_instance_t *inst, unsigned cbs_version,
                   const struct libvlc_parser_cbs *cbs,
                   void *cbs_opaque );

/**
 * Destroy a parser
 *
 * \warning all requests must be destroyed with
 * libvlc_parser_destroy_request() before calling this function.
 *
 * \param parser the parser
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_parser_destroy( libvlc_parser_t *parser );

/**
 * Parse a media asynchronously
 *
 * This fetches (local or network) art, meta data and/or tracks information.
 *
 * If the request is successfully queued, the \ref libvlc_parser_cbs.on_parsed
 * callback is guaranteed to be called (except if the request is destroyed
 * early by the user).
 *
 * \note It is possible to cancel the request by destroying the request with
 * libvlc_parser_destroy_request().
 *
 * \param parser the parser
 * \param req a valid request
 * \return 0 on success, -1 on error.
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API int
libvlc_parser_queue( libvlc_parser_t *parser, libvlc_parser_request_t *req );

/**
 * \brief Create a parser request
 *
 * \param media the media to parse
 * \return a new parser request, or NULL on error.
 *
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API libvlc_parser_request_t *
libvlc_parser_request_new( libvlc_media_t *media );

/**
 * @brief Destroys a parse request
 *
 * This will also cancel the parse request if queued,
 * no events will be emitted after this call.
 *
 * \param req An opaque parse request object.
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_parser_request_destroy( libvlc_parser_request_t *req );

/**
 * \brief Set the timeout for the request
 *
 * \note the default behavior (if this function is not called) is to wait
 * 5 seconds ("preparse-timeout").
 *
 * \param req the request
 * \param timeout maximum time allowed to preparse the media. If -1, the
 * default "preparse-timeout" option will be used as a timeout. If 0, it will
 * wait indefinitely. If > 0, the timeout will be used (in milliseconds).
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_parser_request_set_timeout( libvlc_parser_request_t *req,
                                   libvlc_time_t timeout );

/**
 * \brief Set the parse flags for the request
 *
 * \note the default behavior (if this function is not called) is to only parse
 * local files (libvlc_media_parse_local).
 *
 * It uses a flag to specify parse options (see libvlc_media_parse_flag_t). All
 * these flags can be combined.
 *
 * \param req the request
 * \param parse_flag parse options, cf \ref libvlc_media_parse_flag_t
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_parser_request_set_flags( libvlc_parser_request_t *req,
                                 libvlc_media_parse_flag_t parse_flag );

/**
 * Destroys a parser request
 *
 * This will also cancel the parser request, no events will be emitted after
 * this call.
 *
 * \param req A parser request object.
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void
libvlc_parser_request_destroy( libvlc_parser_request_t *request );

/**
 * Get the media associated with the request
 *
 * \param request A parser request object
 * \return the media passed in libvlc_parser_request(), already held by the
 * request (don't release it)
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_media_t *
libvlc_parser_request_get_media( libvlc_parser_request_t *request );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_PARSER_H */
