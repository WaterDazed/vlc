/*****************************************************************************
 * checkasm.c: assembly test driver for VLC
 *****************************************************************************
 * Copyright (C) 2026 the VideoLAN team
 *
 * Authors: Ahmed Metwally <t.a.metwally35@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <checkasm/checkasm.h>
#include "vlc_checkasm.h"

/* List of tests to invoke */
static const CheckasmTest tests[] = {
    { 0 }
};

/* List of cpu flags to check */
static const CheckasmCpuInfo cpu_flags[] = {
#if defined(__i386__) || defined(__x86_64__)
    { "SSE2",   "sse2",   VLC_CPU_SSE2   },
    { "SSSE3",  "ssse3",  VLC_CPU_SSSE3  },
    { "SSE4.1", "sse41",  VLC_CPU_SSE4_1 },
    { "AVX",    "avx",    VLC_CPU_AVX    },
    { "AVX2",   "avx2",   VLC_CPU_AVX2   },
#elif defined(__aarch64__)
    { "NEON",   "neon",   VLC_CPU_ARM_NEON },
    { "SVE",    "sve",    VLC_CPU_ARM_SVE  },
#elif defined(__arm__)
    { "ARMv6",  "armv6",  VLC_CPU_ARMv6    },
    { "NEON",   "neon",   VLC_CPU_ARM_NEON },
#elif defined(__powerpc__) || defined(__powerpc64__)
    { "AltiVec", "altivec", VLC_CPU_ALTIVEC },
#elif defined(__riscv)
    { "RVV",    "rvv",    VLC_CPU_RV_V },
    { "RVB",    "rvb",    VLC_CPU_RV_B },
#endif
    { 0 }
};

static void set_cpu_flags(CheckasmCpu flags)
{
    vlc_CPU_SetMask((unsigned) flags);
}

int main(int argc, const char *argv[])
{
    unsigned cpu = vlc_CPU();

    CheckasmConfig cfg = {
        .cpu_flags     = cpu_flags,
        .tests         = tests,
        .cpu           = cpu,
        .set_cpu_flags = set_cpu_flags,
    };
    return checkasm_main(&cfg, argc, argv);
}
