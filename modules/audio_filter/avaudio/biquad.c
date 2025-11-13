/*****************************************************************************
 * biquad.c : 
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
    float a[3];
    float b[3];
    float mix;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_biquad( vlc_object_t *p_this );
static void Close_biquad( filter_t *p_filter );

static char* string_generate_biquad(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "biquad=a0=%f:a1=%f:a2=%f:b0=%f:b1=%f:"
            "b2=%f:m=%f", p_sys->a[0], p_sys->a[1], p_sys->a[2], p_sys->b[0], 
            p_sys->b[1], p_sys->b[2], p_sys->mix);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define BIQUAD_A0_TEXT N_( "a0" )
#define BIQUAD_A0_LONGTEXT N_( "a0, a1, a2 are the denominator coefficients." )

#define BIQUAD_A1_TEXT N_( "a1" )
#define BIQUAD_A1_LONGTEXT N_( "a0, a1, a2 are the denominator coefficients." )

#define BIQUAD_A2_TEXT N_( "a2" )
#define BIQUAD_A2_LONGTEXT N_( "a0, a1, a2 are the denominator coefficients." )

#define BIQUAD_B0_TEXT N_( "b0" )
#define BIQUAD_B0_LONGTEXT N_( "b0, b1, b2 are the numerator coefficients" ) 

#define BIQUAD_B1_TEXT N_( "b1" )
#define BIQUAD_B1_LONGTEXT N_( "b0, b1, b2 are the numerator coefficients" ) 

#define BIQUAD_B2_TEXT N_( "b2" )
#define BIQUAD_B2_LONGTEXT N_( "b0, b1, b2 are the numerator coefficients" ) 

#define BIQUAD_MIX_TEXT N_( "mix" )
#define BIQUAD_MIX_LONGTEXT N_( "Set mixing amount. Default is 1." \
                    " Range is [0 - 1]")

vlc_module_begin()

    set_shortname( N_("Biquad") )
    set_description( N_("Biquad filter") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "biquad-a0", 1.0, -50.0, 50.0,
            BIQUAD_A0_TEXT, BIQUAD_A0_LONGTEXT )

    add_float_with_range( "biquad-a1", 0.0, -50.0, 50.0,
            BIQUAD_A1_TEXT, BIQUAD_A1_LONGTEXT )

    add_float_with_range( "biquad-a2", 0.0, -50.0, 50.0,
            BIQUAD_A2_TEXT, BIQUAD_A2_LONGTEXT )
    
    add_float_with_range( "biquad-b0", 0.0, -50.0, 50.0,
            BIQUAD_B0_TEXT, BIQUAD_B0_LONGTEXT )

    add_float_with_range( "biquad-b1", 0.0, -50.0, 50.0,
            BIQUAD_B1_TEXT, BIQUAD_B1_LONGTEXT )

    add_float_with_range( "biquad-b2", 0.0, -50.0, 50.0,
            BIQUAD_B2_TEXT, BIQUAD_B2_LONGTEXT )

    add_float_with_range( "biquad-mix", 1, 0, 1,
            BIQUAD_MIX_TEXT, BIQUAD_MIX_LONGTEXT )

    add_shortcut("biquad")
    set_callback( Open_biquad )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_biquad( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_biquad( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "biquad-a0" ) )
    {
        p_sys->a[0] = newval.f_float;
        if(!p_sys->a[0])
            p_sys->a[0] = 1;
    }
    if( !strcmp( psz_cmd, "biquad-a1" ) )
        p_sys->a[1] = newval.f_float;
    if( !strcmp( psz_cmd, "biquad-a2" ) )
        p_sys->a[2] = newval.f_float;
    if( !strcmp( psz_cmd, "biquad-b0" ) )
        p_sys->b[0] = newval.f_float;
    if( !strcmp( psz_cmd, "biquad-b1" ) )
        p_sys->b[1] = newval.f_float;
    if( !strcmp( psz_cmd, "biquad-b2" ) )
        p_sys->b[2] = newval.f_float;
    if( !strcmp( psz_cmd, "biquad-mix" ) )
        p_sys->mix = newval.f_float;

    if(string_generate_biquad(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_biquad( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->a[0] = var_CreateGetFloat(p_aout,"biquad-a0");
    p_sys->a[1] = var_CreateGetFloat(p_aout,"biquad-a1");
    p_sys->a[2] = var_CreateGetFloat(p_aout,"biquad-a2");

    p_sys->b[0] = var_CreateGetFloat(p_aout,"biquad-b0");
    p_sys->b[1] = var_CreateGetFloat(p_aout,"biquad-b1");
    p_sys->b[2] = var_CreateGetFloat(p_aout,"biquad-b2");
    p_sys->mix = var_CreateGetFloat(p_aout,"biquad-mix");

    if(!p_sys->a[0])
        p_sys->a[0] = 1;

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "biquad-a0", paramCallback_biquad, p_sys );
    var_AddCallback( p_aout, "biquad-a1", paramCallback_biquad, p_sys );
    var_AddCallback( p_aout, "biquad-a2", paramCallback_biquad, p_sys );
    var_AddCallback( p_aout, "biquad-b0", paramCallback_biquad, p_sys );
    var_AddCallback( p_aout, "biquad-b1", paramCallback_biquad, p_sys );
    var_AddCallback( p_aout, "biquad-b2", paramCallback_biquad, p_sys );
    var_AddCallback( p_aout, "biquad-mix", paramCallback_biquad, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_biquad(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"biquad initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_biquad =
        { .filter_audio = Process_biquad, .close = Close_biquad };
    p_filter->ops = &filter_ops_biquad;

    return VLC_SUCCESS;
}

static void Close_biquad( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

