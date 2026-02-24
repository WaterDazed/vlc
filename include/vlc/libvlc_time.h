/*****************************************************************************
 * libvlc_time.h:  libvlc time API
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#ifndef LIBVLC_TIME_H
#define LIBVLC_TIME_H 1

#include <vlc/libvlc.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * @defgroup libvlc_time LibVLC time
 * @ingroup libvlc
 * @{
 * @file
 * LibVLC time API
 */

/**
 * Rational time representation structure
 *
 * Represents time as value/timescale where:
 * - value: numerator (number of ticks)
 * - timescale: denominator (ticks per second)
 *
 * Special values:
 * - Invalid time: timescale = 0
 * - Zero time: value = 0, timescale > 0
 */
typedef struct libvlc_time_t libvlc_time_t;

/**
 * Common timescales
 */
#define LIBVLC_TIME_TIMESCALE_MS   1000    /* Milliseconds */
#define LIBVLC_TIME_TIMESCALE_US   1000000 /* Microseconds */
#define LIBVLC_TIME_TIMESCALE_CD   44100   /* CD audio sample rate */
#define LIBVLC_TIME_TIMESCALE_DVD  48000   /* DVD audio sample rate */
#define LIBVLC_TIME_TIMESCALE_MPEG 90000   /* MPEG video sample rate */

/**
 * Default VLC timescale
 */
#define LIBVLC_TIME_DEFAULT_TIMESCALE LIBVLC_TIME_TIMESCALE_US

/**
 * Special time constants
 */
#define LIBVLC_TIME_ZERO    ((libvlc_time_t){0, LIBVLC_TIME_DEFAULT_TIMESCALE})
#define LIBVLC_TIME_INVALID ((libvlc_time_t){0, 0})
#define LIBVLC_TIME_MAX     ((libvlc_time_t){INT64_MAX, LIBVLC_TIME_DEFAULT_TIMESCALE})

/**
 * Check if time is valid
 */
#define LIBVLC_TIME_IS_VALID(time) ((time).timescale > 0)

/**
 * Check if time is zero
 */
#define LIBVLC_TIME_IS_ZERO(time) (LIBVLC_TIME_IS_VALID(time) && (time).value == 0)

struct libvlc_time_t
{
    int64_t value;
    uint32_t timescale;
};

/**
 * Direct time creation
 * 
 * \param value Number of ticks
 * \param timescale Ticks per second (must be > 0)
 * \return libvlc_time_t structure, or LIBVLC_TIME_INVALID if timescale == 0
 */
LIBVLC_API libvlc_time_t libvlc_time_make(int64_t value, uint32_t timescale);

/**
 * Create time from seconds with specified timescale
 * 
 * \param seconds Time in seconds
 * \param timescale Required timescale (must be > 0)
 * \return libvlc_time_t structure, or LIBVLC_TIME_INVALID on overflow or invalid timescale
 */
LIBVLC_API libvlc_time_t libvlc_time_make_with_seconds(double seconds, uint32_t timescale);

/**
 * Create time from seconds with default timescale
 *
 * \param seconds Time in seconds
 * \return libvlc_time_t structure, or LIBVLC_TIME_INVALID on overflow
 */
LIBVLC_API libvlc_time_t libvlc_time_from_seconds(double seconds);

/**
 * Create time from milliseconds with default timescale
 *
 * \param ms Time in milliseconds
 * \return libvlc_time_t structure
 */
LIBVLC_API libvlc_time_t libvlc_time_from_milliseconds(int64_t ms);

/**
 * Create time from microseconds with default timescale
 *
 * \param us Time in microseconds
 * \return libvlc_time_t structure
 */
LIBVLC_API libvlc_time_t libvlc_time_from_microseconds(int64_t us);

/**
 * Convert libvlc_time_t to seconds
 * 
 * \param time libvlc_time_t
 * \return Time in seconds, or NaN if time is invalid
 */
LIBVLC_API double libvlc_time_to_seconds(libvlc_time_t time);

/**
 * Convert libvlc_time_t to milliseconds
 * 
 * \param time libvlc_time_t
 * \return Time in milliseconds, or INT64_MIN if time is invalid or overflow occurs
 */
LIBVLC_API int64_t libvlc_time_to_milliseconds(libvlc_time_t time);

/**
 * Convert libvlc_time_t to microseconds
 *
 * \param time libvlc_time_t
 * \return Time in microseconds, or INT64_MIN if time is invalid or overflow occurs
 */
LIBVLC_API int64_t libvlc_time_to_microseconds(libvlc_time_t time);

/**
 * Convert libvlc_time_t to a different timescale
 *
 * \param time libvlc_time_t to convert
 * \param new_timescale target timescale (must be > 0)
 * \return libvlc_time_t with new timescale, or LIBVLC_TIME_INVALID on overflow or invalid parameters
 */
LIBVLC_API libvlc_time_t libvlc_time_convert_scale(libvlc_time_t time, uint32_t new_timescale);

/** @} */

/** \defgroup libvlc_clock LibVLC time
 * These functions provide access to the LibVLC time/clock.
 * @{
 */

/**
 * Return the current time as defined by LibVLC.
 *
 * The unit is libvlc_time_t with default timescale of microseconds.
 * Time increases monotonically (regardless of time zone changes and RTC
 * adjustments).
 * The origin is arbitrary but consistent across the whole system
 * (e.g. system uptime).
 * \note On systems that support it, the POSIX monotonic clock is used.
 */
LIBVLC_API
libvlc_time_t libvlc_clock(void);

/**
 * Return the delay until a certain timestamp.
 *
 * \param pts timestamp (a valid libvlc_time_t)
 * \return delay as libvlc_time_t in pts.timescale; LIBVLC_TIME_INVALID if pts is invalid
 */
static inline libvlc_time_t libvlc_delay(libvlc_time_t pts)
{
    if (!LIBVLC_TIME_IS_VALID(pts))
        return LIBVLC_TIME_INVALID;

    libvlc_time_t now = libvlc_clock();

    /* convert "now" to the same timescale as pts */
    libvlc_time_t now_scaled = now;
    if (now.timescale != pts.timescale)
    {
        now_scaled = libvlc_time_convert_scale(now, pts.timescale);
        if (!LIBVLC_TIME_IS_VALID(now_scaled))
            return LIBVLC_TIME_INVALID;
    }

    int64_t value = pts.value - now_scaled.value;
    return libvlc_time_make(value, pts.timescale);
}

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* LIBVLC_TIME_H */
