/*****************************************************************************
 * treble.c : 
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
    float gain;
    float freq;
    char* width_type;
    float width;
    float mix;

    av_filter avf;
}avfilter_sys_t;


static int Open_treble( vlc_object_t *p_this );
static void Close_treble( filter_t *p_filter );

static char* string_generate_treble(avfilter_sys_t *p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "treble=g=%f:f=%f:t=%s:w=%f:m=%f", p_sys->gain, 
            p_sys->freq, p_sys->width_type, p_sys->width, p_sys->mix);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;

    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define TREBLE_GAIN_TEXT N_( "gain" )
#define TREBLE_GAIN_LONGTEXT N_( "Set gain Its useful range is about -20 (for a large cut) to +20" \
                    " (for a large boost). Beware of clipping when using a positive gain." )

#define TREBLE_FREQUENCY_TEXT N_( "frequency" )
#define TREBLE_FREQUENCY_LONGTEXT N_( "Set the filter’s central frequency and so can be used to" \
                    " extend or reduce the frequency range to be boosted or cut." \
                    " The default value is 3000 Hz." )

#define TREBLE_WIDTH_TYPE_TEXT N_( "width type" )
#define TREBLE_WIDTH_TYPE_LONGTEXT N_( "Set method to specify band-width of filter." )

#define TREBLE_WIDTH_TEXT N_( "width" )
#define TREBLE_WIDTH_LONGTEXT N_( "Determine how steep is the filter’s shelf transition." )

#define TREBLE_MIX_TEXT N_( "mix" )
#define TREBLE_MIX_LONGTEXT N_( "Set mixing amount. Default is 1." \
                    " Range is [0 - 1]")

vlc_module_begin()

    set_shortname( N_("Treble") )
    set_description( N_("Boost or cut treble frequencies") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "treble-gain", 0, -150, 50,
            TREBLE_GAIN_TEXT, TREBLE_GAIN_LONGTEXT )

    add_float_with_range( "treble-frequency", 500, 0.1, 10000,
            TREBLE_FREQUENCY_TEXT, TREBLE_FREQUENCY_LONGTEXT )

    add_string( "treble-width-type", "h", TREBLE_WIDTH_TYPE_TEXT,
            TREBLE_WIDTH_TYPE_LONGTEXT )
        change_string_list( lowpass_width_type_list, lowpass_width_type_list_text )

    add_float_with_range( "treble-width", 0.7, 0.1, 1000,
            TREBLE_WIDTH_TEXT, TREBLE_WIDTH_LONGTEXT )

    add_float_with_range( "treble-mix", 1, 0, 1,
            TREBLE_MIX_TEXT, TREBLE_MIX_LONGTEXT )

    add_shortcut("treble")
    set_callback( Open_treble )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_treble( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_treble( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);

    if( !strcmp( psz_cmd, "treble-gain" ) )
        p_sys->gain = newval.f_float;
    if( !strcmp( psz_cmd, "treble-frequency" ) )
        p_sys->freq = newval.f_float;
    if( !strcmp( psz_cmd, "treble-width-type" ) )
    {
        free(p_sys->width_type);
        p_sys->width_type = strdup(newval.psz_string);
        if(p_sys->width_type == NULL)
            return VLC_EGENERIC;
    }
    if( !strcmp( psz_cmd, "treble-width" ) )
        p_sys->width = newval.f_float;
    if( !strcmp( psz_cmd, "treble-mix" ) )
        p_sys->mix = newval.f_float;

    if(string_generate_treble(p_sys) == NULL)
        return VLC_EGENERIC;

    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);


    return VLC_SUCCESS;
}

static int Open_treble( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    
    
    p_filter->p_sys = p_sys;

    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->gain = var_CreateGetFloat(p_aout,"treble-gain");
    p_sys->freq = var_CreateGetFloat(p_aout,"treble-frequency");
    p_sys->width_type = var_CreateGetString(p_aout,"treble-width-type");
    p_sys->width = var_CreateGetFloat(p_aout,"treble-width");
    p_sys->mix = var_CreateGetFloat(p_aout,"treble-mix");

    p_sys->avf.params_changed = false;
    vlc_mutex_init( &p_sys->avf.lock );
    var_AddCallback( p_aout, "treble-gain", paramCallback_treble, p_sys );
    var_AddCallback( p_aout, "treble-frequency", paramCallback_treble, p_sys );
    var_AddCallback( p_aout, "treble-width-type", paramCallback_treble, p_sys );
    var_AddCallback( p_aout, "treble-width", paramCallback_treble, p_sys );
    var_AddCallback( p_aout, "treble-mix", paramCallback_treble, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_treble(p_sys),p_filter,&p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"treble initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_treble =
        { .filter_audio = Process_treble, .close = Close_treble };
    p_filter->ops = &filter_ops_treble;

    return VLC_SUCCESS;
}

static void Close_treble( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

