/*****************************************************************************
 * deesser.c : 
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
    float intensity;
    float m;
    float freq;
    char* mode;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_deesser( vlc_object_t *p_this );
static void Close_deesser( filter_t *p_filter );

static char* string_generate_deesser(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "deesser=i=%f:m=%f:f=%f:s=%s", p_sys->intensity, 
            p_sys->m, p_sys->freq, p_sys->mode);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define DEESSER_INTENSITY_TEXT N_( "i" )
#define DEESSER_INTENSITY_LONGTEXT N_( "Set intensity for triggering de-essing." \
                " Allowed range is from 0 to 1. Default is 0." )

#define DEESSER_M_TEXT N_( "m" )
#define DEESSER_M_LONGTEXT N_( "Set amount of ducking on treble part of sound." \
                " Allowed range is from 0 to 1. Default is 0.5." )

#define DEESSER_F_TEXT N_( "f" )
#define DEESSER_F_LONGTEXT N_( "How much of original frequency content to keep" \
                " when de-essing. Allowed range is from 0 to 1. Default is 0.5." )

#define DEESSER_MODE_TEXT N_( "output mode" )
#define DEESSER_MODE_LONGTEXT N_( "Set the output mode." )

vlc_module_begin()
    
    set_shortname( N_("DeEsser") )
    set_description( N_("Audio de-essing") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "deesser-i", 0, 0.0, 1.0,
            DEESSER_INTENSITY_TEXT, DEESSER_INTENSITY_LONGTEXT )

    add_float_with_range( "deesser-m", 0.5, 0.0, 1.0,
            DEESSER_M_TEXT, DEESSER_M_LONGTEXT )
    
    add_float_with_range( "deesser-f", 0.5, 0.0, 1.0,
            DEESSER_F_TEXT, DEESSER_F_LONGTEXT )

    add_string( "deesser-mode", "o", DEESSER_MODE_TEXT,
                DEESSER_MODE_LONGTEXT )
        change_string_list( deesser_mode_list, deesser_mode_list_text )

    add_shortcut( "deesser" )
    set_callback( Open_deesser )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_deesser( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_deesser( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "deesser-i" ) )
        p_sys->intensity = newval.f_float;
    if( !strcmp( psz_cmd, "deesser-m" ) )
        p_sys->m = newval.f_float;
    if( !strcmp( psz_cmd, "deesser-f" ) )
        p_sys->freq = newval.f_float;
    if( !strcmp( psz_cmd, "deesser-mode" ) )
    {
        free(p_sys->mode);
        p_sys->mode = strdup(newval.psz_string);
        if(p_sys->mode == NULL)
            return VLC_EGENERIC;
    }

    if(string_generate_deesser(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_deesser( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->intensity = var_CreateGetFloat(p_aout,"deesser-i");
    p_sys->m = var_CreateGetFloat(p_aout,"deesser-m");
    p_sys->freq = var_CreateGetFloat(p_aout,"deesser-f");
    p_sys->mode = var_CreateGetString(p_aout,"deesser-mode");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "deesser-i", paramCallback_deesser, p_sys );
    var_AddCallback( p_aout, "deesser-m", paramCallback_deesser, p_sys );
    var_AddCallback( p_aout, "deesser-f", paramCallback_deesser, p_sys );
    var_AddCallback( p_aout, "deesser-mode", paramCallback_deesser, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_deesser(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"deesser initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_deesser =
        { .filter_audio = Process_deesser, .close = Close_deesser };
    p_filter->ops = &filter_ops_deesser;

    return VLC_SUCCESS;
}

static void Close_deesser( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

