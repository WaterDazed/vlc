/*****************************************************************************
 * aphaser.c : 
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
    float in_gain;
    float out_gain;
    float delay;
    float decay;
    float speed;
    
    av_filter avf;
 }avfilter_sys_t;


static int Open_aphaser( vlc_object_t *p_this );
static void Close_aphaser( filter_t *p_filter );

static char* string_generate_aphaser(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "aphaser=%f:%f:%f:%f:%f", p_sys->in_gain, 
            p_sys->out_gain, p_sys->delay, p_sys->decay, p_sys->speed);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define APHASER_IN_GAIN_TEXT N_( "in_gain" )
#define APHASER_IN_GAIN_LONGTEXT N_( "Set the in_gain. Default is 0.4." )

#define APHASER_OUT_GAIN_TEXT N_( "out_gain" )
#define APHASER_OUT_GAIN_LONGTEXT N_( "Set the out_gain. Default is 0.74." )

#define APHASER_DELAY_TEXT N_( "delay" )
#define APHASER_DELAY_LONGTEXT N_( "Set delay in milliseconds. Default is 3.0." )

#define APHASER_DECAY_TEXT N_( "decay" )
#define APHASER_DECAY_LONGTEXT N_( "Set decay. Default is 0.4." )

#define APHASER_SPEED_TEXT N_( "speed" )
#define APHASER_SPEED_LONGTEXT N_( "Set modulation speed in Hz. Default is 0.5." )

vlc_module_begin()

    set_shortname( N_("Aphaser") )
    set_description( N_("Phasing effect") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "aphaser-in_gain", 0.4, 0.0, 1.0,
            APHASER_IN_GAIN_TEXT, APHASER_IN_GAIN_LONGTEXT )

    add_float_with_range( "aphaser-out_gain", 0.74, 0.0, 60.0,
            APHASER_OUT_GAIN_TEXT, APHASER_OUT_GAIN_LONGTEXT )
    
    add_float_with_range( "aphaser-delay", 3, 0.1, 5,
            APHASER_DELAY_TEXT, APHASER_DELAY_LONGTEXT )

    add_float_with_range( "aphaser-decay", 0.4, 0.0, 0.99,
            APHASER_DECAY_TEXT, APHASER_DECAY_LONGTEXT )

    add_float_with_range( "aphaser-speed", 0.5, 0.1, 2,
            APHASER_SPEED_TEXT, APHASER_SPEED_LONGTEXT )

    add_shortcut("aphaser")
    set_callback( Open_aphaser )

    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_aphaser( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 0;
    return Process_avaudio(p_filter,p_block,&p_sys->avf.avfilter,p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_aphaser( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "aphaser-in_gain" ) )
        p_sys->in_gain = newval.f_float;
    if( !strcmp( psz_cmd, "aphaser-out_gain" ) )
        p_sys->out_gain = newval.f_float;
    if( !strcmp( psz_cmd, "aphaser-delay" ) )
        p_sys->delay = newval.f_float;
    if( !strcmp( psz_cmd, "aphaser-decay" ) )
        p_sys->decay = newval.f_float;
    if( !strcmp( psz_cmd, "aphaser-speed" ) )
        p_sys->speed = newval.f_float;

    if(string_generate_aphaser(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_aphaser( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->in_gain = var_CreateGetFloat(p_aout,"aphaser-in_gain");
    p_sys->out_gain = var_CreateGetFloat(p_aout,"aphaser-out_gain");
    p_sys->delay = var_CreateGetFloat(p_aout,"aphaser-delay");
    p_sys->decay = var_CreateGetFloat(p_aout,"aphaser-decay");
    p_sys->speed = var_CreateGetFloat(p_aout,"aphaser-speed");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "aphaser-in_gain", paramCallback_aphaser, p_sys );
    var_AddCallback( p_aout, "aphaser-out_gain", paramCallback_aphaser, p_sys );
    var_AddCallback( p_aout, "aphaser-delay", paramCallback_aphaser, p_sys );
    var_AddCallback( p_aout, "aphaser-decay", paramCallback_aphaser, p_sys );
    var_AddCallback( p_aout, "aphaser-speed", paramCallback_aphaser, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_aphaser(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"aphaser initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_aphaser =
        { .filter_audio = Process_aphaser, .close = Close_aphaser };
    p_filter->ops = &filter_ops_aphaser;

    return VLC_SUCCESS;
}

static void Close_aphaser( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

