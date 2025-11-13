/*****************************************************************************
 * agate.c : 
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
    float range;
    float threshold;
    float ratio;
    float attack;
    float release;
    float makeup;
    float knee;
    char* detection;
    char* link;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_agate( vlc_object_t *p_this );
static void Close_agate( filter_t *p_filter );                

static char* string_generate_agate(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "agate=%f:range=%f:threshold=%f:"
            "ratio=%f:attack=%f:release=%f:makeup=%f:knee=%f:detection=%s:link=%s",
             p_sys->level_in, p_sys->range, p_sys->threshold, p_sys->ratio, 
             p_sys->attack, p_sys->release, p_sys->makeup, p_sys->knee, p_sys->detection, 
             p_sys->link);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define AGATE_LEVEL_IN_TEXT N_( "level_in" )
#define AGATE_LEVEL_IN_LONGTEXT N_( "Set input level before filtering. Default is 1." \
                    " Allowed range is from 0.015625 to 64." )

#define AGATE_RANGE_TEXT N_( "range" )
#define AGATE_RANGE_LONGTEXT N_( "Set the level of gain reduction when the signal" \
                    " is below the threshold. Default is 0.06125. Allowed range is" \
                    " from 0 to 1. Setting this to 0 disables reduction and then" \
                    " filter behaves like expander." )

#define AGATE_THRESHOLD_TEXT N_( "threshold" )
#define AGATE_THRESHOLD_LONGTEXT N_( "If a signal rises above this level the gain" \
                    " reduction is released. Default is 0.125. Allowed range is" \
                    " between 0 to 1." )

#define AGATE_RATIO_TEXT N_( "ratio" )
#define AGATE_RATIO_LONGTEXT N_( "Set a ratio by which the signal is reduced." \
                    " Default is 2. Allowed range is from 1 to 9000." )

#define AGATE_ATTACK_TEXT N_( "attack" )
#define AGATE_ATTACK_LONGTEXT N_( "Amount of milliseconds the signal has to rise" \
                    " above the threshold before gain reduction stops. Default is" \
                    " 20 milliseconds. Allowed range is from 0.01 to 9000." )

#define AGATE_RELEASE_TEXT N_( "release" )
#define AGATE_RELEASE_LONGTEXT N_( "Amount of milliseconds the signal has to fall" \
                    " below the threshold before the reduction is increased again." \
                    " Default is 250 milliseconds. Allowed range is from 0.01 to 9000." )

#define AGATE_MAKEUP_TEXT N_( "makeup" )
#define AGATE_MAKEUP_LONGTEXT N_( "Set amount of amplification of signal after" \
                    " processing. Default is 1. Allowed range is from 1 to 64." )

#define AGATE_KNEE_TEXT N_( "knee" )
#define AGATE_KNEE_LONGTEXT N_( "Curve the sharp knee around the threshold to enter" \
                    " gain reduction more softly. Default is 2.82. " \
                    " Allowed range is from 1 to 8." )

#define AGATE_DETECTION_TEXT N_( "detection" )
#define AGATE_DETECTION_LONGTEXT N_( "Choose if exact signal should be taken for" \
                    " detection or an RMS like one. Default is rms. Can be peak or rms." )

#define AGATE_LINK_TEXT N_( "link" )
#define AGATE_LINK_LONGTEXT N_( "Choose if the average level between all channels or the" \
                    " louder channel affects the reduction. Default is average." \
                    " Can be average or maximum." )

vlc_module_begin()

    set_shortname( N_("Agate") )
    set_description( N_("Gating") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "agate-level_in", 1, 0.015625, 48.0,
            AGATE_LEVEL_IN_TEXT, AGATE_LEVEL_IN_LONGTEXT )

    add_float_with_range( "agate-range", 0.06125, 0.0, 1.0,
            AGATE_RANGE_TEXT, AGATE_RANGE_LONGTEXT )
    
    add_float_with_range( "agate-threshold", 0.125, 0.0, 1.0,
            AGATE_THRESHOLD_TEXT, AGATE_THRESHOLD_LONGTEXT )

    add_float_with_range( "agate-ratio", 2.0, 1.0, 9000.0,
            AGATE_RATIO_TEXT, AGATE_RATIO_LONGTEXT )

    add_float_with_range( "agate-attack", 20.0, 0.01, 9000.0,
            AGATE_ATTACK_TEXT, AGATE_ATTACK_LONGTEXT )

    add_float_with_range( "agate-release", 250.0, 0.01, 9000.0,
            AGATE_RELEASE_TEXT, AGATE_RELEASE_LONGTEXT )

    add_float_with_range( "agate-makeup", 1.0, 1.0, 64.0,
            AGATE_MAKEUP_TEXT, AGATE_MAKEUP_LONGTEXT )

    add_float_with_range( "agate-knee", 2.82, 1.0, 8.0,
            AGATE_KNEE_TEXT, AGATE_KNEE_LONGTEXT )
    
    add_string( "agate-detection", "rms", AGATE_DETECTION_TEXT,
                AGATE_DETECTION_LONGTEXT )
        change_string_list( agate_detection_list, agate_detection_list_text )

    add_string( "agate-link", "average", AGATE_LINK_TEXT,
            AGATE_LINK_LONGTEXT )
        change_string_list( agate_link_list, agate_link_list_text )

    add_shortcut("agate")
    set_callback( Open_agate )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_agate( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_agate( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "agate-level_in" ) )
        p_sys->level_in = newval.f_float;
    if( !strcmp( psz_cmd, "agate-range" ) )
        p_sys->range = newval.f_float;
    if( !strcmp( psz_cmd, "agate-threshold" ) )
        p_sys->threshold = newval.f_float;
    if( !strcmp( psz_cmd, "agate-ratio" ) )
        p_sys->ratio = newval.f_float;
    if( !strcmp( psz_cmd, "agate-attack" ) )
        p_sys->attack = newval.f_float;
    if( !strcmp( psz_cmd, "agate-release" ) )
        p_sys->release = newval.f_float;
    if( !strcmp( psz_cmd, "agate-makeup" ) )
        p_sys->makeup = newval.f_float;
    if( !strcmp( psz_cmd, "agate-knee" ) )
        p_sys->knee = newval.f_float;
    if( !strcmp( psz_cmd, "agate-detection" ) )
    {
        free(p_sys->detection);
        p_sys->detection = strdup(newval.psz_string);
        if(p_sys->detection == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "agate-link" ) )
    {
        free(p_sys->link);
        p_sys->link = strdup(newval.psz_string);
        if(p_sys->link == NULL)
            return VLC_EGENERIC;
    }

    if(string_generate_agate(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_agate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->level_in = var_CreateGetFloat(p_aout,"agate-level_in");
    p_sys->range = var_CreateGetFloat(p_aout,"agate-range");
    p_sys->threshold = var_CreateGetFloat(p_aout,"agate-threshold");
    p_sys->ratio = var_CreateGetFloat(p_aout,"agate-ratio");
    p_sys->attack = var_CreateGetFloat(p_aout,"agate-attack");
    p_sys->release = var_CreateGetFloat(p_aout,"agate-release");
    p_sys->makeup = var_CreateGetFloat(p_aout,"agate-makeup");
    p_sys->knee = var_CreateGetFloat(p_aout,"agate-knee");
    p_sys->detection = var_CreateGetString(p_aout,"agate-detection");
    p_sys->link = var_CreateGetString(p_aout,"agate-link");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "agate-level_in", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-range", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-threshold", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-ratio", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-attack", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-release", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-makeup", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-knee", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-detection", paramCallback_agate, p_sys );
    var_AddCallback( p_aout, "agate-link", paramCallback_agate, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;

    /* initialize ffmpeg filter */
    if(init_filters(string_generate_agate(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"agate initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_agate =
        { .filter_audio = Process_agate, .close = Close_agate };
    p_filter->ops = &filter_ops_agate;

    return VLC_SUCCESS;
}

static void Close_agate( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

