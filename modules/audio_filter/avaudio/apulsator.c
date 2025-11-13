/*****************************************************************************
 * apulsator.c : 
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
    float amount;
    float offset_l;
    float offset_r;
    float width;
    char* timing;
    float bpm;
    float ms;
    float hz;

    av_filter avf;
}avfilter_sys_t;


static int Open_apulsator( vlc_object_t *p_this );
static void Close_apulsator( filter_t *p_filter );

static char* string_generate_apulsator(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;
    
    vlc_memstream_printf(&p_sys->avf.str, "apulsator=%f:%f:%s:%f:%f:%f:%f:%s=", 
            p_sys->level_in, p_sys->level_out, p_sys->mode, p_sys->amount, 
            p_sys->offset_l, p_sys->offset_r, p_sys->width, p_sys->timing);

    if (!strcmp(p_sys->timing, "bpm"))
        vlc_memstream_printf(&p_sys->avf.str, "%f", p_sys->bpm);
    else if (!strcmp(p_sys->timing, "ms"))
        vlc_memstream_printf(&p_sys->avf.str, "%f", p_sys->ms);
    else
        vlc_memstream_printf(&p_sys->avf.str, "%f", p_sys->hz);


    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define APULSATOR_LEVEL_IN_TEXT N_( "level_in" )
#define APULSATOR_LEVEL_IN_LONGTEXT N_( "Set input gain. By default it is 1. Range is [0.015625 - 64]." )

#define APULSATOR_LEVEL_OUT_TEXT N_( "level_out" )
#define APULSATOR_LEVEL_OUT_LONGTEXT N_( "Set output gain. By default it is 1. Range is [0.015625 - 64]." )

#define APULSATOR_MODE_TEXT N_( "mode" )
#define APULSATOR_MODE_LONGTEXT N_( "Set waveform shape the LFO will use. Default is sine." )

#define APULSATOR_AMOUNT_TEXT N_( "amount" )
#define APULSATOR_AMOUNT_LONGTEXT N_( "Set modulation. Define how much of original signal is affected by the LFO." )

#define APULSATOR_OFFSET_L_TEXT N_( "offset_l" )
#define APULSATOR_OFFSET_L_LONGTEXT N_( "Set left channel offset. Default is 0. Allowed range is [0 - 1]." )

#define APULSATOR_OFFSET_R_TEXT N_( "offset_r" )
#define APULSATOR_OFFSET_R_LONGTEXT N_( "Set right channel offset. Default is 0.5. Allowed range is [0 - 1]." )

#define APULSATOR_WIDTH_TEXT N_( "width" )
#define APULSATOR_WIDTH_LONGTEXT N_( "Set pulse width. Default is 1. Allowed range is [0 - 2]." )

#define APULSATOR_TIMING_TEXT N_( "timing" )
#define APULSATOR_TIMING_LONGTEXT N_( "Set possible timing mode. Default is hz." )

#define APULSATOR_BPM_TEXT N_( "bpm" )
#define APULSATOR_BPM_LONGTEXT N_( "Set bpm. Default is 120. Allowed range is [30 - 300]." \
                " Only used if timing is set to bpm." )

#define APULSATOR_MS_TEXT N_( "ms" )
#define APULSATOR_MS_LONGTEXT N_( "Set ms. Default is 500. Allowed range is [10 - 2000]." \
                " Only used if timing is set to ms." )

#define APULSATOR_HZ_TEXT N_( "hz" )
#define APULSATOR_HZ_LONGTEXT N_( "Set frequency in Hz. Default is 2. Allowed range is [0.01 - 100]." \
                " Only used if timing is set to hz." )

vlc_module_begin()

    set_shortname( N_("Apulsator") )
    set_description( N_("Pulsator") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "apulsator-level_in", 1, 0.015625, 32,
            APULSATOR_LEVEL_IN_TEXT, APULSATOR_LEVEL_IN_LONGTEXT )

    add_float_with_range( "apulsator-level_out", 1, 0.015625, 32,
            APULSATOR_LEVEL_OUT_TEXT, APULSATOR_LEVEL_OUT_LONGTEXT )

    add_string( "apulsator-mode", "sine", APULSATOR_MODE_TEXT,
                APULSATOR_MODE_LONGTEXT )
        change_string_list( apulsator_mode_list, apulsator_mode_list_text )

    add_float_with_range( "apulsator-amount", 1, 0, 1,
            APULSATOR_AMOUNT_TEXT, APULSATOR_AMOUNT_LONGTEXT )

    add_float_with_range( "apulsator-offset_l", 0, 0, 1,
            APULSATOR_OFFSET_L_TEXT, APULSATOR_OFFSET_L_LONGTEXT )

    add_float_with_range( "apulsator-offset_r", 0.5, 0, 1,
            APULSATOR_OFFSET_R_TEXT, APULSATOR_OFFSET_R_LONGTEXT )

    add_float_with_range( "apulsator-width", 1, 0, 2,
            APULSATOR_WIDTH_TEXT, APULSATOR_WIDTH_LONGTEXT )

    add_string( "apulsator-timing", "hz", APULSATOR_TIMING_TEXT,
            APULSATOR_TIMING_LONGTEXT )
        change_string_list( apulsator_timing_list, apulsator_timing_list_text )

    add_float_with_range( "apulsator-bpm", 120, 30, 300,
            APULSATOR_BPM_TEXT, APULSATOR_BPM_LONGTEXT )

    add_float_with_range( "apulsator-ms", 500, 10, 2000,
            APULSATOR_MS_TEXT, APULSATOR_MS_LONGTEXT )

    add_float_with_range( "apulsator-hz", 2, 0.1, 100,
            APULSATOR_HZ_TEXT, APULSATOR_HZ_LONGTEXT )

    add_shortcut( "apulsator" )
    set_callback( Open_apulsator )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_apulsator( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_apulsator( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "apulsator-level_in" ) )
        p_sys->level_in = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-level_out" ) )
        p_sys->level_out = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-mode" ) )
    {
        free(p_sys->mode);
        p_sys->mode = strdup(newval.psz_string);
        if(p_sys->mode == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "apulsator-amount" ) )
        p_sys->amount = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-offset_l" ) )
        p_sys->offset_l = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-offset_r" ) )
        p_sys->offset_r = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-width" ) )
        p_sys->width = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-timing" ) )
    {
        free(p_sys->timing);
        p_sys->timing = strdup(newval.psz_string);
        if(p_sys->timing == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "apulsator-bpm" ) )
        p_sys->bpm = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-ms" ) )
        p_sys->ms = newval.f_float;
    if( !strcmp( psz_cmd, "apulsator-hz" ) )
        p_sys->hz = newval.f_float;

    if(string_generate_apulsator(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_apulsator( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );

    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;


    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->level_in = var_CreateGetFloat(p_aout,"apulsator-level_in");
    p_sys->level_out = var_CreateGetFloat(p_aout,"apulsator-level_out");
    p_sys->mode = var_CreateGetString(p_aout,"apulsator-mode");
    p_sys->amount = var_CreateGetFloat(p_aout,"apulsator-amount");
    p_sys->offset_l = var_CreateGetFloat(p_aout,"apulsator-offset_l");
    p_sys->offset_r = var_CreateGetFloat(p_aout,"apulsator-offset_r");
    p_sys->width = var_CreateGetFloat(p_aout,"apulsator-width");
    p_sys->timing = var_CreateGetString(p_aout,"apulsator-timing");
    p_sys->bpm = var_CreateGetFloat(p_aout,"apulsator-bpm");
    p_sys->ms = var_CreateGetFloat(p_aout,"apulsator-ms");
    p_sys->hz = var_CreateGetFloat(p_aout,"apulsator-hz");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "apulsator-level_in", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-level_out", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-mode", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-amount", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-offset_l", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-offset_r", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-width", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-timing", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-bpm", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-ms", paramCallback_apulsator, p_sys );
    var_AddCallback( p_aout, "apulsator-hz", paramCallback_apulsator, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    
    
    /* initialize ffmpeg filter */
    if(init_filters(string_generate_apulsator(p_sys), p_filter, &p_sys->avf.avfilter) <0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"apulsator initialized: %s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_apulsator =
        { .filter_audio = Process_apulsator, .close = Close_apulsator };
    p_filter->ops = &filter_ops_apulsator;

    return VLC_SUCCESS;
}

static void Close_apulsator( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

