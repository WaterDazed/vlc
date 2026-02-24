/*****************************************************************************
 * time.c: LibVLC time API implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc/libvlc_time.h>

#include <math.h>
#include <stdckdint.h>

#include "libvlc_internal.h"

/* safely compute (value * num) / den avoiding intermediate overflow and rounding.
   The function handles the arithmetic in parts, factoring out the GCD between
   numerator and denominator to minimize the chance of overflow.
   Returns INT64_MIN on error (division by zero or overflow). */
static int64_t scale_safediv(int64_t value, uint32_t num, uint32_t den)
{
    if (den == 0)
        return INT64_MIN;

    if (value == 0 || num == 0)
        return 0;

    uint32_t g = (uint32_t)GCD(den, num);
    uint32_t ts = den / g;
    uint32_t mul = num / g;

    int64_t q = value / ts;
    int64_t r = value % ts;

    int64_t res_hi;
    if (ckd_mul(&res_hi, q, mul))
        return INT64_MIN;

    int64_t tmp;
    if (ckd_mul(&tmp, r, mul))
        return INT64_MIN;

    int64_t res_lo = tmp / ts;

    /* computes (value * num) / den as q*mul + (r*mul)/ts with
       value = q*ts + r, reducing overflow risk. */
    int64_t res;
    if (ckd_add(&res, res_hi, res_lo))
        return INT64_MIN;

    return res;
}

libvlc_time_t libvlc_time_make(int64_t value, uint32_t timescale)
{
    if (timescale == 0)
        return LIBVLC_TIME_INVALID;

    return (libvlc_time_t){value, timescale};
}

libvlc_time_t libvlc_time_make_with_seconds(double seconds, uint32_t timescale)
{
    if (timescale == 0 || isinf(seconds) || isnan(seconds))
        return LIBVLC_TIME_INVALID;

    if (seconds > (double)INT64_MAX / timescale ||
        seconds < (double)INT64_MIN / timescale)
        return LIBVLC_TIME_INVALID;

    double scaled = round(seconds * timescale);

    if (scaled > (double)INT64_MAX || scaled < (double)INT64_MIN)
        return LIBVLC_TIME_INVALID;

    int64_t value = (int64_t)scaled;

    return (libvlc_time_t){value, timescale};
}

libvlc_time_t libvlc_time_from_seconds(double seconds)
{
    return libvlc_time_make_with_seconds(seconds, LIBVLC_TIME_DEFAULT_TIMESCALE);
}

libvlc_time_t libvlc_time_from_milliseconds(int64_t milliseconds)
{
    return libvlc_time_make(milliseconds, LIBVLC_TIME_TIMESCALE_MS);
}

libvlc_time_t libvlc_time_from_microseconds(int64_t microseconds)
{
    /* default timescale is microseconds per second, so value == microseconds */
    return libvlc_time_make(microseconds, LIBVLC_TIME_DEFAULT_TIMESCALE);
}

double libvlc_time_to_seconds(libvlc_time_t time)
{
    if (!LIBVLC_TIME_IS_VALID(time))
        return NAN;

    return (double)time.value / (double)time.timescale;
}

int64_t libvlc_time_to_milliseconds(libvlc_time_t time)
{
    if (!LIBVLC_TIME_IS_VALID(time))
        return INT64_MIN;
    return scale_safediv(time.value, 1000, time.timescale);
}

int64_t libvlc_time_to_microseconds(libvlc_time_t time)
{
    if (!LIBVLC_TIME_IS_VALID(time))
        return INT64_MIN;
    return scale_safediv(time.value, 1000000, time.timescale);
}

libvlc_time_t libvlc_time_convert_scale(libvlc_time_t time, uint32_t new_timescale)
{
    if (!LIBVLC_TIME_IS_VALID(time) || new_timescale == 0)
        return LIBVLC_TIME_INVALID;

    if (time.timescale == new_timescale)
        return time;

    int64_t new_value = scale_safediv(time.value, new_timescale, time.timescale);
    if (new_value == INT64_MIN)
        return LIBVLC_TIME_INVALID;

    return (libvlc_time_t){new_value, new_timescale};
}

libvlc_time_t libvlc_clock(void)
{
    return libvlc_time_from_vlc_tick(vlc_tick_now());
}
