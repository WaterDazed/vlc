/*****************************************************************************
 * adelay.c : 
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


static int Open_adelay( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ADELAY_DELAYS_TEXT N_( "delays" )
#define ADELAY_DELAYS_LONGTEXT N_( "Set list of delays in milliseconds for each channel" \
                " separated by '|'. Unused delays will be silently ignored." \
                " If number of given delays is smaller than number of channels all" \
                " remaining channels will not be delayed." )

vlc_module_begin()

    set_shortname( N_("Adelay") )
    set_description( N_("Delay one or more audio channels.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_string( "adelay-delays", "1000", ADELAY_DELAYS_TEXT,
                ADELAY_DELAYS_LONGTEXT )

    add_shortcut( "adelay" )
    set_callback( Open_adelay )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static int Open_adelay( vlc_object_t *p_this )
{
    struct vlc_memstream ms;

    char *delay;
    
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_filter->p_sys = p_sys;
    delay = var_CreateGetString(p_filter,"adelay-delays");
    

/** Configure filter parameters (filters_descr) **/
    if( vlc_memstream_open( &ms ) != 0 )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    vlc_memstream_printf(&ms, "adelay=");

    /* Checking delay Bounds */
    if (delay != NULL)
    {
        char *delay_all = check_Bounds(p_filter,delay,0.000001 ,50000, 0);
        if(!delay_all)
            vlc_memstream_printf(&ms, "0");
        else
            vlc_memstream_printf(&ms, "%s", delay_all);

        free(delay_all);
    }
    else
    {
        vlc_memstream_printf(&ms, "0");
        msg_Warn(p_filter, "delay parameter not set. Set to default(0).");
    }

    if(vlc_memstream_close(&ms))
    {
        free(p_sys);
        return VLC_EGENERIC;
    }


    p_filter->fmt_out.audio = p_filter->fmt_in.audio;

    /* initialize ffmpeg filter */
    if(init_filters(ms.ptr, p_filter, p_sys) <0)
    {
        avfilter_graph_free(&p_sys->filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"adelay initialised: %s",ms.ptr);

    free(ms.ptr);

    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

