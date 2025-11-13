/*****************************************************************************
 * avaudio_filters.c : 
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
    float mfreq;
    float mdepth;
    
    av_filter avf;
}avfilter_sys_t;


static int Open_tremolo( vlc_object_t *p_this );
static void Close_tremolo( filter_t *p_filter );

static char* string_generate_tremolo(avfilter_sys_t* p_sys)
{
    if( vlc_memstream_open( &p_sys->avf.str ) != 0 )
        return NULL;

    vlc_memstream_printf(&p_sys->avf.str, "tremolo=%f:%f", p_sys->mfreq, p_sys->mdepth);

    if(vlc_memstream_close(&p_sys->avf.str))
        return NULL;    
    
    return p_sys->avf.str.ptr;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define TREMOLO_MOD_FREQUENCY_TEXT N_( "mod frequency" )
#define TREMOLO_MOD_FREQUENCY_LONGTEXT N_( "Modulation frequency in Hertz. Modulation frequencies" \
                " in the subharmonic range (20 Hz or lower) will result in a tremolo effect." \
                " This filter may also be used as a ring modulator by specifying a modulation" \
                " frequency higher than 20 Hz. Range is 0.1 - 20000.0." \
                " Default value is 5.0 Hz." )

#define TREMOLO_MOD_DEPTH_TEXT N_( "mod depth" )
#define TREMOLO_MOD_DEPTH_LONGTEXT N_( "Depth of modulation as a percentage. Range is 0.0 - 1.0." \
                " Default value is 0.5." )

vlc_module_begin()

    set_shortname( N_("Tremolo") )
    set_description( N_("Tremolo: Sinusoidal amp modulation.") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )

    add_float_with_range( "tremolo-mod-freq", 5, 0.1, 2000,
            TREMOLO_MOD_FREQUENCY_TEXT, TREMOLO_MOD_FREQUENCY_LONGTEXT )

    add_float_with_range( "tremolo-mod-depth", 0.5, 0, 1,
            TREMOLO_MOD_DEPTH_TEXT, TREMOLO_MOD_DEPTH_LONGTEXT )

    add_shortcut("tremolo")
    set_callback( Open_tremolo )
    
vlc_module_end()


/*****************************************************************************
 * Open: initialize filter
*****************************************************************************/

static block_t *Process_tremolo( filter_t *p_filter, block_t *p_block )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    if(!Process_avaudio_2(p_filter,&p_sys->avf))
        return p_block;
    p_sys->avf.avfilter.frame_len_flag = 1;
    return Process_avaudio(p_filter, p_block, &p_sys->avf.avfilter, p_sys->avf.avfilter.frame_len_flag);
}

static int paramCallback_tremolo( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void * p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    avfilter_sys_t *p_sys;
    p_sys = p_data;

    vlc_mutex_lock(&p_sys->avf.lock);
    
    if( !strcmp( psz_cmd, "tremolo-mod-freq" ) )
        p_sys->mfreq = newval.f_float;
    if( !strcmp( psz_cmd, "tremolo-mod-depth" ) )
        p_sys->mdepth = newval.f_float;

    if(string_generate_tremolo(p_sys) == NULL)
        return VLC_EGENERIC;
  
    p_sys->avf.params_changed = true;

    vlc_mutex_unlock(&p_sys->avf.lock);

    return VLC_SUCCESS;
}

static int Open_tremolo( vlc_object_t *p_this )
{   
    filter_t *p_filter = (filter_t *)p_this;
    avfilter_sys_t *p_sys = malloc( sizeof( *p_sys ) );

    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;
    

    p_filter->p_sys = p_sys;
    
    vlc_object_t *p_aout = vlc_object_parent(p_filter);
    p_sys->mfreq = var_CreateGetFloat(p_aout,"tremolo-mod-freq");
    p_sys->mdepth = var_CreateGetFloat(p_aout,"tremolo-mod-depth");
    
    p_sys->avf.params_changed = false;
    vlc_mutex_init(&p_sys->avf.lock);
    var_AddCallback( p_aout, "tremolo-mod-freq", paramCallback_tremolo, p_sys );
    var_AddCallback( p_aout, "tremolo-mod-depth", paramCallback_tremolo, p_sys );

    p_filter->fmt_out.audio = p_filter->fmt_in.audio;


    /* initialize ffmpeg filter */
    if(init_filters(string_generate_tremolo(p_sys),p_filter, &p_sys->avf.avfilter) < 0)
    {
        avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
        free(p_sys);
        return VLC_EGENERIC;
    }
    /*filter description*/
    msg_Dbg(p_filter,"tremolo initialized:%s",p_sys->avf.str.ptr);
    free(p_sys->avf.str.ptr);

    static const struct vlc_filter_operations filter_ops_tremolo =
        { .filter_audio = Process_tremolo, .close = Close_tremolo };
    p_filter->ops = &filter_ops_tremolo;

    return VLC_SUCCESS;
}

static void Close_tremolo( filter_t *p_filter )
{
    avfilter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->avf.avfilter.filter_graph);
    free(p_sys);
}

