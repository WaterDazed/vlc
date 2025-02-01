/*****************************************************************************
 * fs.c: file system access plugin
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont
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
#include "fs.h"
#include <vlc_plugin.h>

vlc_module_begin ()
    vlc_set_description( N_("File input") )
    vlc_set_shortname( N_("File") )
    vlc_set_subcategory( SUBCAT_INPUT_ACCESS )
    vlc_set_capability( "access", 50 )
    vlc_add_shortcut( "file", "fd", "stream" )
    vlc_set_callbacks( FileOpen, FileClose )

    vlc_add_submodule()
    vlc_set_section( N_("Directory" ), NULL )
    vlc_set_capability( "access", 55 )
#ifndef HAVE_FDOPENDIR
    vlc_add_shortcut( "file", "directory", "dir" )
#else
    vlc_add_shortcut( "directory", "dir" )
#endif
    vlc_set_callbacks( DirOpen, DirClose )

    vlc_add_bool("list-special-files", false, N_("List special files"),
             N_("Include devices and pipes when listing directories"))
    vlc_add_obsolete_string("directory-sort") /* since 3.0.0 */
vlc_module_end ()
