/*****************************************************************************
 * crossfeed.c : 
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



typedef struct 
{
    float strength;
    float range;
    float slope;
    float level_in;
    float level_out;

    av_filter avf;
}avfilter_sys_t;


static int Open_crossfeed( vlc_object_t *p_this );
static void Close_crossfeed( filter_t *p_filter );

static char* string_generate_crossfeed(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "crossfeed=strength=%f:range=%f:%f:level_in=%f:"
            "level_out=%f", p_sys->strength, p_sys->range, p_sys->slope, p_sys->level_in,
             p_sys->level_out);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define CROSSFEED_STRENGTH_TEXT N_( "strength" )
#define CROSSFEED_STRENGTH_LONGTEXT N_( "Set strength of crossfeed. Default is 0.2." \
                    " Allowed range is from 0 to 1. This sets gain of low shelf filter" \
                    " for side part of stereo image. Default is -6dB. Max allowed is" \
                    " -30db when strength is set to 1." )

#define CROSSFEED_RANGE_TEXT N_( "range" )
#define CROSSFEED_RANGE_LONGTEXT N_( "Set soundstage wideness. Default is 0.5. Allowed" \
                    " range is from 0 to 1. This sets cut off frequency of low shelf" \
                    " filter. Default is cut off near 1550 Hz. With range set to 1 cut" \
                    " off frequency is set to 2100 Hz.")

#define CROSSFEED_SLOPE_TEXT N_( "slope" )
#define CROSSFEED_SLOPE_LONGTEXT N_( "Set curve slope of low shelf filter. Default is" \
                    " 0.5. Allowed range is from 0.01 to 1.")

#define CROSSFEED_LEVEL_IN_TEXT N_( "level_in" )
#define CROSSFEED_LEVEL_IN_LONGTEXT N_( "Set input gain. Default is 0.9." \
                    " Range is [0 - 1]" )

#define CROSSFEED_LEVEL_OUT_TEXT N_( "level_out" )
#define CROSSFEED_LEVEL_OUT_LONGTEXT N_( "Set output gain. Default is 1." \
                    " Range is [0 - 1]" )

vlc_module_begin()

    set_shortname( N_("Crossfeed") )
    set_description( N_("Headphone crossfeed.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )
    
    add_float_with_range( "crossfeed-strength", 0.2, 0, 1,
            CROSSFEED_STRENGTH_TEXT, CROSSFEED_STRENGTH_LONGTEXT )
    
    add_float_with_range( "crossfeed-range", 0.5, 0, 1,
            CROSSFEED_RANGE_TEXT, CROSSFEED_RANGE_LONGTEXT )
    
    add_float_with_range( "crossfeed-slope", 0.5, 0.01, 1,
            CROSSFEED_SLOPE_TEXT, CROSSFEED_SLOPE_LONGTEXT )

    add_float_with_range( "crossfeed-level_in", 0.9, 0, 1,
            CROSSFEED_LEVEL_IN_TEXT, CROSSFEED_LEVEL_IN_LONGTEXT )
    
    add_float_with_range( "crossfeed-level_out", 1, 0, 1,
            CROSSFEED_LEVEL_OUT_TEXT, CROSSFEED_LEVEL_OUT_LONGTEXT )

    add_shortcut("crossfeed")
    set_callback( Open_crossfeed )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_crossfeed( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_crossfeed( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "crossfeed-strength" ) )
        p_sys->strength = newval.f_float;
    if( !strcmp( psz_cmd, "crossfeed-range" ) )
        p_sys->range = newval.f_float;
    if( !strcmp( psz_cmd, "crossfeed-slope" ) )
        p_sys->slope = newval.f_float;
    if( !strcmp( psz_cmd, "crossfeed-level_in" ) )
        p_sys->level_in = newval.f_float;
    if( !strcmp( psz_cmd, "crossfeed-level_out" ) )
        p_sys->level_out = newval.f_float;

    if(string_generate_crossfeed(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);

    return VLC_SUCCESS;
}

static int Open_crossfeed( vlc_object_t *p_this )
{  
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;


    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->strength = var_CreateGetFloat(p_aout,"crossfeed-strength");
    p_sys->range = var_CreateGetFloat(p_aout,"crossfeed-range");
    p_sys->slope = var_CreateGetFloat(p_aout,"crossfeed-slope");
    p_sys->level_in = var_CreateGetFloat(p_aout,"crossfeed-level_in");
    p_sys->level_out = var_CreateGetFloat(p_aout,"crossfeed-level_out");
    
    p_sys->avf.params_changed = false;
    vlc_mutex_init(&p_sys->avf.lock);
    var_AddCallback( p_aout, "crossfeed-strength", paramCallback_crossfeed, p_sys );
    var_AddCallback( p_aout, "crossfeed-range", paramCallback_crossfeed, p_sys );
    var_AddCallback( p_aout, "crossfeed-slope", paramCallback_crossfeed, p_sys );
    var_AddCallback( p_aout, "crossfeed-level_in", paramCallback_crossfeed, p_sys );
    var_AddCallback( p_aout, "crossfeed-level_out", paramCallback_crossfeed, p_sys );


    p_filter->fmt_out.audio = p_filter->fmt_in.audio;

    /* initialize ffmpeg filter */
    if(init_filters(string_generate_crossfeed(p_sys), p_filter, &p_sys->avf.avfilter) <0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"crossfeed initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_crossfeed =
        { .filter_audio = Process_crossfeed, .close = Close_crossfeed };
    p_filter->ops = &filter_ops_crossfeed;

    return VLC_SUCCESS;
}

static void Close_crossfeed( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

