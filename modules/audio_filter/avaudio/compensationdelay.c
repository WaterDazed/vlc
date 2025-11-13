/*****************************************************************************
 * compensationdelay.c : 
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
    float mm;
    float cm;
    float m;
    float dry;
    float wet;
    float temp;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_compensationdelay( vlc_object_t *p_this );
static void Close_compensationdelay( filter_t *p_filter );

static char* string_generate_compensationdelay(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "compensationdelay=%f:%f:%f:%f:%f:%f", p_sys->mm, 
            p_sys->cm, p_sys->m, p_sys->dry, p_sys->wet, p_sys->temp);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define COMPENSATIONDELAY_MM_TEXT N_( "mm" )
#define COMPENSATIONDELAY_MM_LONGTEXT N_( "Set millimeters distance. This is compensation" \
                " distance for fine tuning. Default is 0." )

#define COMPENSATIONDELAY_CM_TEXT N_( "cm" )
#define COMPENSATIONDELAY_CM_LONGTEXT N_( "Set cm distance. This is compensation distance" \
                " for tightening distance setup. Default is 0." )

#define COMPENSATIONDELAY_M_TEXT N_( "m" )
#define COMPENSATIONDELAY_M_LONGTEXT N_( "Set meters distance. This is compensation distance" \
                " for hard distance setup. Default is 0." )

#define COMPENSATIONDELAY_DRY_TEXT N_( "dry" )
#define COMPENSATIONDELAY_DRY_LONGTEXT N_( "Set dry amount. Amount of unprocessed (dry)" \
                " signal. Default is 0." )

#define COMPENSATIONDELAY_WET_TEXT N_( "wet" )
#define COMPENSATIONDELAY_WET_LONGTEXT N_( "Set wet amount. Amount of processed (wet)" \
                " signal. Default is 1." )

#define COMPENSATIONDELAY_TEMP_TEXT N_( "temperature" )
#define COMPENSATIONDELAY_TEMP_LONGTEXT N_( "Set temperature in degrees Celsius. This is" \
                " the temperature of the environment. Default is 20." )

vlc_module_begin()

    set_shortname( N_("CompensationDelay") )
    set_description( N_("Delay to compensate differing positions of microphones or speakers.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "compensationdelay-mm", 0, 0, 10,
            COMPENSATIONDELAY_MM_TEXT, COMPENSATIONDELAY_MM_LONGTEXT )

    add_float_with_range( "compensationdelay-cm", 0, 0, 100,
            COMPENSATIONDELAY_CM_TEXT, COMPENSATIONDELAY_CM_LONGTEXT )
    
    add_float_with_range( "compensationdelay-m", 0, 0, 100,
            COMPENSATIONDELAY_M_TEXT, COMPENSATIONDELAY_M_LONGTEXT )

    add_float_with_range( "compensationdelay-dry", 0.0, 0.0, 1.0,
            COMPENSATIONDELAY_DRY_TEXT, COMPENSATIONDELAY_DRY_LONGTEXT )

    add_float_with_range( "compensationdelay-wet", 1.0, 0.0, 1.0,
            COMPENSATIONDELAY_WET_TEXT, COMPENSATIONDELAY_WET_LONGTEXT )

    add_float_with_range( "compensationdelay-temperature", 20, -50, 50,
            COMPENSATIONDELAY_TEMP_TEXT, COMPENSATIONDELAY_TEMP_LONGTEXT )

    add_shortcut("compensationdelay")
    set_callback( Open_compensationdelay )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_compensationdelay( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_compensationdelay( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "compensationdelay-mm" ) )
        p_sys->mm = newval.f_float;
    if( !strcmp( psz_cmd, "compensationdelay-cm" ) )
        p_sys->cm = newval.f_float;
    if( !strcmp( psz_cmd, "compensationdelay-m" ) )
        p_sys->m = newval.f_float;
    if( !strcmp( psz_cmd, "compensationdelay-dry" ) )
        p_sys->dry = newval.f_float;
    if( !strcmp( psz_cmd, "compensationdelay-wet" ) )
        p_sys->wet = newval.f_float;
    if( !strcmp( psz_cmd, "compensationdelay-temperature" ) )
        p_sys->temp = newval.f_float;

    if(string_generate_compensationdelay(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_compensationdelay( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->mm = var_CreateGetFloat(p_aout,"compensationdelay-mm");
    p_sys->cm = var_CreateGetFloat(p_aout,"compensationdelay-cm");
    p_sys->m = var_CreateGetFloat(p_aout,"compensationdelay-m");
    p_sys->dry = var_CreateGetFloat(p_aout,"compensationdelay-dry");
    p_sys->wet = var_CreateGetFloat(p_aout,"compensationdelay-wet");
    p_sys->temp = var_CreateGetFloat(p_aout,"compensationdelay-temperature");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "compensationdelay-mm", paramCallback_compensationdelay, p_sys );
    var_AddCallback( p_aout, "compensationdelay-cm", paramCallback_compensationdelay, p_sys );
    var_AddCallback( p_aout, "compensationdelay-m", paramCallback_compensationdelay, p_sys );
    var_AddCallback( p_aout, "compensationdelay-dry", paramCallback_compensationdelay, p_sys );
    var_AddCallback( p_aout, "compensationdelay-wet", paramCallback_compensationdelay, p_sys );
    var_AddCallback( p_aout, "compensationdelay-temperature", paramCallback_compensationdelay, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_compensationdelay(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"compensationdelay initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_compensationdelay =
        { .filter_audio = Process_compensationdelay, .close = Close_compensationdelay };
    p_filter->ops = &filter_ops_compensationdelay;

    return VLC_SUCCESS;
}

static void Close_compensationdelay( filter_t *p_filter )
{
    avfilter_sys_t *p_sys_compensatedelay = p_filter->p_sys;

    avfilter_graph_free(&p_sys_compensatedelay->avf.avfilter.filter_graph);
    free(p_sys_compensatedelay);
}

