/*****************************************************************************
 * avaudio_lists.h :
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

#ifndef VLC_AVAUDIO_LISTS_H_
#define VLC_AVAUDIO_LISTS_H_

/*****************************************************************************
 * aemphasis : mode, type
 *****************************************************************************/

#define NB_AEMPHASIS_MODES 2
static const char *const aemphasis_mode_list[NB_AEMPHASIS_MODES] = {
    "reproduction", "production"
};
static const char *const aemphasis_mode_list_text[NB_AEMPHASIS_MODES] = {
    N_("reproduction"),
    N_("production")
};

#define NB_AEMPHASIS_TYPES 9
static const char *const aemphasis_type_list[NB_AEMPHASIS_TYPES] = {
    "cd", "emi", "bsi", 
    "riaa", "col", "50fm", 
    "75fm", "50kf", "75kf" 
};
static const char *const aemphasis_type_list_text[NB_AEMPHASIS_TYPES] = {
    N_("cd"), 
    N_("emi"), 
    N_("bsi"),
    N_("riaa"), 
    N_("col"), 
    N_("50fm"),
    N_("75fm"), 
    N_("50kf"), 
    N_("75kf")
};


/*****************************************************************************
 * agate : detection, link
 *****************************************************************************/

#define NB_AGATE_DETECTION 2
static const char *const agate_detection_list[NB_AGATE_DETECTION] = {
    "rms", "peak"
};
static const char *const agate_detection_list_text[NB_AGATE_DETECTION] = {
    N_("rms"), 
    N_("peak")
};

#define NB_AGATE_LINK 2
static const char *const agate_link_list[NB_AGATE_LINK] = {
    "average", "maximum"
};
static const char *const agate_link_list_text[NB_AGATE_LINK] = {
    N_("average"), 
    N_("maximum")
};


/*****************************************************************************
 * Asoftclip: soft-clip type
 *****************************************************************************/

#define NB_ASOFTCLIP_TYPES 7
static const char *const softclip_type_list[NB_ASOFTCLIP_TYPES] = {
    "exp", "atan", "cubic", "tanh",
    "alg", "quintic", "sin"
};
static const char *const softclip_type_list_text[NB_ASOFTCLIP_TYPES] = {
    N_("exp"),
    N_("atan"),
    N_("cubic"),
    N_("tanh"),
    N_("alg"),
    N_("quintic"),
    N_("sin")
};


/*****************************************************************************
 * aiir: 
 *****************************************************************************/

static const char *const aiir_list[] = {

    "k=1.5:z=0.10498 -2.1 3.035 -2.15 0.10502:p=5 -2.64 3 -2.62 1:f=tf:r=d",
    
    "k=0.79575848078096756:dry=0.3"
    ":z=0.80918701+0.58773007i 0.80918701-0.58773007i 0.80884700+0.58784055i 0.80884700-0.58784055i"
    ":p=0.63892345+0.59951235i 0.63892345-0.59951235i 0.79582691+0.44198673i 0.79582691-0.44198673i:f=zp:r=s",
    
    "k=0.79575848078096756:dry=0.3:wet=0.6"
    ":z=0.80918701+0.58773007i 0.80918701-0.58773007i 0.80884700+0.58784055i 0.80884700-0.58784055i"
    ":p=0.63892345+0.59951235i 0.63892345-0.59951235i 0.79582691+0.44198673i 0.79582691-0.44198673i:f=zp:r=s",

    "k=0.79575848078096756:dry=0.8:wet=0.29"
    ":z=0.80918701+0.58773007i 0.80918701-0.58773007i 0.80884700+0.58784055i 0.80884700-0.58784055i"
    ":p=0.63892345+0.59951235i 0.63892345-0.59951235i 0.79582691+0.44198673i 0.79582691-0.44198673i:f=zp:r=s",
    
    "k=1:dry=0.1:wet=0.82"
    ":z=7.957584807809675810E-1 -2.575128568908332300 3.674839853930788710 -2.57512875289799137 7.957586296317130880E-1"
    ":p=1 -2.86950072432325953 3.63022088054647218 -2.28075678147272232 6.361362326477423500E-1:f=tf:r=d",

    "k=3:dry=0.78:wet=0.2"
    ":z=7.957584807809675810E-1 -2.575128568908332300 3.674839853930788710 -2.57512875289799137 7.957586296317130880E-1"
    ":p=1 -2.86950072432325953 3.63022088054647218 -2.28075678147272232 6.361362326477423500E-1:f=tf:r=d",
};
static const char *const aiir_list_text[] = {
    N_("aiir 1"),
    N_("aiir 2"),
    N_("aiir 3"),
    N_("aiir 4"),
    N_("aiir 5"),
    N_("aiir 6"),
};


/*****************************************************************************
 * Apulsator : mode, timing
 *****************************************************************************/

#define NB_APULSATOR_MODES 5
static const char *const apulsator_mode_list[NB_APULSATOR_MODES] = {
    "sine", "triangle", "square", "sawup", "sawdown"
};
static const char *const apulsator_mode_list_text[NB_APULSATOR_MODES] = {
    N_("sine"), 
    N_("triangle"), 
    N_("square"),
    N_("sawup"),
    N_("sawdown")
};

#define NB_APULSATOR_TIMING 3
static const char *const apulsator_timing_list[NB_APULSATOR_TIMING] = {
    "bpm", "ms","hz"
};
static const char *const apulsator_timing_list_text[NB_APULSATOR_TIMING] = {
    N_("bpm"), 
    N_("ms"), 
    N_("hz")
};


/*****************************************************************************
 * bs2b: bs2b-profile
 *****************************************************************************/

#define NB_PROFILES 3
static const char *const bs2b_profile_list[NB_PROFILES] = {
    "default", "cmoy", "jmeier"
};
static const char *const bs2b_profile_list_text[NB_PROFILES] = {
    N_("default"),
    N_("cmoy"),
    N_("jmeier")
};


/*****************************************************************************
 * Deesser: output mode.
 *****************************************************************************/

#define NB_DEESSER_MODES 3
static const char *const deesser_mode_list[NB_DEESSER_MODES] = {
    "i", "o", "e"
};
static const char *const deesser_mode_list_text[NB_DEESSER_MODES] = {
    N_("Pass input unchanged"),
    N_("Pass ess filtered out"),
    N_("Pass only ess")
};


/*****************************************************************************
 * flanger : shape, interp
 *****************************************************************************/

#define NB_FLANGER_SHAPE 2
static const char *const flanger_shape_list[NB_FLANGER_SHAPE] = {
    "sinusoidal", "triangular"
};
static const char *const flanger_shape_list_text[NB_FLANGER_SHAPE] = {
    N_("sinusoidal"), 
    N_("triangular")
};

#define NB_FLANGER_INTERP 2
static const char *const flanger_interp_list[NB_FLANGER_INTERP] = {
    "linear", "quadratic"
};
static const char *const flanger_interp_list_text[NB_FLANGER_INTERP] = {
    N_("Linear"), 
    N_("Quadratic")
};


/*****************************************************************************
 * lowpass, bandpass, bandreject, highpass, treble : width_type
 *****************************************************************************/

#define NB_WIDTH_TYPES 5
static const char *const lowpass_width_type_list[] = {
    "h", "q", "o", "s", "k"
};
static const char *const lowpass_width_type_list_text[] = {
    N_("Hz"), 
    N_("Q-Factor"), 
    N_("octave"), 
    N_("slope"), 
    N_("kHz")
};


#endif

