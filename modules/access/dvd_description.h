/*****************************************************************************
 * dvd_description.h: DVD audio/subtitle lang_extension description tables
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

/**
 * DVD audio track lang_extension values as defined in the DVD-Video spec.
 * Maps audio_attr_t.lang_extension to translatable description strings.
 */
static const char *const dvd_audio_lang_ext[] = {
    /* 0 */ NULL,
    /* 1 */ NULL,
    /* 2 */ N_("Audio for visually impaired"),
    /* 3 */ N_("Director's comments"),
    /* 4 */ N_("Alternate director's comments"),
};

/**
 * DVD subtitle track lang_extension values as defined in the DVD-Video spec.
 * Maps subp_attr_t.lang_extension to translatable description strings.
 */
static const char *const dvd_spu_lang_ext[] = {
    /* 0 */ NULL,
    /* 1 */ N_("Caption"),
    /* 2 */ N_("Caption (large)"),
    /* 3 */ N_("Caption (children)"),
    /* 4 */ NULL,
    /* 5 */ N_("Closed caption"),
    /* 6 */ N_("Closed caption (large)"),
    /* 7 */ N_("Closed caption (children)"),
    /* 8 */ NULL,
    /* 9 */ N_("Forced caption"),
    /* 10 */ NULL,
    /* 11 */ NULL,
    /* 12 */ NULL,
    /* 13 */ N_("Director's comments"),
    /* 14 */ N_("Director's comments (large)"),
    /* 15 */ N_("Director's comments (children)"),
};
