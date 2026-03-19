#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_aout.h>
#include <math.h>

/*****************************************************************************
 * Local prototypes and structures
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( filter_t * );
static block_t *Process( filter_t *, block_t * );

typedef struct
{
    float f_alpha;
    float *p_previous_samples; // Array to store memory for each channel
    unsigned i_channels;
} filter_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
    set_description( "Simple Low Pass Filter" )
    set_capability( "audio filter", 0 )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    
    add_float( "lpf-cutoff", 1000.0, "Cutoff Frequency (Hz)", NULL )
    
    set_callback( Open )
vlc_module_end()

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    // We only support Float32 audio for this simple example
    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 )
    {
        msg_Err( p_filter, "LPF requires Float32 audio format" );
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_filter->p_sys = p_sys;
    p_sys->i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    
    // Allocate memory for the 'previous sample' of each channel
    p_sys->p_previous_samples = calloc( p_sys->i_channels, sizeof(float) );
    if( !p_sys->p_previous_samples )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    // Calculate Alpha
    float f_cutoff = var_InheritFloat( p_filter, "lpf-cutoff" );
    float f_sample_rate = p_filter->fmt_in.audio.i_rate;
    float dt = 1.0f / f_sample_rate;
    float rc = 1.0f / ( 2.0f * (float)M_PI * f_cutoff );
    p_sys->f_alpha = dt / ( rc + dt );

    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Process
 *****************************************************************************/
static block_t *Process( filter_t *p_filter, block_t *p_block )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    float *p_samples = (float *)p_block->p_buffer;
    unsigned i_samples_count = p_block->i_nb_samples;
    unsigned i_channels = p_sys->i_channels;
    float alpha = p_sys->f_alpha;

    // Iterate through samples. 
    // VLC interlaces channels: [L, R, L, R, L, R]
    for( unsigned i = 0; i < i_samples_count; i++ )
    {
        for( unsigned c = 0; c < i_channels; c++ )
        {
            float f_input = p_samples[i * i_channels + c];
            
            // First-Order IIR: y[n] = a*x[n] + (1-a)*y[n-1]
            float f_output = ( alpha * f_input ) + 
                             ( ( 1.0f - alpha ) * p_sys->p_previous_samples[c] );
            
            p_samples[i * i_channels + c] = f_output;
            p_sys->p_previous_samples[c] = f_output;
        }
    }

    return p_block;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    free( p_sys->p_previous_samples );
    free( p_sys );
}