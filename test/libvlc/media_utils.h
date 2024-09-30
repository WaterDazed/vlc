/*
 * media_utils.h - media utils function
 */

/**********************************************************************
 *  Copyright (C) 2021 VLC authors and VideoLAN                       *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

static void
libvlc_media_parse_finished_event(void *opaque, libvlc_parser_request_t *req,
                                  libvlc_media_parsed_status_t status)
{
    libvlc_media_t *media = libvlc_parser_request_get_media(req);
    assert(media != NULL);
    assert(libvlc_media_get_parsed_status(media) == status);
    libvlc_parser_request_destroy(req);

    vlc_sem_t *sem = opaque;
    vlc_sem_post(sem);
}

static inline void
libvlc_media_parse_sync(libvlc_instance_t *vlc, libvlc_media_t *p_m,
                        libvlc_media_parse_flag_t parse_flag, int timeout)
{
    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    struct libvlc_parser_cbs cbs = {
        .on_parsed = libvlc_media_parse_finished_event,
    };
    libvlc_parser_t *parser =
        libvlc_parser_new(vlc, LIBVLC_PARSER_CBS_VER_LATEST, &cbs, &sem);
    assert(parser != NULL);
    libvlc_parser_request_t *req = libvlc_parser_request_new(p_m);
    assert(req != NULL);
    libvlc_parser_request_set_timeout(req, timeout);
    libvlc_parser_request_set_flags(req, parse_flag);
    int ret = libvlc_parser_queue(parser, req);
    assert(ret == 0);

    vlc_sem_wait (&sem);

    libvlc_parser_destroy(parser);
}
