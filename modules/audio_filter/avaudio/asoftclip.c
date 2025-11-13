/*****************************************************************************
 * asoftclip.c : 
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
    char* type;
    float threshold;

    av_filter avf;
}avfilter_sys_t;


static int Open_asoftclip( vlc_object_t *p_this );
static void Close_asoftclip( filter_t *p_filter );

static char* string_generate_softclip(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "asoftclip=%s:%f", p_sys->type, p_sys->threshold);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ASOFTCLIP_TYPE_TEXT N_( "type" )
#define ASOFTCLIP_TYPE_LONGTEXT N_( "Set type of soft-clipping." )

#define ASOFTCLIP_THRESHOLD_TEXT N_( "threshold" )
#define ASOFTCLIP_THRESHOLD_LONGTEXT N_( "Set threshold from where to start clipping." \
                " Default value is 0dB or 1." )

vlc_module_begin()

    set_shortname( N_("Asoftclip") )
    set_description( N_("Audio soft clipping.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_string( "asoftclip-type", "exp", ASOFTCLIP_TYPE_TEXT,
                ASOFTCLIP_TYPE_LONGTEXT )
        change_string_list( softclip_type_list, softclip_type_list_text )

    add_float_with_range( "asoftclip-threshold", 1, 0.01, 1,
            ASOFTCLIP_THRESHOLD_TEXT, ASOFTCLIP_THRESHOLD_LONGTEXT )

    add_shortcut("asoftclip")
    set_callback( Open_asoftclip )

vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_asoftclip( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 0;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_asoftclip( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "asoftclip-type" ) )
    {
        free(p_sys->type);
        p_sys->type = strdup(newval.psz_string);
        if(p_sys->type == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "asoftclip-threshold" ) )
        p_sys->threshold = newval.f_float;
    
    if(string_generate_softclip(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_asoftclip( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    

    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->type = var_CreateGetString(p_aout,"asoftclip-type");
    p_sys->threshold = var_CreateGetFloat(p_aout,"asoftclip-threshold");
    
    
    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "asoftclip-type", paramCallback_asoftclip, p_sys );
    var_AddCallback( p_aout, "asoftclip-threshold", paramCallback_asoftclip, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    

    /* initialize ffmpeg filter */
    if(init_filters(string_generate_softclip(p_sys), p_filter, &p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"asoftclip initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_asoftclip =
        { .filter_audio = Process_asoftclip, .close = Close_asoftclip };
    p_filter->ops = &filter_ops_asoftclip;


    return VLC_SUCCESS;
}

static void Close_asoftclip( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

