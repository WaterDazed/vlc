/*****************************************************************************
 * aecho.c : 
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


static int Open_aecho( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define AECHO_IN_GAIN_TEXT N_( "in_gain" )
#define AECHO_IN_GAIN_LONGTEXT N_( "Set the in_gain." )

#define AECHO_OUT_GAIN_TEXT N_( "out_gain" )
#define AECHO_OUT_GAIN_LONGTEXT N_( "Set the out_gain (reflected)." )

#define AECHO_DELAY_TEXT N_( "delays" )
#define AECHO_DELAY_LONGTEXT N_( "Set list of time intervals in " \
                    "milliseconds between original signal and " \
                    "reflections separated by '|'. Allowed " \
                    "range for each delay is (0 - 90000.0]" )

#define AECHO_DECAY_TEXT N_( "decays" )
#define AECHO_DECAY_LONGTEXT N_( "Set list of loudness of reflected " \
                    "signals separated by '|'. Allowed range for " \
                    "each decay is (0 - 1.0]. Default is 0.5." )

vlc_module_begin()

    set_shortname( N_("Aecho") )
    set_description( N_("Echo effect") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "aecho-in_gain", 0.6, 0.0, 1.0,
            AECHO_IN_GAIN_TEXT, AECHO_IN_GAIN_LONGTEXT )

    add_float_with_range( "aecho-out_gain", 0.3, 0.0, 1.0,
            AECHO_OUT_GAIN_TEXT, AECHO_OUT_GAIN_LONGTEXT )
    
    add_string( "aecho-delay", "1000", AECHO_DELAY_TEXT,
                AECHO_DELAY_LONGTEXT )

    add_string( "aecho-decay", "0.5", AECHO_DECAY_TEXT,
                AECHO_DECAY_LONGTEXT )

    add_shortcut("aecho")
    set_callback( Open_aecho )

vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static int Open_aecho( vlc_object_t *p_this )
{
    struct vlc_memstream ms;

    float in_gain;
    float out_gain;
    char *delay;
    char *decay;
    
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;


    p_filter->p_sys = p_sys;
    in_gain = var_CreateGetFloat(p_filter,"aecho-in_gain");
    out_gain = var_CreateGetFloat(p_filter,"aecho-out_gain");
    delay = var_CreateGetString(p_filter,"aecho-delay");
    decay = var_CreateGetString(p_filter,"aecho-decay");

/** Configure filter parameters (filter_desc)**/

    if( vlc_memstream_open( &ms ) != 0 )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    vlc_memstream_printf(&ms, "aecho=%f:%f:",in_gain, out_gain);

    /* Checking delay Bounds   */
    if (delay != NULL)
    {
        char *delay_all = check_Bounds(p_filter, delay, 0, 90000, 1000);
        if(!delay_all)
            vlc_memstream_printf(&ms, "1000:");
        else
            vlc_memstream_printf(&ms, "%s:", delay_all);
        
        free(delay_all);
    }
    else
    {
        vlc_memstream_printf(&ms, "1000:");
        msg_Warn(p_filter, "delay parameter not set. Set to default(1000).");
    }

    /* Checking decay Bounds   */
    if (decay != NULL)
    {
        char *decay_all = check_Bounds(p_filter, decay, 0, 1, 0.5);
        if(!decay_all)
            vlc_memstream_printf(&ms, "0.5");
        else
            vlc_memstream_printf(&ms, "%s", decay_all);
        
        free(decay_all);
    }
    else
    {
        vlc_memstream_printf(&ms, "0.5");
        msg_Warn(p_filter, "decay parameter not set. Set to default(0.5).");
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
    msg_Dbg(p_filter,"aecho initialised: %s",ms.ptr);

    free( ms.ptr );

    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

