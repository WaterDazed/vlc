/*****************************************************************************
 * avaudio.h :
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


#ifndef AVAUDIO_H
#define AVAUDIO_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif


#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_aout_volume.h>
#include <vlc_filter.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_memstream.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>

/**
 * Holds Instances of filter.
 */
typedef struct
{
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
    int frame_len_flag;
    int frmt;
    unsigned int i_rate;
} filter_sys_t;

typedef struct
{
    struct vlc_memstream str;
    filter_sys_t avfilter;

    bool params_changed;
    vlc_mutex_t lock;
}av_filter;

typedef struct
{
    block_t block;
    AVFrame *frame;
} vlc_av_frame_t;


/**
 * @brief 
 * 
 * @param filters_descr 
 * @param p_filter 
 * @return int 
 */
int init_filters(const char *filters_descr, filter_t *p_filter, filter_sys_t *p_sys);


/**
 * Check filter parameter bounds for parameters with multiple values (list) separated by '|'.
 * 
 * @param check_str complete parameter value string
 * @param min lower range value
 * @param max upper range value
 * @param val_default default value for the parameter
 */
char* check_Bounds(filter_t *p_filter,char* check_str,float min, float max,float val_default);


block_t *Process( filter_t *p_filter, block_t *p_block );
block_t *Process_avaudio( filter_t *p_filter, block_t *p_block, filter_sys_t *p_sys, int frame_flag );
bool Process_avaudio_2( filter_t *p_filter, av_filter *p_sys  );
void Close( filter_t *p_filter );

#endif

