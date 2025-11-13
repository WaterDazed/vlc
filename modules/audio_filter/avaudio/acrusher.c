/*****************************************************************************
 * acrusher.c : 
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
    float bits;
    float mix;
    float dc;
    float aa;
    float samples;

    av_filter avf;
}avfilter_sys_t;


static int Open_acrusher( vlc_object_t *p_this );
static void Close_acrusher( filter_t *p_filter );

static char* string_generate_acrusher(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "acrusher=%f:%f:"
        "bits=%f:mix=%f:dc=%f:aa=%f:samples=%f",
            p_sys->level_in, p_sys->level_out, p_sys->bits, 
            p_sys->mix, p_sys->dc, p_sys->aa, p_sys->samples);
    
    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;


    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ACRUSHER_LEVEL_IN_TEXT N_( "level_in" )
#define ACRUSHER_LEVEL_IN_LONGTEXT N_( "Set input level. Default is 1." \
                    " Range is [0.015625 - 64]" )

#define ACRUSHER_LEVEL_OUT_TEXT N_( "level_out" )
#define ACRUSHER_LEVEL_OUT_LONGTEXT N_( "Set output level. Default is 1." \
                    " Range is [0.015625 - 64]" )

#define ACRUSHER_BITS_TEXT N_( "bits" )
#define ACRUSHER_BITS_LONGTEXT N_( "Set bit reduction. Default is 8." \
                    " Range is [1 - 64]" )

#define ACRUSHER_MIX_TEXT N_( "mix" )
#define ACRUSHER_MIX_LONGTEXT N_( "Set mixing amount. Default is 0.5." \
                    " Range is [0 - 1]")

#define ACRUSHER_DC_TEXT N_( "dc" )
#define ACRUSHER_DC_LONGTEXT N_( "Set DC offeset. Default is 1." \
                    " Range is [0.25 - 4]")

#define ACRUSHER_AA_TEXT N_( "anti-aliasing " )
#define ACRUSHER_AA_LONGTEXT N_( "Set anti-aliasing. Default is 0.5." \
                    " Range is [0 - 1]" )

#define ACRUSHER_SAMPLES_TEXT N_( "sample reduction" )
#define ACRUSHER_SAMPLES_LONGTEXT N_( "Set sample reduction. Default is 1." \
                    " Range is [1 - 250]" )

vlc_module_begin()

    set_shortname( N_("Acrusher") )
    set_description( N_("Reduce audio bit resolution.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "acrusher-level_in", 1, 0.015625, 32,
            ACRUSHER_LEVEL_IN_TEXT, ACRUSHER_LEVEL_IN_LONGTEXT )

    add_float_with_range( "acrusher-level_out", 1, 0.015625, 64,
            ACRUSHER_LEVEL_OUT_TEXT, ACRUSHER_LEVEL_OUT_LONGTEXT )

    add_float_with_range( "acrusher-bits", 8, 1, 64,
            ACRUSHER_BITS_TEXT, ACRUSHER_BITS_LONGTEXT )

    add_float_with_range( "acrusher-mix", 0.5, 0, 1,
            ACRUSHER_MIX_TEXT, ACRUSHER_MIX_LONGTEXT )

    add_float_with_range( "acrusher-dc", 1, 0.25, 4,
            ACRUSHER_DC_TEXT, ACRUSHER_DC_LONGTEXT )

    add_float_with_range( "acrusher-aa", 0.5, 0, 1,
            ACRUSHER_AA_TEXT, ACRUSHER_AA_LONGTEXT )

    add_float_with_range( "acrusher-samples", 1, 1, 250,
            ACRUSHER_SAMPLES_TEXT, ACRUSHER_SAMPLES_LONGTEXT )

    add_shortcut("acrusher")
    set_callback( Open_acrusher )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_acrusher( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_acrusher( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "acrusher-level_in" ) )
        p_sys->level_in = newval.f_float;
    if( !strcmp( psz_cmd, "acrusher-level_out" ) )
        p_sys->level_out = newval.f_float;
    if( !strcmp( psz_cmd, "acrusher-bits" ) )
        p_sys->bits = newval.f_float;
    if( !strcmp( psz_cmd, "acrusher-mix" ) )
        p_sys->mix = newval.f_float;
    if( !strcmp( psz_cmd, "acrusher-dc" ) )
        p_sys->dc = newval.f_float;
    if( !strcmp( psz_cmd, "acrusher-aa" ) )
        p_sys->aa = newval.f_float;
    if( !strcmp( psz_cmd, "acrusher-samples" ) )
        p_sys->samples = newval.f_float;
    
    if(string_generate_acrusher(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;
    
    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_acrusher( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );

    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->level_in = var_CreateGetFloat(p_aout,"acrusher-level_in");
    p_sys->level_out = var_CreateGetFloat(p_aout,"acrusher-level_out");
    p_sys->bits = var_CreateGetFloat(p_aout,"acrusher-bits");
    p_sys->mix = var_CreateGetFloat(p_aout,"acrusher-mix");
    p_sys->dc = var_CreateGetFloat(p_aout,"acrusher-dc");
    p_sys->aa = var_CreateGetFloat(p_aout,"acrusher-aa");
    p_sys->samples = var_CreateGetFloat(p_aout,"acrusher-samples");
    

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "acrusher-level_in", paramCallback_acrusher, p_sys );
    var_AddCallback( p_aout, "acrusher-level_out", paramCallback_acrusher, p_sys );
    var_AddCallback( p_aout, "acrusher-bits", paramCallback_acrusher, p_sys );
    var_AddCallback( p_aout, "acrusher-mix", paramCallback_acrusher, p_sys );
    var_AddCallback( p_aout, "acrusher-dc", paramCallback_acrusher, p_sys );
    var_AddCallback( p_aout, "acrusher-aa", paramCallback_acrusher, p_sys );
    var_AddCallback( p_aout, "acrusher-samples", paramCallback_acrusher, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    
    /* initialize ffmpeg filter */
    if(init_filters(string_generate_acrusher(p_sys), p_filter, &p_sys->avf.avfilter) <0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"acrusher initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);
    
    static const struct vlc_filter_operations filter_ops_acrusher =
        { .filter_audio = Process_acrusher, .close = Close_acrusher };
    p_filter->ops = &filter_ops_acrusher;

    return VLC_SUCCESS;
}

static void Close_acrusher( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

