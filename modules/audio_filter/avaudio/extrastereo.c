/*****************************************************************************
 * extrastereo.c : 
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
    float m;
    bool c;

    av_filter avf;
}avfilter_sys_t;


static int Open_extrastereo( vlc_object_t *p_this );
static void Close_extrastereo( filter_t *p_filter );

static char* string_generate_extrastereo(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "extrastereo=m=%f:", p_sys->m);

    if(p_sys->c)
        vlc_memstream_printf(&p_sys->avf.str, "c=1");
    else
        vlc_memstream_printf(&p_sys->avf.str, "c=0");

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define EXTRASTEREO_M_TEXT N_( "difference coefficient" )
#define EXTRASTEREO_M_LONGTEXT N_( "Sets the difference coefficient" \
                " (default: 2.5). 0.0 means mono sound (average of both" \
                " channels), with 1.0 sound will be unchanged, with -1.0" \
                " left and right channels will be swapped." )

#define EXTRASTEREO_C_TEXT N_( "clipping" )
#define EXTRASTEREO_C_LONGTEXT N_( "Enable clipping. By default is enabled." )

vlc_module_begin()

    set_shortname( N_("Extrastereo") )
    set_description("Psuedo live effect")
    set_help( N_("Linearly increases the difference between left and right" \
                " channels which adds some sort of 'live' effect to playback.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )
    
    add_float_with_range( "extrastereo-m", 2.5, -10, 10,
            EXTRASTEREO_M_TEXT, EXTRASTEREO_M_LONGTEXT )
    
    add_bool( "extrastereo-c", true, EXTRASTEREO_C_TEXT,
            EXTRASTEREO_C_LONGTEXT )

    add_shortcut("extrastereo")
    set_callback( Open_extrastereo )

    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_extrastereo( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 0;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_extrastereo( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;
    
    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "extrastereo-m" ) )
        p_sys->m = newval.f_float;

    if(string_generate_extrastereo(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_extrastereo( vlc_object_t *p_this )
{  
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;


    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->m = var_CreateGetFloat(p_aout,"extrastereo-m");
    p_sys->c = var_CreateGetBool(p_aout,"extrastereo-c");
    
    p_sys->avf.params_changed = false;
    vlc_mutex_init(&p_sys->avf.lock);
    var_AddCallback( p_aout, "extrastereo-m", paramCallback_extrastereo, p_sys );


    p_filter->fmt_out.audio = p_filter->fmt_in.audio;

    /* initialize ffmpeg filter */
    if(init_filters(string_generate_extrastereo(p_sys), p_filter, &p_sys->avf.avfilter) <0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"extrastereo initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_extrastereo =
        { .filter_audio = Process_extrastereo, .close = Close_extrastereo };
    p_filter->ops = &filter_ops_extrastereo;

    return VLC_SUCCESS;
}

static void Close_extrastereo( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

