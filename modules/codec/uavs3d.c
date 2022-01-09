/*****************************************************************************
 * uavs3d.c: AVS3-P2 video decoder using libuavs3d
 *****************************************************************************
 * Copyright © 2021 Rémi Denis-Courmont
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
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under a 4-clause BSD license. Therefore, the
 * result of compiling this module will typically not be redistributable.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <uavs3d.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_frame.h>
#include <vlc_codec.h>

static int UpdateFormat(decoder_t *dec, const com_seqh_t *info)
{
    vlc_fourcc_t chroma;
    video_format_t *fmt = &dec->fmt_out.video;

    switch (info->bit_depth_internal) {
         case 8:
             chroma = VLC_CODEC_I420;
             break;
         case 10:
#ifdef WORDS_BIGENDIAN
             chroma = VLC_CODEC_I420_10B;
#else
             chroma = VLC_CODEC_I420_10L;
#endif
             break;
         default:
             msg_Err(dec, "unsupported bit depth: %d",
                     info->bit_depth_internal);
             chroma = 0;
    }

    dec->fmt_out.i_codec = chroma;
    video_format_Init(fmt, chroma);
    fmt->i_width = info->pic_width;
    fmt->i_height = info->pic_height;
    fmt->i_visible_width = info->pic_width;
    fmt->i_visible_height = info->pic_height;
    fmt->i_sar_num = 1;
    fmt->i_sar_den = 1;

    static const struct {
        uint16_t num;
        uint16_t den;
    } frame_rates[16] = {
        { 0, 0 }, { 24000, 1001 }, { 24, 1 }, { 25, 1 },
        { 30000, 1001 }, { 30, 1 }, { 50, 1 }, { 60000, 1001 },
        { 60, 1 }, { 100, 1}, { 120, 1 }, { 200, 1 },
        { 240, 1 }, { 300, 1 }, { 0, 0 }, { 0, 0 }
    };

    assert(info->frame_rate_code <= ARRAY_SIZE(frame_rates));
    fmt->i_frame_rate = frame_rates[info->frame_rate_code].num;
    fmt->i_frame_rate_base = frame_rates[info->frame_rate_code].den;

    if (fmt->i_frame_rate == 0)
        msg_Warn(dec, "unknown frame rate code %hhu", info->frame_rate_code);

    /* TODO: colorimetry */
    return decoder_UpdateVideoFormat(dec);
}

static void Dequeue(uavs3d_io_frm_t *frame)
{
    decoder_t *dec = frame->priv;
    const com_seqh_t *info = frame->seqhdr;

    assert(frame->got_pic);
    assert(info != NULL);

    if (UpdateFormat(dec, info))
        return;

    picture_t *pic = decoder_NewPicture(dec);

    if (pic != NULL) {
        uavs3d_io_frm_t outframe;

        memcpy(outframe.width, frame->width, sizeof (outframe.width));
        memcpy(outframe.height, frame->height, sizeof (outframe.height));

        for (int i = 0; i < pic->i_planes; i++) {
            outframe.buffer[i] = pic->p[i].p_pixels;
            outframe.stride[i] = pic->p[i].i_visible_pitch;
        }

        uavs3d_img_cpy_cvt(&outframe, frame, frame->bit_depth);
        pic->date = frame->pts;
        /* TODO: field and type */
        pic->b_progressive = true;
        decoder_QueueVideo(dec, pic);
    }
}

static int Decode(decoder_t *dec, vlc_frame_t *block)
{
    void *hd = dec->p_sys;
    uavs3d_io_frm_t frame = { .priv = dec, };
    int err;

    if (block != NULL) {
        if (!(block->i_flags & BLOCK_FLAG_CORRUPTED)) {
            assert(block->i_buffer <= INT_MAX);

            frame.bs = block->p_buffer;
            frame.bs_len = block->i_buffer;
            frame.pts = block->i_pts;
            frame.dts = block->i_dts;

            err = uavs3d_decode(hd, &frame);
        } else
            err = RET_OK;

        block_Release(block);

    } else
       err = uavs3d_flush(hd, &frame);

    if (err < 0)
        msg_Err(dec, "decoding error %d", err);

    return VLCDEC_SUCCESS;
}

static void Flush(decoder_t *dec)
{
    void *hd = dec->p_sys;

    uavs3d_reset(hd);
}

static void Close(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    uavs3d_delete(dec->p_sys);
}

static int Open(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    if (dec->fmt_in->i_codec != VLC_CODEC_CAVS3)
        return VLC_EGENERIC;

    uavs3d_cfg_t cfg = { };
    int err;

    dec->p_sys = uavs3d_create(&cfg, Dequeue, &err);
    if (dec->p_sys == NULL) {
        msg_Err(obj, "decoder opening error %d", err);
        return VLC_EGENERIC;
    }

    dec->pf_decode = Decode;
    dec->pf_flush = Flush;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("uavs3d")
    set_description(N_("AVS3 decoder (using uavs3d)"))
    set_capability("video decoder", 200)
    set_callbacks(Open, Close)
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end()
