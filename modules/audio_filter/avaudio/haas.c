/*****************************************************************************
 * haas.c : 
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


static int Open_haas( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static const int haas_middle_src_vlclist[] = { 0, 1, 2, 3 };
static const char *const haas_middle_src_vlctext[] =
{
     "left",
     "right",
     "mid",
     "side",
};

#define HAAS_LEVEL_IN_TEXT N_( "level_in" )
#define HAAS_LEVEL_IN_LONGTEXT N_( "Set input level. By default is 1, or 0dB" )

#define HAAS_LEVEL_OUT_TEXT N_( "level_out" )
#define HAAS_LEVEL_OUT_LONGTEXT N_( "Set output level. By default is 1, or 0dB." )

#define HAAS_SIDE_GAIN_TEXT N_( "side_gain" )
#define HAAS_SIDE_GAIN_LONGTEXT N_( "Set gain applied to side part of signal." \
                    " By default is 1." )

#define HAAS_MIDDLE_SRC_TEXT N_( "middle_source" )
#define HAAS_MIDDLE_SRC_LONGTEXT N_( "Set kind of middle source." )

#define HAAS_MIDDLE_PHASE_TEXT N_( "middle_phase" )
#define HAAS_MIDDLE_PHASE_LONGTEXT N_( "Change middle phase. By default is disabled." )

#define HAAS_LEFT_DELAY_TEXT N_( "left_delay" )
#define HAAS_LEFT_DELAY_LONGTEXT N_( "Set left channel delay. By default is 2.05 milliseconds." )

#define HAAS_LEFT_BAL_TEXT N_( "left_balance" )
#define HAAS_LEFT_BAL_LONGTEXT N_( "Set left channel balance. By default is -1." )

#define HAAS_LEFT_GAIN_TEXT N_( "left_gain" )
#define HAAS_LEFT_GAIN_LONGTEXT N_( "Set left channel gain. By default is 1." )

#define HAAS_LEFT_PHASE_TEXT N_( "left_phase" )
#define HAAS_LEFT_PHASE_LONGTEXT N_( "Change left phase. By default is disabled." )

#define HAAS_RIGHT_DELAY_TEXT N_( "right_delay" )
#define HAAS_RIGHT_DELAY_LONGTEXT N_( "Set right channel delay. By default is 2.05 milliseconds." )

#define HAAS_RIGHT_BAL_TEXT N_( "right_balance" )
#define HAAS_RIGHT_BAL_LONGTEXT N_( "Set right channel balance. By default is -1." )

#define HAAS_RIGHT_GAIN_TEXT N_( "right_gain" )
#define HAAS_RIGHT_GAIN_LONGTEXT N_( "Set right channel gain. By default is 1." )

#define HAAS_RIGHT_PHASE_TEXT N_( "right_phase" )
#define HAAS_RIGHT_PHASE_LONGTEXT N_( "Change right phase. By default is disabled." )

vlc_module_begin()

    set_shortname( N_("Haas") )
    set_description( N_("Haas effect") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "level_in", 1, 0.015625, 64,
            HAAS_LEVEL_IN_TEXT, HAAS_LEVEL_IN_LONGTEXT )
    
    add_float_with_range( "level_out", 1, 0.015625, 64,
            HAAS_LEVEL_OUT_TEXT, HAAS_LEVEL_OUT_LONGTEXT )
    
    add_float_with_range( "side_gain", 1, 0.015625, 64,
            HAAS_SIDE_GAIN_TEXT, HAAS_SIDE_GAIN_LONGTEXT )
    
    add_integer( "middle_source", 2,
                HAAS_MIDDLE_SRC_TEXT, HAAS_MIDDLE_SRC_LONGTEXT )
        change_integer_list( haas_middle_src_vlclist,
                            haas_middle_src_vlctext )
    
    add_bool( "middle_phase", false, HAAS_MIDDLE_PHASE_TEXT,
                HAAS_MIDDLE_PHASE_LONGTEXT )
    
    add_float_with_range( "left_delay", 2.05, 0, 40,
            HAAS_LEFT_DELAY_TEXT, HAAS_LEFT_DELAY_LONGTEXT )
    
    add_float_with_range( "left_balance", -1, -1, 1,
            HAAS_LEFT_BAL_TEXT, HAAS_LEFT_BAL_LONGTEXT )
    
    add_float_with_range( "left_gain", 1, 0.015625, 64,
            HAAS_LEFT_GAIN_TEXT, HAAS_LEFT_GAIN_LONGTEXT )
    
    add_bool( "left_phase", false, HAAS_LEFT_PHASE_TEXT,
                HAAS_LEFT_PHASE_LONGTEXT )

    add_float_with_range( "right_delay", 2.12, 0, 40,
            HAAS_RIGHT_DELAY_TEXT, HAAS_RIGHT_DELAY_LONGTEXT )
    
    add_float_with_range( "right_balance", 1, -1, 1,
            HAAS_RIGHT_BAL_TEXT, HAAS_RIGHT_BAL_LONGTEXT )
    
    add_float_with_range( "right_gain", 1, 0.015625, 64,
            HAAS_RIGHT_GAIN_TEXT, HAAS_RIGHT_GAIN_LONGTEXT )
    
    add_bool( "right_phase", false, HAAS_RIGHT_PHASE_TEXT,
                HAAS_RIGHT_PHASE_LONGTEXT )

    add_shortcut("haas")
    set_callback( Open_haas )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static int Open_haas( vlc_object_t *p_this )
{
    struct vlc_memstream ms;

    float level_in;
    float level_out;
    float side_gain;
    int middle_src;
    bool mid_phase;
    float left_delay;
    float left_bal;
    float left_gain;
    bool left_phase;
    float right_delay;
    float right_bal;
    float right_gain;
    bool right_phase;

    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_filter->p_sys = p_sys;

    level_in = var_CreateGetFloat(p_filter,"level_in");
    level_out = var_CreateGetFloat(p_filter,"level_out");
    side_gain = var_CreateGetFloat(p_filter,"side_gain");
    middle_src = var_CreateGetInteger(p_filter,"middle_source");
    mid_phase = var_CreateGetBool(p_filter,"middle_phase");
    left_delay = var_CreateGetFloat(p_filter,"left_delay");
    left_bal = var_CreateGetFloat(p_filter,"left_balance");
    left_gain = var_CreateGetFloat(p_filter,"left_gain");
    left_phase = var_CreateGetBool(p_filter,"left_phase");
    right_delay = var_CreateGetFloat(p_filter,"right_delay");
    right_bal = var_CreateGetFloat(p_filter,"right_balance");
    right_gain = var_CreateGetFloat(p_filter,"right_gain");
    right_phase = var_CreateGetBool(p_filter,"right_phase");
    

/** Configure filter parameters (filters_descr) **/
    if( vlc_memstream_open( &ms ) != 0 )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }
    
    vlc_memstream_printf(&ms, "haas=%f:%f:%f:", level_in, level_out, side_gain);

    switch (middle_src)
    {
    case 0:
        vlc_memstream_printf(&ms, "left:");
        break;
    case 1:
        vlc_memstream_printf(&ms, "right:");
        break;
    case 2:
        vlc_memstream_printf(&ms, "mid:");

        break;
    case 3:
        vlc_memstream_printf(&ms, "side:");
        break;
    default:
        break;
    }
    
    vlc_memstream_printf(&ms, "middle_phase=");
    if(mid_phase)
        vlc_memstream_printf(&ms, "1:");
    else
        vlc_memstream_printf(&ms, "0:");

    vlc_memstream_printf(&ms, "left_delay=%f:left_balance=%f:left_gain=%f:"
            "left_phase=", left_delay, left_bal, left_gain);
    if(left_phase)
        vlc_memstream_printf(&ms, "1:");
    else
        vlc_memstream_printf(&ms, "0:");
    
    vlc_memstream_printf(&ms, "right_delay=%f:right_balance=%f:right_gain=%f:"
            "right_phase=", right_delay, right_bal, right_gain);
    if(right_phase)
        vlc_memstream_printf(&ms, "1");
    else
        vlc_memstream_printf(&ms, "0");
 

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
    msg_Dbg(p_filter,"hass initialized: %s",ms.ptr);

    free(ms.ptr);

    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

