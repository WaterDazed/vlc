// SPDX-License-Identifier: LGPL-2.1-or-later

// edge264.c : software H.264/MVC decoder
// Copyright © 2025 VideoLabs, VLC authors and VideoLAN

// Authors: Steve Lhomme <robux4@videolabs.io>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef MODULE_NAME
# define MODULE_NAME edge264
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <edge264.h>
#include "hxxx_helper.h"

static int OpenEdge264(vlc_object_t *);
static void CloseEdge264(vlc_object_t *);

#define THREAD_FRAMES_TEXT N_("Threads")
#define THREAD_FRAMES_LONGTEXT N_( "Max number of threads used for decoding, default -1=auto" )

vlc_module_begin ()
    set_shortname("edge264")
    set_description(N_("edge264 video decoder"))
    set_capability("video decoder", 1)
    set_callbacks(OpenEdge264, CloseEdge264)
    set_subcategory(SUBCAT_INPUT_VCODEC)

    add_integer("edge264-threads", -1,
                THREAD_FRAMES_TEXT, THREAD_FRAMES_LONGTEXT)
vlc_module_end ()

typedef struct
{
    Edge264Decoder *dec;
    struct hxxx_helper hh;
    date_t pts;
} decoder_sys_t;

void *worker_loop_log(Edge264Decoder *d); // missing in f8809f722453a803186bbf6d7ea8eabb7fe9d86c
void *worker_loop_log(Edge264Decoder *d)
{
    VLC_UNUSED(d);
    return NULL;
}

static int Decode( decoder_t *dec, vlc_frame_t *frame )
{
    decoder_sys_t *p_sys = dec->p_sys;
    Edge264Frame frm;
    int res;

    if (!frame)
    {
        // handle draining, do nothing ?
        return VLCDEC_SUCCESS;
    }

    if (frame->i_flags & (VLC_FRAME_FLAG_CORRUPTED))
    {
        date_Set( &p_sys->pts, frame->i_dts );
        vlc_frame_Release(frame);
        return VLCDEC_SUCCESS;
    }

    if (frame->i_flags & (VLC_FRAME_FLAG_DISCONTINUITY))
    {
        date_Set( &p_sys->pts, frame->i_dts );
    }

    const uint8_t *nal_end;
    block_t *hframe = hxxx_helper_process_block(&p_sys->hh, frame);

    nal_end = hframe->p_buffer + 4; // skip the [0]001 delimiter
    res = 0;
    do {
        res = edge264_decode_NAL(p_sys->dec, nal_end, &hframe->p_buffer[hframe->i_buffer], 0, NULL, NULL, &nal_end);
        while (edge264_get_frame(p_sys->dec, &frm, 0) == 0 && frm.return_arg)
        {
            vlc_fourcc_t chroma;

            if (frm.bit_depth_Y != frm.bit_depth_C)
            {
                msg_Err(dec, "mismatched bitdepth (Y:%d, C:%d)", (int)frm.bit_depth_Y, (int)frm.bit_depth_C);
                return VLCDEC_ECRITICAL;
            }
            if (frm.bit_depth_Y == 8)
                chroma = VLC_CODEC_I420;
            else
#ifdef WORDS_BIGENDIAN
                chroma = VLC_CODEC_I420_16B;
#else
                chroma = VLC_CODEC_I420_16L;
#endif

            dec->fmt_out.i_codec = chroma;
            dec->fmt_out.video.i_chroma = chroma;
            dec->fmt_out.video.i_visible_width  = frm.width_Y;
            dec->fmt_out.video.i_visible_height = frm.height_Y;
            dec->fmt_out.video.i_width  = frm.width_Y - frm.frame_crop_offsets[1] -  - frm.frame_crop_offsets[3];
            dec->fmt_out.video.i_height = frm.height_Y - frm.frame_crop_offsets[0] -  - frm.frame_crop_offsets[2];
            dec->fmt_out.video.i_x_offset = frm.frame_crop_offsets[1];
            dec->fmt_out.video.i_y_offset = frm.frame_crop_offsets[0];

            if (frm.FrameId_mvc)
            {
                // Top Bottom output (Side By Side would be better as there's less padding)
                unsigned original_height = dec->fmt_out.video.i_height;
                dec->fmt_out.video.i_height         += original_height;
                dec->fmt_out.video.i_visible_height += original_height;
            }

            if (decoder_UpdateVideoFormat(dec) != 0)
            {
                msg_Err(dec, "Failed to get a decoder output");
                return VLCDEC_ECRITICAL;
            }
            // TODO use picture_NewFromResource(), doesn't work with MVC as they are split ?
            picture_t *pic = decoder_NewPicture(dec);
            if (pic == NULL)
            {
                msg_Err(dec, "Failed to get a decoder picture");
                return VLCDEC_ECRITICAL;
            }
            pic->b_progressive = true; /* TODO: support interlaced */
            pic->date = date_Get(&p_sys->pts);
            date_Increment( &p_sys->pts, 1 );
            for (int plane = 0; plane < pic->i_planes; plane++ ) {
                plane_t src_plane = pic->p[plane];
                src_plane.p_pixels = (uint8_t*)frm.samples[plane];
                src_plane.i_visible_lines = plane == 0 ? frm.height_Y : frm.height_C;
                src_plane.i_lines = src_plane.i_visible_lines + frm.frame_crop_offsets[2];
                src_plane.i_pitch = plane == 0 ? frm.stride_Y : frm.stride_C;
                plane_CopyPixels(&pic->p[plane], &src_plane);
            }
            if (frm.FrameId_mvc)
            {
                for (int plane = 0; plane < pic->i_planes; plane++ ) {
                    plane_t src_plane = pic->p[plane];
                    src_plane.p_pixels = (uint8_t*)frm.samples_mvc[plane];
                    src_plane.i_visible_lines = plane == 0 ? frm.height_Y : frm.height_C;
                    src_plane.i_lines = plane == 0 ? frm.height_Y : frm.height_C;
                    src_plane.i_pitch = plane == 0 ? frm.stride_Y : frm.stride_C;

                    plane_t dst_plane = pic->p[plane];
                    size_t should_write = (dst_plane.i_lines / 2) * dst_plane.i_pitch;
                    if (plane == 0)
                    {
                        size_t was_written = (size_t)(frm.height_Y - 20) * frm.stride_Y;
                        if (should_write > was_written)
                            memset(pic->p[plane].p_pixels + was_written, 0, should_write - was_written);
                    }
                    else
                    {
                        size_t was_written = (size_t)(frm.height_C + frm.frame_crop_offsets[2]) * frm.stride_C;
                        if (should_write > was_written)
                            memset(pic->p[plane].p_pixels + was_written, 0x80, should_write - was_written);
                    }
                    dst_plane.p_pixels += should_write;
                    dst_plane.i_lines /= 2;
                    dst_plane.i_visible_lines /= 2;
                    plane_CopyPixels(&dst_plane, &src_plane);

                }
            }
            decoder_QueueVideo(dec, pic);
        }
    } while (res == 0 || res == ENOBUFS || res == EBADMSG);
    vlc_frame_Release(frame);

    return VLCDEC_SUCCESS;
}

static void  FlushDecoder( decoder_t *dec )
{
    decoder_sys_t *p_sys = dec->p_sys;
    // TODO crash on assert after flush
    edge264_flush(p_sys->dec);
}

int OpenEdge264(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    if (dec->fmt_in->i_codec != VLC_CODEC_H264) // TODO handle MVC
        return VLC_ENOTSUP;

    decoder_sys_t *p_sys = vlc_obj_calloc(p_this, 1, sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->dec = edge264_alloc(var_InheritInteger(p_this, "edge264-threads"), NULL, NULL, 0, NULL, NULL, NULL);
    if (unlikely(p_sys->dec == NULL))
    {
        msg_Err(p_this, "Failed to create edge264 decoder!");
        return VLC_EACCES;
    }

    if (dec->fmt_in->i_extra)
    {
        hxxx_helper_init(&p_sys->hh, p_this, dec->fmt_in->i_codec, 0, 0);
        hxxx_helper_set_extra(&p_sys->hh, dec->fmt_in->p_extra, dec->fmt_in->i_extra);

        block_t *p_xps_blocks = hxxx_helper_get_extradata_block(&p_sys->hh);
        if (p_xps_blocks)
        {
            const uint8_t *nal_end = p_xps_blocks->p_buffer + 4; // skip the [0]001 delimiter
            int res = 0;
            do {
                res = edge264_decode_NAL(p_sys->dec, nal_end, &p_xps_blocks->p_buffer[p_xps_blocks->i_buffer], 0, NULL, NULL, &nal_end);
            } while (res == 0 || res == ENOBUFS);
            block_Release(p_xps_blocks);
        }
    }

    if( dec->fmt_out.video.i_frame_rate == 0 ||
        dec->fmt_out.video.i_frame_rate_base == 0)
    {
        msg_Warn( dec, "unknown frame rate %d/%d, using 25 fps instead",
                  dec->fmt_out.video.i_frame_rate,
                  dec->fmt_out.video.i_frame_rate_base);
        date_Init( &p_sys->pts, 25, 1 );
    }
    else
        date_Init( &p_sys->pts, dec->fmt_out.video.i_frame_rate,
                    dec->fmt_out.video.i_frame_rate_base );
    date_Set( &p_sys->pts, VLC_TICK_0 );

    dec->pf_decode = Decode;
    // TODO dec->pf_flush = FlushDecoder;
    dec->p_sys = p_sys;

    return VLC_SUCCESS;
}

void CloseEdge264(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = dec->p_sys;
    edge264_free(&p_sys->dec);
    if (p_sys->hh.p_obj)
        hxxx_helper_clean(&p_sys->hh);
}
