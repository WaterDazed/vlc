/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#include <vlc_access.h>

#if defined( _WIN32 ) || defined( __OS2__ )
# include <ctype.h>                          /* isalpha */
#endif
#include <assert.h>

#include "playlist.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SHOW_ADULT_TEXT N_( "Show shoutcast adult content" )
#define SHOW_ADULT_LONGTEXT N_( "Show NC17 rated video streams when " \
                "using shoutcast video playlists." )

#define SKIP_ADS_TEXT N_( "Skip ads" )
#define SKIP_ADS_LONGTEXT N_( "Use playlist options usually used to prevent " \
    "ads skipping to detect ads and prevent adding them to the playlist." )

vlc_module_begin ()
    vlc_add_shortcut( "playlist" )
    vlc_set_subcategory( SUBCAT_INPUT_DEMUX )

    vlc_add_bool( "playlist-skip-ads", true,
              SKIP_ADS_TEXT, SKIP_ADS_LONGTEXT )

    vlc_set_shortname( N_("Playlist") )
    vlc_set_description( N_("Playlist") )
    vlc_add_submodule ()
        vlc_set_description( N_("M3U playlist import") )
        vlc_add_shortcut( "m3u", "m3u8" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_M3U )
        vlc_add_file_extension("m3u")
    vlc_add_submodule ()
        vlc_set_description( N_("RAM playlist import") )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_RAM )
    vlc_add_submodule ()
        vlc_set_description( N_("PLS playlist import") )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_PLS )
    vlc_add_submodule ()
        vlc_set_description( N_("B4S playlist import") )
        vlc_add_shortcut( "shout-b4s" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_B4S )
    vlc_add_submodule ()
        vlc_set_description( N_("DVB playlist import") )
        vlc_add_shortcut( "dvb" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_DVB )
    vlc_add_submodule ()
        vlc_set_description( N_("Podcast parser") )
        vlc_add_shortcut( "podcast" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_podcast )
    vlc_add_submodule ()
        vlc_set_description( N_("XSPF playlist import") )
        vlc_set_capability( "demux", 10 )
        vlc_set_callbacks( Import_xspf, Close_xspf )
    vlc_add_submodule ()
        vlc_set_description( N_("ASX playlist import") )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_ASX )
    vlc_add_submodule ()
        vlc_set_description( N_("Kasenna MediaBase parser") )
        vlc_add_shortcut( "sgimb" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callbacks( Import_SGIMB, Close_SGIMB )
    vlc_add_submodule ()
        vlc_set_description( N_("QuickTime Media Link importer") )
        vlc_add_shortcut( "qtl" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_QTL )
    vlc_add_submodule ()
        vlc_set_description( N_("Dummy IFO demux") )
        vlc_set_capability( "demux", 12 )
        vlc_set_callback( Import_IFO )
    vlc_add_submodule ()
        vlc_set_description( N_("Dummy BDMV demux") )
        vlc_set_capability( "demux", 12 )
        vlc_set_callback( Import_BDMV )
    vlc_add_submodule ()
        vlc_set_description( N_("iTunes Music Library importer") )
        vlc_add_shortcut( "itml" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callback( Import_iTML )
    vlc_add_submodule()
        vlc_set_description(N_("Windows Media Server metafile import") )
        vlc_set_capability("demux", 10)
        vlc_set_callback(Import_WMS)
    vlc_add_submodule ()
        vlc_set_description( N_("WPL playlist import") )
        vlc_add_shortcut( "wpl" )
        vlc_set_capability( "demux", 10 )
        vlc_set_callbacks( Import_WPL, Close_WPL )
vlc_module_end ()

/**
 * Resolves a playlist location.
 *
 * Resolves a resource location within the playlist relative to the playlist
 * base URL.
 */
char *ProcessMRL(const char *str, const char *base)
{
    char const* orig = str;

    if (str == NULL)
        return NULL;

#if (DIR_SEP_CHAR == '\\')
    /* UNC path prefix? */
    if (strncmp(str, "\\\\", 2) == 0
    /* Drive letter prefix? */
     || (isalpha((unsigned char)str[0]) && str[1] == ':'))
        /* Assume this an absolute file path - usually true */
        return vlc_path2uri(str, NULL);
    /* TODO: drive-relative path: if (str[0] == '\\') */
#endif

    /* The base URL is always an URL: it is the URL of the playlist.
     *
     * However it is not always known if the input string is a valid URL, a
     * broken URL or a local file path. As a rule, if it looks like a valid
     * URL, it must be treated as such, since most playlist formats use URLs.
     *
     * There are a few corner cases file paths that look like an URL but whose
     * URL representation does not match, notably when they contain a
     * percentage sign, a colon, a hash or a question mark. Luckily, they are
     * rather exceptional (and can be encoded as URL to make the playlist
     * work properly).
     *
     * If the input is not a valid URL, then we try to fix it up. It works in
     * all cases for URLs with incorrectly encoded segments, such as URLs with
     * white spaces or non-ASCII Unicode code points. It also works in most
     * cases where the input is a Unix-style file path, but not all.
     * It fails miserably if the playlist character encoding is misdetected.
     */
    char *rel = vlc_uri_fixup(str);
    if (rel != NULL)
        str = rel;

    char *abs = vlc_uri_resolve(base, str);
    free(rel);

    if (abs == NULL)
    {
        /** If the input is not a valid URL, see if there is a scheme:// where
         * the scheme itself consists solely of valid scheme-characters
         * (including the VLC's scheme-extension). If it does, fall back to
         * allowing the URI in order to not break back-compatibility.
         */
        char const* scheme_end = strstr( orig, "://" );
        char const* valid_chars = "abcdefghijklmnopqrstuvwxyz"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "0123456789" "+-./";
        if (scheme_end &&
            strspn (orig, valid_chars) == (size_t)(scheme_end - orig))
        {
            abs = strdup (orig);
        }
    }

    return abs;
}

int PlaylistControl( stream_t *p_access, int i_query, va_list args )
{
    switch ( i_query )
    {
        case STREAM_GET_TYPE:
        {
            *va_arg( args, int* ) = ITEM_TYPE_PLAYLIST;
            return VLC_SUCCESS;
        }
        default:
            return access_vaDirectoryControlHelper( p_access, i_query, args );
    }
}
