/*****************************************************************************
 * alimiter.c : 
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
    float limit;
    float attack;
    float release;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_alimiter( vlc_object_t *p_this );
static void Close_alimiter( filter_t *p_filter );

static char* string_generate_alimiter(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "alimiter=%f:%f:%f:%f:%f", p_sys->level_in,
            p_sys->level_out, p_sys->limit, p_sys->attack, p_sys->release);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

//alimiter
#define ALIMITER_LEVEL_IN_TEXT N_( "level_in" )
#define ALIMITER_LEVEL_IN_LONGTEXT N_( "Set input gain. Default is 1." \
                    " Range is [0.015625 - 64]" )

#define ALIMITER_LEVEL_OUT_TEXT N_( "level_out" )
#define ALIMITER_LEVEL_OUT_LONGTEXT N_( "Set output gain. Default is 1." \
                    " Range is [0.015625 - 64]" )

#define ALIMITER_LIMIT_TEXT N_( "limit" )
#define ALIMITER_LIMIT_LONGTEXT N_( "Don’t let signals above this level pass the limiter." \
                    " Default is 0.0625. Range is [0- 1]" )

#define ALIMITER_ATTACK_TEXT N_( "attack" )
#define ALIMITER_ATTACK_LONGTEXT N_( "The limiter will reach its attenuation level in this" \
                    " amount of time in milliseconds. Default is 5 milliseconds." \
                    " Allowed range is from 0.1 to 80." )

#define ALIMITER_RELEASE_TEXT N_( "release" )
#define ALIMITER_RELEASE_LONGTEXT N_( "Come back from limiting to attenuation 1.0 in this" \
                    " amount of milliseconds. Default is 50 milliseconds." \
                    " Allowed range is from 1 to 8000." )

vlc_module_begin()

    set_shortname( N_("Alimiter") )
    set_description( N_("Alimiter") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "alimiter-level_in", 1, 0.015625, 50,
            ALIMITER_LEVEL_IN_TEXT, ALIMITER_LEVEL_IN_LONGTEXT )
    
    add_float_with_range( "alimiter-level_out", 1, 0.015625, 50,
            ALIMITER_LEVEL_OUT_TEXT, ALIMITER_LEVEL_OUT_LONGTEXT )
    
    add_float_with_range( "alimiter-limit", 1, 0.0625, 1,
            ALIMITER_LIMIT_TEXT, ALIMITER_LIMIT_LONGTEXT )
    
    add_float_with_range( "alimiter-attack", 5.0, 0.1, 80.0,
            ALIMITER_ATTACK_TEXT, ALIMITER_ATTACK_LONGTEXT )

    add_float_with_range( "alimiter-release", 50.0, 1.0, 8000.0,
            ALIMITER_RELEASE_TEXT, ALIMITER_RELEASE_LONGTEXT )
    
    add_shortcut("alimiter")
    set_callback( Open_alimiter )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_alimiter( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_alimiter( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "alimiter-level_in" ) )
        p_sys->level_in = newval.f_float;
    if( !strcmp( psz_cmd, "alimiter-level_out" ) )
        p_sys->level_out = newval.f_float;
    if( !strcmp( psz_cmd, "alimiter-limit" ) )
        p_sys->limit = newval.f_float;
    if( !strcmp( psz_cmd, "alimiter-attack" ) )
        p_sys->attack = newval.f_float;
    if( !strcmp( psz_cmd, "alimiter-release" ) )
        p_sys->release = newval.f_float;

    if(string_generate_alimiter(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_alimiter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->level_in = var_CreateGetFloat(p_aout,"alimiter-level_in");
    p_sys->level_out = var_CreateGetFloat(p_aout,"alimiter-level_out");
    p_sys->limit = var_CreateGetFloat(p_aout,"alimiter-limit");
    p_sys->attack = var_CreateGetFloat(p_aout,"alimiter-attack");
    p_sys->release = var_CreateGetFloat(p_aout,"alimiter-release");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "alimiter-level_in", paramCallback_alimiter, p_sys );
    var_AddCallback( p_aout, "alimiter-level_out", paramCallback_alimiter, p_sys );
    var_AddCallback( p_aout, "alimiter-limit", paramCallback_alimiter, p_sys );
    var_AddCallback( p_aout, "alimiter-attack", paramCallback_alimiter, p_sys );
    var_AddCallback( p_aout, "alimiter-release", paramCallback_alimiter, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_alimiter(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"alimiter initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_alimiter =
        { .filter_audio = Process_alimiter, .close = Close_alimiter };
    p_filter->ops = &filter_ops_alimiter;

    return VLC_SUCCESS;
}

static void Close_alimiter( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

