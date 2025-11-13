/*****************************************************************************
 * aemphasis.c : 
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
    float level_in;
    float level_out;
    char* mode;
    char* type;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_aemphasis( vlc_object_t *p_this );
static void Close_aemphasis( filter_t *p_filter );

static char* string_generate_aemphasis(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "aemphasis=%f:%f:%s:%s", p_sys->level_in, 
            p_sys->level_out, p_sys->mode, p_sys->type);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define AEMPHASIS_LEVEL_IN_TEXT N_( "level_in" )
#define AEMPHASIS_LEVEL_IN_LONGTEXT N_( "Set input gain. By default it is 1. Range is [0 - 64]." )

#define AEMPHASIS_LEVEL_OUT_TEXT N_( "level_out" )
#define AEMPHASIS_LEVEL_OUT_LONGTEXT N_( "Set output gain. By default it is 1. Range is [0 - 64]." )

#define AEMPHASIS_MODE_TEXT N_( "mode" )
#define AEMPHASIS_MODE_LONGTEXT N_( "Set filter mode. For restoring material use 'reproduction'" \
                    " mode, otherwise use 'production' mode. Default is 'reproduction' mode." )

#define AEMPHASIS_TYPE_TEXT N_( "type" )
#define AEMPHASIS_TYPE_LONGTEXT N_( "Set filter type. Selects medium." )

vlc_module_begin()

    set_section( N_("aemphasis"), NULL )
    set_shortname( N_("Aemphasis") )
    set_description( N_("Audio emphasis filter") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "aemphasis-level_in", 1, 0.0, 64,
            AEMPHASIS_LEVEL_IN_TEXT, AEMPHASIS_LEVEL_IN_LONGTEXT )

    add_float_with_range( "aemphasis-level_out", 1, 0.0, 64,
            AEMPHASIS_LEVEL_OUT_TEXT, AEMPHASIS_LEVEL_OUT_LONGTEXT )

    add_string( "aemphasis-mode", "reproduction", AEMPHASIS_MODE_TEXT,
                AEMPHASIS_MODE_LONGTEXT )
        change_string_list( aemphasis_mode_list, aemphasis_mode_list_text )

    add_string( "aemphasis-type", "cd", AEMPHASIS_TYPE_TEXT,
            AEMPHASIS_TYPE_LONGTEXT )
        change_string_list( aemphasis_type_list, aemphasis_type_list_text )

    add_shortcut( "aemphasis" )
    set_callback( Open_aemphasis )

vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_aemphasis( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_aemphasis( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "aemphasis-level_in" ) )
        p_sys->level_in = newval.f_float;
    if( !strcmp( psz_cmd, "aemphasis-level_out" ) )
        p_sys->level_out = newval.f_float;
    if( !strcmp( psz_cmd, "aemphasis-mode" ) )
    {
        free(p_sys->mode);
        p_sys->mode = strdup(newval.psz_string);
        if(p_sys->mode == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "aemphasis-type" ) )
    {
        free(p_sys->type);
        p_sys->type = strdup(newval.psz_string);
        if(p_sys->type == NULL)
            return VLC_EGENERIC;
    }

    if(string_generate_aemphasis(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_aemphasis( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->level_in = var_CreateGetFloat(p_aout,"aemphasis-level_in");
    p_sys->level_out = var_CreateGetFloat(p_aout,"aemphasis-level_out");
    p_sys->mode = var_CreateGetString(p_aout,"aemphasis-mode");
    p_sys->type = var_CreateGetString(p_aout,"aemphasis-type");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "aemphasis-level_in", paramCallback_aemphasis, p_sys );
    var_AddCallback( p_aout, "aemphasis-level_out", paramCallback_aemphasis, p_sys );
    var_AddCallback( p_aout, "aemphasis-mode", paramCallback_aemphasis, p_sys );
    var_AddCallback( p_aout, "aemphasis-type", paramCallback_aemphasis, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_aemphasis(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"aemphasis initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_aemphasis =
        { .filter_audio = Process_aemphasis, .close = Close_aemphasis };
    p_filter->ops = &filter_ops_aemphasis;

    return VLC_SUCCESS;
}

static void Close_aemphasis( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

