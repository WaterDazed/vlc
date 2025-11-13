/*****************************************************************************
 * astereowiden.c : 
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
    float feedback;
    float crossfeed;
    float drymix;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_astereowiden( vlc_object_t *p_this );
static void Close_astereowiden( filter_t *p_filter );

static char* string_generate_astereowiden(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "stereowiden=%f:%f:%f:%f", p_sys->delay, 
            p_sys->feedback, p_sys->crossfeed, p_sys->drymix);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ASTEREOWIDEN_DELAY_TEXT N_( "delay" )
#define ASTEREOWIDEN_DELAY_LONGTEXT N_( "Time in milliseconds of the delay of left signal into" \
                " right and vice versa. Default is 20 milliseconds." )

#define ASTEREOWIDEN_FEEDBACK_TEXT N_( "feedback" )
#define ASTEREOWIDEN_FEEDBACK_LONGTEXT N_( "Amount of gain in delayed signal into right and vice" \
                " versa. Gives a delay effect of left signal in right output and vice" \
                " versa which gives widening effect. Default is 0.3." )

#define ASTEREOWIDEN_CROSSFEED_TEXT N_( "crossfeed" )
#define ASTEREOWIDEN_CROSSFEED_LONGTEXT N_( "Cross feed of left into right with inverted phase." \
                " This helps in suppressing the mono. If the value is 1 it will cancel" \
                " all the signal common to both channels. Default is 0.3." )

#define ASTEREOWIDEN_DRYMIX_TEXT N_( "drymix" )
#define ASTEREOWIDEN_DRYMIX_LONGTEXT N_( "Set level of input signal of original channel." \
                " Default is 0.8." )

vlc_module_begin()

    set_shortname( N_("Astereowiden") )
    set_description( N_("Stereo widening effect") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "astereowiden-delay", 20.0, 1.0, 100.0,
            ASTEREOWIDEN_DELAY_TEXT, ASTEREOWIDEN_DELAY_LONGTEXT )

    add_float_with_range( "astereowiden-feedback", 0.3, 0.0, 0.9,
            ASTEREOWIDEN_FEEDBACK_TEXT, ASTEREOWIDEN_FEEDBACK_LONGTEXT )
    
    add_float_with_range( "astereowiden-crossfeed", 0.3, 0.0, 0.8,
            ASTEREOWIDEN_CROSSFEED_TEXT, ASTEREOWIDEN_CROSSFEED_LONGTEXT )

    add_float_with_range( "astereowiden-drymix", 0.8, 0.0, 1.0,
            ASTEREOWIDEN_DRYMIX_TEXT, ASTEREOWIDEN_DRYMIX_LONGTEXT )

    add_shortcut("astereowiden")
    set_callback( Open_astereowiden )
        
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_astereowiden( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 0;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_astereowiden( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "astereowiden-delay" ) )
        p_sys->delay = newval.f_float;
    if( !strcmp( psz_cmd, "astereowiden-feedback" ) )
        p_sys->feedback = newval.f_float;
    if( !strcmp( psz_cmd, "astereowiden-crossfeed" ) )
        p_sys->crossfeed = newval.f_float;
    if( !strcmp( psz_cmd, "astereowiden-drymix" ) )
        p_sys->drymix = newval.f_float;

    if(string_generate_astereowiden(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_astereowiden( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->delay = var_CreateGetFloat(p_aout,"astereowiden-delay");
    p_sys->feedback = var_CreateGetFloat(p_aout,"astereowiden-feedback");
    p_sys->crossfeed = var_CreateGetFloat(p_aout,"astereowiden-crossfeed");
    p_sys->drymix = var_CreateGetFloat(p_aout,"astereowiden-drymix");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "astereowiden-delay", paramCallback_astereowiden, p_sys );
    var_AddCallback( p_aout, "astereowiden-feedback", paramCallback_astereowiden, p_sys );
    var_AddCallback( p_aout, "astereowiden-crossfeed", paramCallback_astereowiden, p_sys );
    var_AddCallback( p_aout, "astereowiden-drymix", paramCallback_astereowiden, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_astereowiden(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"astereowiden initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_astereowiden =
        { .filter_audio = Process_astereowiden, .close = Close_astereowiden };
    p_filter->ops = &filter_ops_astereowiden;

    return VLC_SUCCESS;
}

static void Close_astereowiden( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

