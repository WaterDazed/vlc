/*****************************************************************************
 * display.h: "vout display" management
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_vout_wrapper.h>

vout_display_t *vout_NewSplitter(vout_thread_t *vout,
                                 const video_format_t *source,
                                 const vout_display_state_t *state,
                                 const char *module,
                                 const char *splitter_module,
                                 vlc_tick_t double_click_timeout,
                                 vlc_tick_t hide_timeout);

/* FIXME should not be there */
void vout_SendDisplayEventMouse(vout_thread_t *, const vlc_mouse_t *);

vout_window_t *vout_NewDisplayWindow(vout_thread_t *, unsigned type);
void vout_DeleteDisplayWindow(vout_thread_t *, vout_window_t *);
void vout_SetDisplayWindowSize(vout_thread_t *, unsigned, unsigned);
int  vout_HideWindowMouse(vout_thread_t *, bool);

void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *);

#ifdef HAVE_LIBLCMS2
#include <lcms2.h>
static const int ICC_3D_LUT_SIZE_MAX = 256;
static const float ICC_SDR_MASTER_LUM = 100.0;
static const float ICC_SDR_MASTER_BP = 0.1;
static const float ICC_HDR_MASTER_LUM = 1000.0;
static const float ICC_HDR_MASTER_BP = 0.005;
static const float ICC_HDR_LUM_THRESH = 500.0;
static const int ICC_TRC_TAB_SIZE = 65536;

typedef struct icc_eotf_data {
    size_t  trc_size;
    float *trc;
    float master_lum; float master_bp;
    float target_lum; float target_bp;
} icc_eotf_data;

static const cmsCIExyY icc_d65_wp = { 0.3127, 0.3290, 1.0 };

static const cmsCIExyYTRIPLE icc_bt709_prim = { { 0.640, 0.330, 1.00 },
                                                { 0.300, 0.600, 1.00 },
                                                { 0.150, 0.060, 1.00 } };

static const cmsCIExyYTRIPLE icc_bt601_525_prim = { { 0.630, 0.340, 1.00 },
                                                    { 0.310, 0.595, 1.00 },
                                                    { 0.155, 0.070, 1.00 } };

static const cmsCIExyYTRIPLE icc_bt601_625_prim = { { 0.640, 0.330, 1.00 },
                                                    { 0.290, 0.600, 1.00 },
                                                    { 0.150, 0.060, 1.00 } };

static const cmsCIExyYTRIPLE icc_bt2020_prim = { { 0.708, 0.292, 1.0 },
                                                 { 0.170, 0.797, 1.0 },
                                                 { 0.131, 0.046, 1.0 } };

static const cmsCIExyYTRIPLE icc_dcip3_prim = { { 0.68,  0.32, 1.0 },
                                                { 0.265, 0.69, 1.0 },
                                                { 0.15 , 0.06, 1.0 } };
                                                
static enum icc_bp_mode {
    ICC_BP_MODE_DEFAULT = 0,
    ICC_BP_MODE_USER = 1
};

static enum icc_intent {
    ICC_INTENT_PERCEPTUAL = 0,
    ICC_INTENT_RELATIVE_COLORIMETRIC = 1,
    ICC_INTENT_SATURATION = 2,
    ICC_INTENT_ABSOLUTE_COLORIMETRIC = 3
};
                                                
void icc_set_eotf_srgb( icc_eotf_data *eotf_data );
void icc_set_eotf_pq( icc_eotf_data *eotf_data );
void icc_set_eotf_hlg( icc_eotf_data *eotf_data );
void icc_set_eotf_bt1886( icc_eotf_data *eotf_data );
void icc_hdr_to_hdr_lum_bt2408( icc_eotf_data *eotf_data );
float icc_eotf_pq( float x );
float icc_hermite_spl( float x, float x0, float p0, float m0,
                                float x1, float p1, float m1 );
int SetCorrectionLUT( uint16_t *lut, vout_display_t *vd,
                      const video_format_t *fmt, const char *filename );
#endif
