/*****************************************************************************
 * avaudio.c :
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif


#include "avaudio.h"

/**
 * Get AVSampleFormat for vlc audio format fourcc
 * 
 * @param vlc_frmt -> input audio format fourcc
 * @return enum AVSampleFormat 
 */
enum AVSampleFormat GetAVSampleFormat(vlc_fourcc_t vlc_frmt);

int init_filters(const char *filters_descr, filter_t *p_filter, filter_sys_t *p_sys)
{
    char* args;
    int ret = 0;
    const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    
    p_sys->frame_len_flag = 1;
    p_sys->frmt = GetAVSampleFormat(p_filter->fmt_in.audio.i_format);
    

    const enum AVSampleFormat out_sample_fmts[] = { p_sys->frmt, -1 };
    const uint64_t in_ch_layout = av_get_default_channel_layout(p_filter->fmt_in.audio.i_channels);
    const int64_t out_channel_layouts[] = { in_ch_layout, -1 };
    
    p_sys->i_rate = p_filter->fmt_in.audio.i_rate;
    const int out_sample_rates[] = { p_sys->i_rate, -1 };
    const AVFilterLink *outlink;
    

    ret = asprintf(&args, "sample_fmt=%s:sample_rate=%d:channel_layout=0x%"PRIx64,
             av_get_sample_fmt_name(p_sys->frmt), p_sys->i_rate,
             in_ch_layout);
    if (ret < 0) {
        goto end1;
    }

    p_sys->filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !p_sys->filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ret = avfilter_graph_create_filter(&p_sys->buffersrc_ctx, abuffersrc, "in",
                                       args, NULL, p_sys->filter_graph);
    if (ret < 0) {
        msg_Dbg(p_filter,"init_filters: Could not create audio buffer source");
        goto end;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&p_sys->buffersink_ctx, abuffersink, "out",
                                       NULL, NULL, p_sys->filter_graph);
    if (ret < 0) {
        msg_Dbg(p_filter,"init_filters: Could not create audio buffer sink");
        goto end;
    }

    ret = av_opt_set_int_list(p_sys->buffersink_ctx, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        msg_Dbg(p_filter,"init_filters: Could not set output sample format.");
        goto end;
    }

    ret = av_opt_set_int_list(p_sys->buffersink_ctx, "channel_layouts", out_channel_layouts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        msg_Dbg(p_filter,"init_filters: Could not set output channel layout.");
        goto end;
    }

    ret = av_opt_set_int_list(p_sys->buffersink_ctx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        msg_Dbg(p_filter,"init_filters: Could not set output sample rate");
        goto end;
    }
    
    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = p_sys->buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = p_sys->buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    ret = avfilter_graph_parse_ptr(p_sys->filter_graph, filters_descr,
                                        &inputs, &outputs, NULL);
    if (ret < 0){
        msg_Dbg(p_filter, "Failed to add avfilter graph.");                                            
        goto end;
    }

    ret = avfilter_graph_config(p_sys->filter_graph, NULL);
    if (ret < 0)
        goto end;

    /* args buffer is reused to store channel layout string */
    outlink = p_sys->buffersink_ctx->inputs[0];
    av_get_channel_layout_string(args, sizeof(args), -1, outlink->channel_layout);
    
    
end:
    free(args);
end1:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

enum AVSampleFormat GetAVSampleFormat(vlc_fourcc_t vlc_frmt)
{
    enum AVSampleFormat frmt;
    switch (vlc_frmt)
    {
    case VLC_CODEC_U8:
        frmt = AV_SAMPLE_FMT_U8;
        break;
    case VLC_CODEC_S16N:
        frmt = AV_SAMPLE_FMT_S16;
        break; 
    case VLC_CODEC_S32N:
        frmt = AV_SAMPLE_FMT_S32;
        break;
    case VLC_CODEC_FL32:
        frmt = AV_SAMPLE_FMT_FLT;
        break;
    case VLC_CODEC_FL64:
        frmt = AV_SAMPLE_FMT_DBL;
        break;
    default:
        frmt=AV_SAMPLE_FMT_NONE;
        break;
    }
    return frmt;
}


char* check_Bounds(filter_t *p_filter,char* check_str,float min, float max,float val_default)
{
    struct vlc_memstream final_str;
    char* str_default;
    if(asprintf(&str_default, "%f", val_default) < 0)
        return NULL;
    
    if( vlc_memstream_open( &final_str ) != 0 )
    {
        free(str_default);
        return NULL;
    }
    char* current_str = strtok_r(check_str, "|", &check_str);
    float current_str_val = strtof(current_str, NULL);

    if(current_str_val <= min || current_str_val > max){
        vlc_memstream_printf(&final_str, "%s", str_default);
        msg_Warn(p_filter,"filter parameter out of bound. Set to default");
    }
    else
        vlc_memstream_printf(&final_str, "%s", current_str);

    current_str = strtok_r(check_str, "|", &check_str);
    while (current_str != NULL) 
    {
        current_str_val = strtof(current_str, NULL);
        vlc_memstream_printf(&final_str, "|" );

        if(current_str_val <= min || current_str_val > max){
            vlc_memstream_printf(&final_str, "%s", str_default);
            msg_Warn(p_filter,"filter parameter out of bound. Set to default");
        }
        else
            vlc_memstream_printf(&final_str, "%s", current_str);
        
        current_str = strtok_r(check_str, "|", &check_str);
    }

    if(vlc_memstream_close(&final_str))
    {
        free(str_default);
        return NULL;
    }

    free(str_default);
    return final_str.ptr;
}

/*****************************************************************************
 * Process: process samples buffer
 *****************************************************************************/
void vlc_av_frame_Release(block_t *block);
block_t *vlc_av_frame_Wrap(AVFrame *frame, int frame_len_flag);
block_t *convert_AVFrame( filter_t *filter , AVFrame *frame , int frame_len_flag);


block_t *Process( filter_t *p_filter, block_t *p_block )
{
    filter_sys_t* p_sys = p_filter->p_sys;
    return Process_avaudio(p_filter, p_block, p_filter->p_sys, p_sys->frame_len_flag);
}

block_t *Process_avaudio( filter_t *p_filter, block_t *p_block, filter_sys_t *p_sys, int frame_flag )
{
    block_t *p_out_block;
    block_t *temp = p_block;

    int ret;
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        msg_Err(p_filter,"Could not allocate frame.");
        return p_block;
    }
    
    AVFrame *filt_frame = av_frame_alloc();
    if (!filt_frame) {
        msg_Err(p_filter,"Could not allocate frame.");
        av_frame_free(&frame);
        return p_block;
    }


    frame->data[0] = temp->p_buffer;
    frame->linesize[0] = temp->i_buffer;

    frame->nb_samples = temp->i_nb_samples;
    frame->format = p_sys->frmt;

    frame->pts = temp->i_pts;
    frame->pkt_dts = temp->i_dts;
    frame->pkt_size = temp->i_length;

    frame->sample_rate = (int)p_filter->fmt_in.audio.i_rate;
    frame->channel_layout = av_get_default_channel_layout(p_filter->fmt_in.audio.i_channels);
    
    frame->channels = p_filter->fmt_in.audio.i_channels;
    

    /* push the audio data from decoded frame into the filtergraph */
    if (av_buffersrc_add_frame_flags(p_sys->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
        msg_Dbg(p_filter, "Error while feeding the audio filtergraph.");
        goto end;
    }

    /* pull filtered audio from the filtergraph */
    ret = av_buffersink_get_frame(p_sys->buffersink_ctx, filt_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
        msg_Err(p_filter,"Could not allocate frame.");
    }
    if (ret < 0 && ret != AVERROR_EOF) {
        msg_Err(p_filter,"Error occurred: %s", av_err2str(ret));
        goto end;
    }
    
    if(!frame_flag) //need work to avoid duplicating block
        p_out_block = block_Duplicate(convert_AVFrame(p_filter,filt_frame,frame_flag));
    else
        p_out_block = convert_AVFrame(p_filter,filt_frame,frame_flag);

    if( p_out_block == NULL ){
        msg_Err(p_filter , "Could not allocate out block_t.");
        goto end;
    }

    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    block_Release(p_block);

    return p_out_block;
    
end:
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    return p_block;
}

bool Process_avaudio_2( filter_t *p_filter, av_filter *p_sys  )
{
    vlc_mutex_lock(&p_sys->lock);
    if(p_sys->params_changed)
    {
        p_sys->params_changed = false;
        avfilter_graph_free(&p_sys->avfilter.filter_graph);
        if(init_filters(p_sys->str.ptr,p_filter,&p_sys->avfilter) < 0)
        {
            avfilter_graph_free(&p_sys->avfilter.filter_graph);
            free(p_sys);
            return false;
        }
        free(p_sys->str.ptr);
    }
    vlc_mutex_unlock( &p_sys->lock);
    
    return true;
}

void vlc_av_frame_Release(block_t *block)
{
    vlc_av_frame_t *b = container_of(block, vlc_av_frame_t , block );

    free(b);
}

struct vlc_block_callbacks vlc_av_frame_cbs =
{
    vlc_av_frame_Release,
};

block_t *vlc_av_frame_Wrap(AVFrame *frame, int frame_len_flag)
{
    int frame_len_correct = 256;
    if(frame_len_flag == 0)
        frame_len_correct = 0;
    

    for (unsigned i = 1; i < AV_NUM_DATA_POINTERS; i++)
        assert(frame->linesize[i] == 0); /* only packed frame supported */

    vlc_av_frame_t *b = malloc(sizeof (*b));
    if (unlikely(b == NULL))
        return NULL;

    block_t *block = &b->block;

    block_Init(block, &vlc_av_frame_cbs,
               frame->extended_data[0], frame->linesize[0]-frame_len_correct);
    block->i_nb_samples = frame->nb_samples;

    return block;
}

/**
 * Convert AVFrame* to Block_t*
 */
block_t *convert_AVFrame( filter_t *filter , AVFrame *frame , int frame_len_flag)
{
    block_t *p_block;

    if( av_sample_fmt_is_planar( frame->format ) )
    {
        p_block = block_Alloc(frame->linesize[0] * frame->channels);
        if ( likely(p_block) )
        {
            int nb_channels = 32 < frame->channels? 32:frame->channels;
            const void *planes[nb_channels];
            for (int i = 0; i < nb_channels ; i++)
                planes[i] = frame->extended_data[i];

            aout_Interleave(p_block->p_buffer, planes, frame->nb_samples,
                            nb_channels, filter->fmt_in.audio.i_format);
            p_block->i_nb_samples = frame->nb_samples;

        }        
        av_frame_free(&frame);
    }
    else
    {
        p_block = vlc_av_frame_Wrap(frame, frame_len_flag);
    }

    return p_block;
}


/*****************************************************************************
 * Close: close filter
 *****************************************************************************/

void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    avfilter_graph_free(&p_sys->filter_graph);
    free(p_sys);
}

