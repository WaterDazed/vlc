/*****************************************************************************
 * chorus.c : 
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


static int Open_chorus( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define CHORUS_IN_GAIN_TEXT N_( "in_gain" )
#define CHORUS_IN_GAIN_LONGTEXT N_( "Set the in_gain." )

#define CHORUS_OUT_GAIN_TEXT N_( "out_gain" )
#define CHORUS_OUT_GAIN_LONGTEXT N_( "Set the out_gain (reflected)." )

#define CHORUS_DELAY_TEXT N_( "delays" )
#define CHORUS_DELAY_LONGTEXT N_( "Set the delays in" \
                " milliseconds separated by '|'." \
                " Typical delay is arround 40ms to 60ms." \
                " Allowed " \
                " range for each delay is (0 - 1500.0]" )

#define CHORUS_DECAY_TEXT N_( "decays" )
#define CHORUS_DECAY_LONGTEXT N_( "Set deacays in" \
                " signals separated by '|'. Allowed range for" \
                " each decay is (0 - 1.0]. Default is 0.5." )

#define CHORUS_SPEED_TEXT N_( "speeds" )
#define CHORUS_SPEED_LONGTEXT N_( "Set speeds for" \
                " signals separated by '|'. Allowed range for" \
                " each decay is (0 - 1.0]. Default is 0.5." )

#define CHORUS_DEPTH_TEXT N_( "depths" )
#define CHORUS_DEPTH_LONGTEXT N_( "Set deacays in" \
                " signals separated by '|'. Allowed range for" \
                " each decay is (0 - 10]. Default is 2." )
    
vlc_module_begin()

    set_shortname( N_("Chorus") )
    set_description( N_("Chorus effect") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "chorus-in_gain", 0.5, 0.0, 1.0,
            CHORUS_IN_GAIN_TEXT, CHORUS_IN_GAIN_LONGTEXT )

    add_float_with_range( "chorus-out_gain", 0.5, 0.0, 1.0,
            CHORUS_OUT_GAIN_TEXT, CHORUS_OUT_GAIN_LONGTEXT )
    
    add_string( "chorus-delay", "20|40", CHORUS_DELAY_TEXT,
                CHORUS_DELAY_LONGTEXT )

    add_string( "chorus-decay", "0.4|0.6", CHORUS_DECAY_TEXT,
                CHORUS_DECAY_LONGTEXT )

    add_string( "chorus-speed", "1|0.5", CHORUS_SPEED_TEXT,
                CHORUS_SPEED_LONGTEXT )

    add_string( "chorus-depth", "0.5|0.1", CHORUS_DEPTH_TEXT,
                CHORUS_DEPTH_LONGTEXT )

    add_shortcut("chorus")
    set_callback( Open_chorus )
   
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static int Open_chorus( vlc_object_t *p_this )
{
    struct vlc_memstream ms;

    float in_gain;
    float out_gain;
    char *delay;
    char *decay;
    char *speed;
    char *depth;

    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;


    p_filter->p_sys = p_sys;
    
    in_gain = var_CreateGetFloat(p_filter,"chorus-in_gain");
    out_gain = var_CreateGetFloat(p_filter,"chorus-out_gain");
    delay = var_CreateGetString(p_filter,"chorus-delay");
    decay = var_CreateGetString(p_filter,"chorus-decay");
    speed = var_CreateGetString(p_filter,"chorus-speed");
    depth = var_CreateGetString(p_filter,"chorus-depth");
    

/** Configure filter parameters (filters_descr) **/
    if( vlc_memstream_open( &ms ) != 0 )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    vlc_memstream_printf(&ms, "chorus=%f:%f:", in_gain, out_gain);

    /* Checking delay Bounds */   
    if (delay != NULL)
    {
        char *delay_all = check_Bounds(p_filter, delay, 0, 1500, 1000);
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

    /* Checking decay Bounds */   
    if (decay != NULL)
    {
        char *decay_all = check_Bounds(p_filter, decay, 0, 1, 0.5);
        if(!decay_all)
            vlc_memstream_printf(&ms, "0.5:");
        else
            vlc_memstream_printf(&ms, "%s:", decay_all); 

        free(decay_all);
    }
    else
    {
        vlc_memstream_printf(&ms, "0.5:");
        msg_Warn(p_filter, "decay parameter not set. Set to default(0.5).");
    }
    
    /* Checking speed Bounds */
    if (speed != NULL)
    {
        char *speed_all = check_Bounds(p_filter, speed, 0, 1, 0.5);
        if(!speed_all)
            vlc_memstream_printf(&ms, "0.5:");
        else
            vlc_memstream_printf(&ms, "%s:", speed_all);

        free(speed_all);
    }
    else
    {
        vlc_memstream_printf(&ms, "0.5:");
        msg_Warn(p_filter, "speed parameter not set. Set to default(0.5).");
    }

    /* Checking depth Bounds */
    if (depth != NULL)
    {
        char *depth_all = check_Bounds(p_filter, depth, 0, 10, 2);
        if(!depth_all)
            vlc_memstream_printf(&ms, "2");
        else
            vlc_memstream_printf(&ms, "%s", depth_all);

        free(depth_all);
    }
    else
    {
        vlc_memstream_printf(&ms, "2");
        msg_Warn(p_filter, "depth parameter not set. Set to default(2).");
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
    msg_Dbg(p_filter,"chorus initialized: %s",ms.ptr);

    free( ms.ptr );
    
    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

