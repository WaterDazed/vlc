/*****************************************************************************
 * aiir.c : 
 *****************************************************************************
 * Copyright © 2022 VLC authors and VideoLAN
 *
 * Authors: Razdutt Sarnaik <rajduttsarnaik@gmail.com>
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

#include "avaudio.h"
#include "avaudio_lists.h"


static int Open_aiir( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define AIIR_FILTER_TEXT N_( "aiir" )
#define AIIR_FILTER_LONGTEXT N_( "Set arbitrary Infinite Impulse Response filter configuration." )

vlc_module_begin()

    set_shortname( N_("Aiir") )
    set_description( N_("Arbitrary Infinite Impulse Response filter") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_string( "aiir-filter", "aiir 2", AIIR_FILTER_TEXT,
                AIIR_FILTER_LONGTEXT )
        change_string_list( aiir_list, aiir_list_text )

    add_shortcut("aiir")
    set_callback( Open_aiir )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static int Open_aiir( vlc_object_t *p_this )
{
    struct vlc_memstream ms;
    char* aiir_filter_descr;
    
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = malloc( sizeof( *p_sys ) );

    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_filter->p_sys = p_sys;
    aiir_filter_descr = var_CreateGetString(p_filter,"aiir-filter");
    
/** Configure filter parameters (filter_desc)**/
    if( vlc_memstream_open( &ms ) != 0 )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    vlc_memstream_printf(&ms, "aiir=%s",aiir_filter_descr);

    if(vlc_memstream_close(&ms))
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    
    /* initialize ffmpeg filter */
    if(init_filters(ms.ptr, p_filter ,p_sys) <0)
    {
        avfilter_graph_free(&p_sys->filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    
    /*filter description*/
    msg_Dbg(p_filter,"aiir initialised: %s",ms.ptr);

    free( ms.ptr );

    p_sys->frame_len_flag = 1;

    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

