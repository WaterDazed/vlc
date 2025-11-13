/*****************************************************************************
 * flanger.c : 
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
    float delay;
    float depth;
    float regen;
    float width;
    float speed;
    char* shape;
    float phase;
    char* interp;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_flanger( vlc_object_t *p_this );
static void Close_flanger( filter_t *p_filter );

static char* string_generate_flanger(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "flanger=%f:%f:%f:%f:%f:%s:%f:%s", 
            p_sys->delay, p_sys->depth, p_sys->regen, p_sys->width, p_sys->speed, 
            p_sys->shape, p_sys->phase, p_sys->interp);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define FLANGER_DELAY_TEXT N_( "delay" )
#define FLANGER_DELAY_LONGTEXT N_( "Set base delay in milliseconds. Range from" \
                " 0 to 30. Default value is 0." )

#define FLANGER_DEPTH_TEXT N_( "depth" )
#define FLANGER_DEPTH_LONGTEXT N_( "Set added sweep delay in milliseconds. Range" \
                " from 0 to 10. Default value is 2." )

#define FLANGER_REGEN_TEXT N_( "regeneration" )
#define FLANGER_REGEN_LONGTEXT N_( "Set percentage regeneration (delayed signal" \
                " feedback). Range from -95 to 95. Default value is 0." )

#define FLANGER_WIDTH_TEXT N_( "width" )
#define FLANGER_WIDTH_LONGTEXT N_( "Set percentage of delayed signal mixed with" \
                " original. Range from 0 to 100. Default value is 71." )

#define FLANGER_SPEED_TEXT N_( "speed" )
#define FLANGER_SPEED_LONGTEXT N_( "Set sweeps per second (Hz). Range from 0.1" \
                " to 10. Default value is 0.5." )

#define FLANGER_SHAPE_TEXT N_( "shape" )
#define FLANGER_SHAPE_LONGTEXT N_( "Set swept wave shape, can be triangular or" \
                " sinusoidal. Default value is sinusoidal." )

#define FLANGER_PHASE_TEXT N_( "phase" )
#define FLANGER_PHASE_LONGTEXT N_( "Set swept wave percentage-shift for multi" \
                " channel. Range from 0 to 100. Default value is 25." )

#define FLANGER_INTERP_TEXT N_( "interp" )
#define FLANGER_INTERP_LONGTEXT N_( "Set delay-line interpolation, linear or" \
                " quadratic. Default is linear." )

vlc_module_begin()

    set_shortname( N_("Flanger") )
    set_description( N_("Flanging effect") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "flanger-delay", 0.0, 0.0, 30.0,
            FLANGER_DELAY_TEXT, FLANGER_DELAY_LONGTEXT )

    add_float_with_range( "flanger-depth", 2.0, 0.0, 10.0,
            FLANGER_DEPTH_TEXT, FLANGER_DEPTH_LONGTEXT )
    
    add_float_with_range( "flanger-regen", 0.0, -95.0, 95.0,
            FLANGER_REGEN_TEXT, FLANGER_REGEN_LONGTEXT )

    add_float_with_range( "flanger-width", 71.0, 0.0, 100.0,
            FLANGER_WIDTH_TEXT, FLANGER_WIDTH_LONGTEXT )

    add_float_with_range( "flanger-speed", 0.5, 0.1, 10.0,
            FLANGER_SPEED_TEXT, FLANGER_SPEED_LONGTEXT )
    
    add_string( "flanger-shape", "sinusoidal", FLANGER_SHAPE_TEXT,
            FLANGER_SHAPE_LONGTEXT )
        change_string_list( flanger_shape_list, flanger_shape_list_text )
    
    add_float_with_range( "flanger-phase", 25.0, 0.0, 100.0,
            FLANGER_PHASE_TEXT, FLANGER_PHASE_LONGTEXT )
    
    add_string( "flanger-interp", "linear", FLANGER_INTERP_TEXT,
            FLANGER_INTERP_LONGTEXT )
        change_string_list( flanger_interp_list, flanger_interp_list_text )

    add_shortcut("flanger")
    set_callback( Open_flanger )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_flanger( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_flanger( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "flanger-delay" ) )
        p_sys->delay = newval.f_float;
    if( !strcmp( psz_cmd, "flanger-depth" ) )
        p_sys->depth = newval.f_float;
    if( !strcmp( psz_cmd, "flanger-regen" ) )
        p_sys->regen = newval.f_float;
    if( !strcmp( psz_cmd, "flanger-width" ) )
        p_sys->width = newval.f_float;
    if( !strcmp( psz_cmd, "flanger-speed" ) )
        p_sys->speed = newval.f_float;
    if( !strcmp( psz_cmd, "flanger-shape" ) )
    {
        free(p_sys->shape);
        p_sys->shape = strdup(newval.psz_string);
        if(p_sys->shape == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "flanger-phase" ) )
        p_sys->phase = newval.f_float;
    if( !strcmp( psz_cmd, "flanger-interp" ) )
    {
        free(p_sys->interp);
        p_sys->interp = strdup(newval.psz_string);
        if(p_sys->interp == NULL)
            return VLC_EGENERIC;
    }

    if(string_generate_flanger(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_flanger( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->delay = var_CreateGetFloat(p_aout,"flanger-delay");
    p_sys->depth = var_CreateGetFloat(p_aout,"flanger-depth");
    p_sys->regen = var_CreateGetFloat(p_aout,"flanger-regen");
    p_sys->width = var_CreateGetFloat(p_aout,"flanger-width");
    p_sys->speed = var_CreateGetFloat(p_aout,"flanger-speed");
    p_sys->shape = var_CreateGetString(p_aout,"flanger-shape");
    p_sys->phase = var_CreateGetFloat(p_aout,"flanger-phase");
    p_sys->interp = var_CreateGetString(p_aout,"flanger-interp");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "flanger-delay", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-depth", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-regen", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-width", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-speed", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-shape", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-phase", paramCallback_flanger, p_sys );
    var_AddCallback( p_aout, "flanger-interp", paramCallback_flanger, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_flanger(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"flanger initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_flanger =
        { .filter_audio = Process_flanger, .close = Close_flanger };
    p_filter->ops = &filter_ops_flanger;

    return VLC_SUCCESS;
}

static void Close_flanger( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

