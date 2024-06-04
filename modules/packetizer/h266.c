/*****************************************************************************
 * h266.c: H.266/VVC video packetizer
 *****************************************************************************
 * Copyright © 2019-2024 VideoLabs, VideoLAN and VLC Authors
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_bits.h>

#include <vlc_block_helper.h>
#include "packetizer_helper.h"
#include "startcode_helper.h"
#include "h266_nal.h"
#include "hxxx_nal.h"
#include "hxxx_sei.h"
#include "hxxx_common.h"

#include <limits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_subcategory(SUBCAT_SOUT_PACKETIZER)
    set_description(N_("H266/H.266 video packetizer"))
    set_capability("packetizer", 50)
    set_callbacks(Open, Close)
vlc_module_end ()


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
struct h266_tuple_s
{
    block_t *p_nal;
    void *p_decoded;
};

typedef struct
{
    /* */
    packetizer_t packetizer;

    struct
    {
        block_t *p_chain;
        block_t **pp_chain_last;
    } frame, pre, post;

    struct h266_tuple_s rg_vps[H266_MAX_NUM_VPS],
                        rg_sps[H266_MAX_NUM_SPS],
                        rg_pps[H266_MAX_NUM_PPS];

    const h266_video_parameter_set_t    *p_active_vps;
    const h266_sequence_parameter_set_t *p_active_sps;
    const h266_picture_parameter_set_t  *p_active_pps;
    enum
    {
        MISSING = 0,
        COMPLETE,
        SENT,
    } sets;
    /* Recovery starts from IFRAME or SEI recovery point */
    bool b_recovery_point;

    date_t dts;
    vlc_tick_t pts;
    bool b_need_ts;

    /* */
    cc_storage_t *p_ccs;

} decoder_sys_t;

static block_t *PacketizeAnnexB(decoder_t *, block_t **);
static void PacketizeFlush(decoder_t *);
static void PacketizeReset(void *p_private, bool b_flush);
static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *);
static block_t *ParseNALBlock(decoder_t *, bool *pb_ts_used, block_t *);
static int PacketizeValidate(void *p_private, block_t *);
static block_t * PacketizeDrain(void *);
static block_t *GetCc(decoder_t *p_dec, decoder_cc_desc_t *p_desc);
static block_t *GetXPSCopy(decoder_sys_t *p_sys);
static bool ParseSEICallback(const hxxx_sei_data_t *p_sei_data, void *cbdata);

#define BLOCK_FLAG_DROP (1 << BLOCK_FLAG_PRIVATE_SHIFT)

/****************************************************************************
 * Helpers
 ****************************************************************************/
static inline void InitQueue(block_t **pp_head, block_t ***ppp_tail)
{
    *pp_head = NULL;
    *ppp_tail = pp_head;
}
#define INITQ(name) InitQueue(&p_sys->name.p_chain, &p_sys->name.pp_chain_last)

static block_t * OutputQueues(decoder_sys_t *p_sys, bool b_valid)
{
    block_t *p_output = NULL;
    block_t *p_xps = NULL;
    block_t **pp_output_last = &p_output;
    uint32_t i_flags = 0; /* Because block_ChainGather does not merge flags or times */

    if(p_sys->b_recovery_point && p_sys->sets != SENT)
        p_xps = GetXPSCopy(p_sys);

    if(p_sys->pre.p_chain)
    {
        i_flags |= p_sys->pre.p_chain->i_flags;

        if(p_sys->pre.p_chain->i_buffer >= 5 &&
            h266_getNALType(&p_sys->pre.p_chain->p_buffer[4]) == H266_NAL_AUD)
        {
            block_t *p_au = p_sys->pre.p_chain;
            p_sys->pre.p_chain = p_sys->pre.p_chain->p_next;
            p_au->p_next = NULL;
            block_ChainLastAppend(&pp_output_last, p_au);
        }

        if(p_xps)
            block_ChainLastAppend(&pp_output_last, p_xps);

        if(p_sys->pre.p_chain)
            block_ChainLastAppend(&pp_output_last, p_sys->pre.p_chain);
        INITQ(pre);
    }
    else if(p_xps)
    {
        block_ChainLastAppend(&pp_output_last, p_xps);
    }

    if(p_sys->frame.p_chain)
    {
        i_flags |= p_sys->frame.p_chain->i_flags;
        block_ChainLastAppend(&pp_output_last, p_sys->frame.p_chain);
        p_output->i_dts = date_Get(&p_sys->dts);
        p_output->i_pts = p_sys->pts;
        INITQ(frame);
    }

    if(p_sys->post.p_chain)
    {
        i_flags |= p_sys->post.p_chain->i_flags;
        block_ChainLastAppend(&pp_output_last, p_sys->post.p_chain);
        INITQ(post);
    }

    if(p_output)
    {
        p_output->i_flags |= i_flags;
        if(!b_valid)
            p_output->i_flags |= BLOCK_FLAG_DROP;
    }

    return p_output;
}


/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in->i_codec != VLC_CODEC_VVC)
        return VLC_EGENERIC;

    p_dec->p_sys = p_sys = calloc(1, sizeof(decoder_sys_t));
    if (!p_dec->p_sys)
        return VLC_ENOMEM;

    p_sys->p_ccs = cc_storage_new();
    if(unlikely(!p_sys->p_ccs))
    {
        free(p_dec->p_sys);
        return VLC_ENOMEM;
    }

    INITQ(pre);
    INITQ(frame);
    INITQ(post);

    packetizer_Init(&p_sys->packetizer,
                    annexb_startcode3, 3, startcode_FindAnnexB,
                    annexb_startcode3, 1, 5,
                    PacketizeReset, PacketizeParse, PacketizeValidate, PacketizeDrain,
                    p_dec);

    /* Copy properties */
    es_format_Copy(&p_dec->fmt_out, p_dec->fmt_in);
    p_dec->fmt_out.b_packetized = true;

    /* Init timings */
    if(p_dec->fmt_in->video.i_frame_rate_base &&
        p_dec->fmt_in->video.i_frame_rate &&
        p_dec->fmt_in->video.i_frame_rate <= UINT_MAX / 2)
        date_Init(&p_sys->dts, p_dec->fmt_in->video.i_frame_rate * 2,
                                p_dec->fmt_in->video.i_frame_rate_base);
    else
        date_Init(&p_sys->dts, 2 * 30000, 1001);
    p_sys->pts = VLC_TICK_INVALID;
    p_sys->b_need_ts = true;
    p_sys->sets = MISSING;

    p_dec->pf_packetize = PacketizeAnnexB;

    p_dec->pf_flush = PacketizeFlush;
    p_dec->pf_get_cc = GetCc;

    if(p_dec->fmt_out.i_extra)
    {
        /* Feed with AnnexB VPS/SPS/PPS/SEI extradata */
        packetizer_Header(&p_sys->packetizer,
                          p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra);
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    packetizer_Clean(&p_sys->packetizer);

    block_ChainRelease(p_sys->frame.p_chain);
    block_ChainRelease(p_sys->pre.p_chain);
    block_ChainRelease(p_sys->post.p_chain);

    for(unsigned i=0;i<H266_MAX_NUM_PPS; i++)
    {
        if(p_sys->rg_pps[i].p_decoded)
            h266_rbsp_release_pps(p_sys->rg_pps[i].p_decoded);
        if(p_sys->rg_pps[i].p_nal)
            block_Release(p_sys->rg_pps[i].p_nal);
    }

    for(unsigned i=0;i<H266_MAX_NUM_SPS; i++)
    {
        if(p_sys->rg_sps[i].p_decoded)
            h266_rbsp_release_sps(p_sys->rg_sps[i].p_decoded);
        if(p_sys->rg_sps[i].p_nal)
            block_Release(p_sys->rg_sps[i].p_nal);
    }

    for(unsigned i=0;i<H266_MAX_NUM_VPS; i++)
    {
        if(p_sys->rg_vps[i].p_decoded)
            h266_rbsp_release_vps(p_sys->rg_vps[i].p_decoded);
        if(p_sys->rg_vps[i].p_nal)
            block_Release(p_sys->rg_vps[i].p_nal);
    }

    cc_storage_delete(p_sys->p_ccs);

    free(p_sys);
}

/****************************************************************************
 * Packetize
 ****************************************************************************/
static block_t *PacketizeAnnexB(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return packetizer_Packetize(&p_sys->packetizer, pp_block);
}

static void PacketizeFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Flush(&p_sys->packetizer);
}

/*****************************************************************************
 * GetCc:
 *****************************************************************************/
static block_t *GetCc(decoder_t *p_dec, decoder_cc_desc_t *p_desc)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    return cc_storage_get_current(p_sys->p_ccs, p_desc);
}

/****************************************************************************
 * Packetizer Helpers
 ****************************************************************************/
static void PacketizeReset(void *p_private, bool b_flush)
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t *p_out = OutputQueues(p_sys, false);
    if(p_out)
        block_ChainRelease(p_out);

    if(b_flush)
    {
        p_sys->sets = MISSING;
        p_sys->b_recovery_point = false;
    }
    p_sys->b_need_ts = true;
    date_Set(&p_sys->dts, VLC_TICK_INVALID);
}

static bool InsertXPS(decoder_t *p_dec, uint8_t i_nal_type, uint8_t i_id,
                      const block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct h266_tuple_s *p_tuple;
    void **pp_active;

    switch(i_nal_type)
    {
    case H266_NAL_VPS: /* VPS is optional, ID 0 does not exists */
        if(i_id > H266_VPS_ID_MAX)
            return false;
        p_tuple = &p_sys->rg_vps[i_id - 1];
        pp_active = (void**)&p_sys->p_active_vps;
        break;
    case H266_NAL_SPS:
        if(i_id > H266_SPS_ID_MAX)
            return false;
        p_tuple = &p_sys->rg_sps[i_id];
        pp_active = (void**)&p_sys->p_active_sps;
        break;
    case H266_NAL_PPS:
        if(i_id > H266_PPS_ID_MAX)
            return false;
        p_tuple = &p_sys->rg_pps[i_id];
        pp_active = (void**)&p_sys->p_active_pps;
        break;
    default:
        return false;
    }

    /* Check if we really need to re-decode/replace */
    if(p_tuple->p_nal)
    {
        const uint8_t *p_stored = p_tuple->p_nal->p_buffer;
        size_t i_stored = p_tuple->p_nal->i_buffer;
        hxxx_strip_AnnexB_startcode(&p_stored, &i_stored);
        const uint8_t *p_new = p_nalb->p_buffer;
        size_t i_new = p_nalb->i_buffer;
        hxxx_strip_AnnexB_startcode(&p_new, &i_new);
        if(i_stored == i_new && !memcmp(p_stored, p_new, i_new))
            return true;
    }

    /* Free associated decoded version */
    if(p_tuple->p_decoded)
    {
        switch(i_nal_type)
        {
        case H266_NAL_VPS:
            h266_rbsp_release_vps(p_tuple->p_decoded);
            break;
        case H266_NAL_SPS:
            h266_rbsp_release_sps(p_tuple->p_decoded);
            break;
        case H266_NAL_PPS:
            h266_rbsp_release_pps(p_tuple->p_decoded);
            break;
        }
        if(*pp_active == p_tuple->p_decoded)
            *pp_active = NULL;
        else
            pp_active = NULL; /* don't change pointer */
        p_tuple->p_decoded = NULL;
    }
    else pp_active = NULL;

    /* Free raw stored version */
    if(p_tuple->p_nal)
    {
        block_Release(p_tuple->p_nal);
        p_tuple->p_nal = NULL;
    }

    const uint8_t *p_buffer = p_nalb->p_buffer;
    size_t i_buffer = p_nalb->i_buffer;
    if(hxxx_strip_AnnexB_startcode(&p_buffer, &i_buffer))
    {
        /* Create decoded entries */
        switch(i_nal_type)
        {
        case H266_NAL_SPS:
            p_tuple->p_decoded = h266_decode_sps(p_buffer, i_buffer, true);
            if(!p_tuple->p_decoded)
            {
                msg_Err(p_dec, "Failed decoding SPS id %d", i_id);
                return false;
            }
            break;
        case H266_NAL_PPS:
            p_tuple->p_decoded = h266_decode_pps(p_buffer, i_buffer, true);
            if(!p_tuple->p_decoded)
            {
                msg_Err(p_dec, "Failed decoding PPS id %d", i_id);
                return false;
            }
            break;
        case H266_NAL_VPS:
            p_tuple->p_decoded = h266_decode_vps(p_buffer, i_buffer, true);
            if(!p_tuple->p_decoded)
            {
                msg_Err(p_dec, "Failed decoding VPS id %d", i_id);
                return false;
            }
            break;
        }

        if(p_tuple->p_decoded && pp_active) /* restore active by id */
            *pp_active = p_tuple->p_decoded;

        p_tuple->p_nal = block_Duplicate((block_t *)p_nalb);

        return true;
    }

    return false;
}

static block_t *GetXPSCopy(decoder_sys_t *p_sys)
{
    block_t *p_chain = NULL;
    block_t **pp_append = &p_chain;
    struct h266_tuple_s *xpstype[3] = {p_sys->rg_vps, p_sys->rg_sps, p_sys->rg_pps};
    size_t xpsmax[3] = {ARRAY_SIZE(p_sys->rg_vps), ARRAY_SIZE(p_sys->rg_sps), ARRAY_SIZE(p_sys->rg_pps)};
    for(size_t i=0; i<3; i++)
    {
        struct h266_tuple_s *xps = xpstype[i];
        for(size_t j=0; j<xpsmax[i]; j++)
        {
            block_t *p_dup;
            if(xps[j].p_nal &&
                (p_dup = block_Duplicate(xps[j].p_nal)))
                block_ChainLastAppend(&pp_append, p_dup);
        };
    }
    return p_chain;
}

static bool XPSReady(decoder_sys_t *p_sys)
{
    for(unsigned i=0;i<=H266_PPS_ID_MAX; i++)
    {
        const h266_picture_parameter_set_t *p_pps = p_sys->rg_pps[i].p_decoded;
        if (p_pps)
        {
            uint8_t id_sps = h266_get_pps_sps_id(p_pps);
            const h266_sequence_parameter_set_t *p_sps = p_sys->rg_sps[id_sps].p_decoded;
            if(p_sps)
            {
                uint8_t id_vps = h266_get_sps_vps_id(p_sps);
                if(id_vps == 0 || p_sys->rg_vps[id_vps].p_decoded)
                    return true;
            }
        }
    }
    return false;
}

static void AppendAsAnnexB(const block_t *p_block,
                           uint8_t **pp_dst, size_t *pi_dst)
{
    if(SIZE_MAX - p_block->i_buffer < *pi_dst)
        return;

    size_t i_realloc = p_block->i_buffer + *pi_dst;
    uint8_t *p_realloc = realloc(*pp_dst, i_realloc);
    if(p_realloc)
    {
        memcpy(&p_realloc[*pi_dst], p_block->p_buffer, p_block->i_buffer);
        *pi_dst = i_realloc;
        *pp_dst = p_realloc;
    }
}

#define APPENDIF(set, rg, b) \
for(size_t i=0; i<ARRAY_SIZE(rg); i++)\
    {\
            if(((set != rg[i].p_decoded) == !b) && rg[i].p_nal)\
        {\
                AppendAsAnnexB(rg[i].p_nal, &p_data, &i_data);\
                if(b) break;\
        }\
    }

static void SetsToAnnexB(decoder_sys_t *p_sys,
                         const h266_picture_parameter_set_t *p_pps,
                         const h266_sequence_parameter_set_t *p_sps,
                         const h266_video_parameter_set_t *p_vps,
                         uint8_t **pp_out, size_t *pi_out)
{
    uint8_t *p_data = NULL;
    size_t i_data = 0;

    APPENDIF(p_vps, p_sys->rg_vps, true);
    APPENDIF(p_vps, p_sys->rg_vps, false);
    APPENDIF(p_sps, p_sys->rg_sps, true);
    APPENDIF(p_sps, p_sys->rg_sps, false);
    APPENDIF(p_pps, p_sys->rg_pps, true);
    APPENDIF(p_pps, p_sys->rg_pps, false);

    /* because we copy to i_extra :/ */
    if(i_data <= INT_MAX)
    {
        *pp_out = p_data;
        *pi_out = i_data;
    }
    else free(p_data);
}

static void ActivateSets(decoder_t *p_dec,
                         const h266_picture_parameter_set_t *p_pps,
                         const h266_sequence_parameter_set_t *p_sps,
                         const h266_video_parameter_set_t *p_vps)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    p_sys->p_active_pps = p_pps;
    p_sys->p_active_sps = p_sps;
    p_sys->p_active_vps = p_vps;

    if(p_sps)
    {
        if(!p_dec->fmt_out.video.i_frame_rate || !p_dec->fmt_out.video.i_frame_rate_base)
        {
            unsigned num, den;
            if(h266_get_frame_rate(p_sps, p_vps, &num, &den))
            {
                p_dec->fmt_out.video.i_frame_rate = num;
                p_dec->fmt_out.video.i_frame_rate_base = den;
                if(num <= UINT_MAX / 2 &&
                    (p_sys->dts.i_divider_den != den ||
                     p_sys->dts.i_divider_num != 2 * num))
                {
                    date_Change(&p_sys->dts, 2 * num, den);
                }
            }
            p_dec->fmt_out.video.i_frame_rate = p_sys->dts.i_divider_num >> 1;
            p_dec->fmt_out.video.i_frame_rate_base = p_sys->dts.i_divider_den;
        }

        if(p_dec->fmt_in->video.primaries == COLOR_PRIMARIES_UNDEF)
        {
            (void) h266_get_colorimetry(p_sps,
                                        &p_dec->fmt_out.video.primaries,
                                        &p_dec->fmt_out.video.transfer,
                                        &p_dec->fmt_out.video.space,
                                        &p_dec->fmt_out.video.color_range);
        }

        unsigned sizes[6];
        if(h266_get_picture_size(p_sps, &sizes[0], &sizes[1],
                                  &sizes[2], &sizes[3],
                                  &sizes[4], &sizes[5]))
        {
            p_dec->fmt_out.video.i_x_offset = sizes[0];
            p_dec->fmt_out.video.i_y_offset = sizes[1];
            p_dec->fmt_out.video.i_width = sizes[2];
            p_dec->fmt_out.video.i_height = sizes[3];
            if(p_dec->fmt_in->video.i_visible_width == 0)
            {
                p_dec->fmt_out.video.i_visible_width = sizes[4];
                p_dec->fmt_out.video.i_visible_height = sizes[5];
            }
        }

        if (p_dec->fmt_in->video.i_sar_num == 0 || p_dec->fmt_in->video.i_sar_den == 0)
        {
            unsigned num, den;
            if (h266_get_aspect_ratio(p_sps, &num, &den))
            {
                p_dec->fmt_out.video.i_sar_num = num;
                p_dec->fmt_out.video.i_sar_den = den;
            }
        }

        if(p_dec->fmt_out.i_extra == 0 && p_pps)
            SetsToAnnexB(p_sys, p_pps, p_sps, p_vps,
                         (uint8_t **)&p_dec->fmt_out.p_extra, &p_dec->fmt_out.i_extra);
    }
}

static void ParseStoredSEI(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for(block_t *p_nal = p_sys->pre.p_chain;
         p_nal; p_nal = p_nal->p_next)
    {
        if(p_nal->i_buffer < 5)
            continue;

        if(h266_getNALType(&p_nal->p_buffer[4]) == H266_NAL_PREFIX_SEI)
        {
            HxxxParse_AnnexB_SEI(p_nal->p_buffer, p_nal->i_buffer,
                                 2 /* nal header */, ParseSEICallback, p_dec);
        }
    }
}

static void GetXPSSet(uint8_t i_pps_id, void *priv,
                      h266_picture_parameter_set_t **pp_pps,
                      h266_sequence_parameter_set_t **pp_sps,
                      h266_video_parameter_set_t **pp_vps)
{
    decoder_sys_t *p_sys = priv;
    *pp_sps = NULL;
    *pp_vps = NULL;
    if(!(*pp_pps = p_sys->rg_pps[i_pps_id].p_decoded))
        return;
    if(!(*pp_sps = p_sys->rg_sps[h266_get_pps_sps_id(*pp_pps)].p_decoded))
        return;
    uint8_t vps_id = h266_get_sps_vps_id(*pp_sps);
    if(vps_id == 0) /* optional */
        return;
    *pp_vps = p_sys->rg_vps[vps_id - 1].p_decoded;
}

static block_t *ParseVCL(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_outputchain = NULL;
    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if(unlikely(!hxxx_strip_AnnexB_startcode(&p_buffer, &i_buffer) || i_buffer < 3))
    {
        block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_frag); /* might be corrupted */
        return NULL;
    }

    const uint8_t i_layer = h266_getNALLayer(p_buffer);
    const bool sh_picture_header_in_slice_header_flag = p_buffer[2] & 0x80;
    if (sh_picture_header_in_slice_header_flag)
    {
        if(p_sys->frame.p_chain)
        {
            /* Starting new frame: return previous frame data for output */
            p_outputchain = OutputQueues(p_sys, p_sys->sets != MISSING &&
                                                p_sys->b_recovery_point);
        }

        h266_picture_header_t *ph = h266_decode_picture_header(p_buffer, i_buffer, true,
                                                               GetXPSSet, p_sys);
        if(ph && i_layer == 0)
        {
            h266_sequence_parameter_set_t *p_sps;
            h266_picture_parameter_set_t *p_pps;
            h266_video_parameter_set_t *p_vps; /* /!\ optional */
            GetXPSSet(h266_get_picture_header_pps_id(ph), p_sys, &p_pps, &p_sps, &p_vps);
            ActivateSets(p_dec, p_pps, p_sps, p_vps);
        }

        ParseStoredSEI(p_dec);

        switch(i_nal_type)
        {
            case H266_NAL_IDR_W_RADL:
            case H266_NAL_IDR_N_LP:
            case H266_NAL_CRA:
            case H266_NAL_GDR:
                p_frag->i_flags |= BLOCK_FLAG_TYPE_I;
                break;

            default:
            {
                if(ph)
                {
                    enum h266_slice_type_e type;
                    if(h266_get_slice_type(ph, &type))
                    {
                        switch(type)
                        {
                        case H266_SLICE_TYPE_B:
                            p_frag->i_flags |= BLOCK_FLAG_TYPE_B;
                            break;
                        case H266_SLICE_TYPE_P:
                            p_frag->i_flags |= BLOCK_FLAG_TYPE_P;
                            break;
                        case H266_SLICE_TYPE_I:
                            p_frag->i_flags |= BLOCK_FLAG_TYPE_I;
                            break;
                        }
                    }
               }
           }
           break;
        }

        if(ph)
            h266_rbsp_release_picture_header(ph);
    }

    if(p_sys->sets == MISSING && i_layer == 0 && XPSReady(p_sys))
        p_sys->sets = COMPLETE;

    if(p_sys->sets != MISSING && (p_frag->i_flags & BLOCK_FLAG_TYPE_I))
    {
        p_sys->b_recovery_point = true; /* won't care about SEI recovery */
    }

    if(!p_sys->b_recovery_point) /* content will be dropped */
        cc_storage_reset(p_sys->p_ccs);

    block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_frag);

    return p_outputchain;
}

static block_t * ParseAUHead(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_ret = NULL;
    const uint8_t *p_buffer = p_nalb->p_buffer;
    size_t i_buffer = p_nalb->i_buffer;

    if(p_sys->post.p_chain || p_sys->frame.p_chain)
        p_ret = OutputQueues(p_sys, p_sys->sets != MISSING &&
                                    p_sys->b_recovery_point);

    switch(i_nal_type)
    {
        case H266_NAL_AUD:
            if(!p_ret && p_sys->pre.p_chain)
                p_ret = OutputQueues(p_sys, p_sys->sets != MISSING &&
                                            p_sys->b_recovery_point);
            break;

        case H266_NAL_DCI:
            if(p_dec->fmt_out.i_profile == -1 &&
                hxxx_strip_AnnexB_startcode(&p_buffer, &i_buffer))
            {
                h266_decoding_capability_information_t *p_dci =
                    h266_decode_dci(p_buffer, i_buffer, true);
                if(p_dci)
                {
                    uint8_t pl[2];
                    if(h266_get_profile_tier_level(p_dci, NULL, &pl[0], &pl[1]))
                    {
                        p_dec->fmt_out.i_profile = pl[0];
                        p_dec->fmt_out.i_level = pl[1];
                    }
                    h266_rbsp_release_dci(p_dci);
                }
            }
            break;

        case H266_NAL_VPS:
        case H266_NAL_PPS:
        case H266_NAL_SPS:
        {
            uint8_t i_id;
            const uint8_t *p_xps = p_nalb->p_buffer;
            size_t i_xps = p_nalb->i_buffer;
            if(hxxx_strip_AnnexB_startcode(&p_xps, &i_xps) &&
                h266_get_xps_id(p_xps, i_xps, &i_id))
                InsertXPS(p_dec, i_nal_type, i_id, p_nalb);
            if(p_sys->sets != SENT) /* will store/inject on first recovery point */
            {
                block_Release(p_nalb);
                return p_ret;
            }
            break;
        }

        default:
            break;
    }

    block_ChainLastAppend(&p_sys->pre.pp_chain_last, p_nalb);

    return p_ret;
}

static block_t * ParseAUTail(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_ret = NULL;

    block_ChainLastAppend(&p_sys->post.pp_chain_last, p_nalb);

    switch(i_nal_type)
    {
        case H266_NAL_EOS:
        case H266_NAL_EOB:
            p_ret = OutputQueues(p_sys, p_sys->sets != MISSING &&
                                        p_sys->b_recovery_point);
            if(p_ret)
                p_ret->i_flags |= BLOCK_FLAG_END_OF_SEQUENCE;
            break;
    }

    if(!p_ret && p_sys->frame.p_chain == NULL)
        p_ret = OutputQueues(p_sys, false);

    return p_ret;
}

static block_t * ParseNonVCL(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    block_t *p_ret = NULL;

    switch(i_nal_type)
    {
        case H266_NAL_OPI:
        case H266_NAL_DCI:
        case H266_NAL_VPS:
        case H266_NAL_SPS:
        case H266_NAL_PPS:
        case H266_NAL_PREFIX_APS:
        case H266_NAL_SUFFIX_APS:
        case H266_NAL_PH:
        case H266_NAL_AUD:
        case H266_NAL_EOS:
        case H266_NAL_EOB:
        case H266_NAL_PREFIX_SEI:
        case H266_NAL_SUFFIX_SEI:
        case H266_NAL_FD:
        case H266_NAL_RSV_NVCL_26:
        case H266_NAL_RSV_NVCL_27:
        case H266_NAL_UNSPEC_28:
        case H266_NAL_UNSPEC_29:
        case H266_NAL_UNSPEC_30:
        case H266_NAL_UNSPEC_31:
            p_ret = ParseAUHead(p_dec, i_nal_type, p_nalb);
            break;
        default:
            p_ret = ParseAUTail(p_dec, i_nal_type, p_nalb);
            break;
    }

    return p_ret;
}

static block_t *GatherAndValidateChain(block_t *p_outputchain)
{
    block_t *p_output = NULL;

    if(p_outputchain)
    {
        if(p_outputchain->i_flags & BLOCK_FLAG_DROP)
            p_output = p_outputchain; /* Avoid useless gather */
        else
            p_output = block_ChainGather(p_outputchain);
    }

    if(p_output && (p_output->i_flags & BLOCK_FLAG_DROP))
    {
        block_ChainRelease(p_output); /* Chain! see above */
        p_output = NULL;
    }

    return p_output;
}

static void SetOutputBlockProperties(decoder_t *p_dec, block_t *p_output)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    /* Set frame duration */

    const vlc_tick_t i_start = date_Get(&p_sys->dts);
    if(i_start != VLC_TICK_INVALID)
    {
        date_Increment(&p_sys->dts, 2);
        p_output->i_length = date_Get(&p_sys->dts) - i_start;
    }
    p_sys->pts = VLC_TICK_INVALID;

    p_output->i_flags &= ~BLOCK_FLAG_AU_END;
}

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock(decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    *pb_ts_used = false;
    bool b_au_end = p_frag->i_flags & BLOCK_FLAG_AU_END;

    if(p_sys->b_need_ts)
    {
        if(p_frag->i_dts != VLC_TICK_INVALID)
            date_Set(&p_sys->dts, p_frag->i_dts);
        p_sys->pts = p_frag->i_pts;
        if(date_Get(&p_sys->dts) != VLC_TICK_INVALID)
            p_sys->b_need_ts = false;
        *pb_ts_used = true;
    }

    if(unlikely(p_frag->i_buffer < 5))
    {
        msg_Warn(p_dec,"NAL too small");
        block_Release(p_frag);
        return NULL;
    }

    /* Get NALU type */
    const vlc_tick_t dts = p_frag->i_dts, pts = p_frag->i_pts;
    block_t * p_output = NULL;
    uint8_t i_nal_type = h266_getNALType(&p_frag->p_buffer[4]);
    switch(i_nal_type)
    {
        case H266_NAL_IDR_W_RADL:
        case H266_NAL_IDR_N_LP:
        case H266_NAL_CRA:
        case H266_NAL_GDR:
        case H266_NAL_RSV_IRAP_11:
            p_frag->i_flags |= BLOCK_FLAG_TYPE_I;
            // fallthrough
        case H266_NAL_TRAIL:
        case H266_NAL_STSA:
        case H266_NAL_RADL:
        case H266_NAL_RASL:
        case H266_NAL_RSV_VCL_4:
        case H266_NAL_RSV_VCL_5:
        case H266_NAL_RSV_VCL_6:
            /* NAL is a VCL NAL */
            p_output = ParseVCL(p_dec, i_nal_type, p_frag);
            if (p_output && (p_output->i_flags & BLOCK_FLAG_DROP))
                msg_Info(p_dec, "Waiting for SPS/PPS");
            break;
        default:
            p_output = ParseNonVCL(p_dec, i_nal_type, p_frag);
            break;
    }

    if(!p_output && b_au_end)
        p_output = OutputQueues(p_sys, p_sys->sets != MISSING &&
                                       p_sys->b_recovery_point);

    p_output = GatherAndValidateChain(p_output);
    if(p_output)
    {
        if(p_sys->sets != SENT)
        {
            assert(p_sys->sets == COMPLETE);
            p_sys->sets = SENT;
        }
        SetOutputBlockProperties(p_dec, p_output);
        if (dts != VLC_TICK_INVALID && dts > date_Get(&p_sys->dts))
            date_Set(&p_sys->dts, dts);
        p_sys->pts = pts;
        *pb_ts_used = true;
    }

    return p_output;
}

static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *p_block)
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;
    VLC_UNUSED(p_sys);

    /* Remove trailing 0 bytes */
    while (p_block->i_buffer > 5 && p_block->p_buffer[p_block->i_buffer-1] == 0x00)
        p_block->i_buffer--;

    p_block = ParseNALBlock(p_dec, pb_ts_used, p_block);

    return p_block;
}

static int PacketizeValidate(void *p_private, block_t *p_au)
{
    VLC_UNUSED(p_private);
    VLC_UNUSED(p_au);
    return VLC_SUCCESS;
}

static block_t * PacketizeDrain(void *p_private)
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t *p_out = NULL;

    if(p_sys->frame.p_chain)
    {
        p_out = OutputQueues(p_sys, p_sys->sets != MISSING &&
                                    p_sys->b_recovery_point);
        if(p_out)
        {
            p_out = GatherAndValidateChain(p_out);
            if(p_out)
                SetOutputBlockProperties(p_dec, p_out);
        }
    }
    return p_out;
}

static bool ParseSEICallback(const hxxx_sei_data_t *p_sei_data, void *cbdata)
{
    decoder_t *p_dec = (decoder_t *) cbdata;
    decoder_sys_t *p_sys = p_dec->p_sys;

    switch(p_sei_data->i_type)
    {
    case HXXX_SEI_USER_DATA_REGISTERED_ITU_T_T35:
    {
        if(p_sei_data->itu_t35.type == HXXX_ITU_T35_TYPE_CC)
        {
            cc_storage_append(p_sys->p_ccs, true, p_sei_data->itu_t35.u.cc.p_data,
                              p_sei_data->itu_t35.u.cc.i_data);
        }
    } break;
    case HXXX_SEI_FRAME_PACKING_ARRANGEMENT:
    {
        if(p_dec->fmt_in->video.multiview_mode == MULTIVIEW_2D)
        {
            video_multiview_mode_t mode;
            switch(p_sei_data->frame_packing.type)
            {
            case FRAME_PACKING_INTERLEAVED_CHECKERBOARD:
                mode = MULTIVIEW_STEREO_CHECKERBOARD; break;
            case FRAME_PACKING_INTERLEAVED_COLUMN:
                mode = MULTIVIEW_STEREO_COL; break;
            case FRAME_PACKING_INTERLEAVED_ROW:
                mode = MULTIVIEW_STEREO_ROW; break;
            case FRAME_PACKING_SIDE_BY_SIDE:
                mode = MULTIVIEW_STEREO_SBS; break;
            case FRAME_PACKING_TOP_BOTTOM:
                mode = MULTIVIEW_STEREO_TB; break;
            case FRAME_PACKING_TEMPORAL:
                mode = MULTIVIEW_STEREO_FRAME; break;
            case FRAME_PACKING_TILED:
            default:
                mode = MULTIVIEW_2D; break;
            }
            p_dec->fmt_out.video.multiview_mode = mode;
        }
    } break;
    case HXXX_SEI_MASTERING_DISPLAY_COLOUR_VOLUME:
    {
        video_format_t *p_fmt = &p_dec->fmt_out.video;
        for (size_t i=0; i<ARRAY_SIZE(p_sei_data->colour_volume.primaries); ++i)
            p_fmt->mastering.primaries[i] = p_sei_data->colour_volume.primaries[i];
        for (size_t i=0; i<ARRAY_SIZE(p_sei_data->colour_volume.white_point); ++i)
            p_fmt->mastering.white_point[i] = p_sei_data->colour_volume.white_point[i];
        p_fmt->mastering.max_luminance = p_sei_data->colour_volume.max_luminance;
        p_fmt->mastering.min_luminance = p_sei_data->colour_volume.min_luminance;
    } break;
    case HXXX_SEI_CONTENT_LIGHT_LEVEL:
    {
        video_format_t *p_fmt = &p_dec->fmt_out.video;
        p_fmt->lighting.MaxCLL = p_sei_data->content_light_lvl.MaxCLL;
        p_fmt->lighting.MaxFALL = p_sei_data->content_light_lvl.MaxFALL;
    } break;
    }

    return true;
}
