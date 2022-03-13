/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libswscale/swscale.h"
#include "libswscale/swscale_internal.h"
#include "libavutil/attributes.h"
#include "libavutil/cpu.h"
#include "libavutil/x86/cpu.h"

extern void ff_v210_decode_unaligned_10_ssse3(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_unaligned_10_avx(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_unaligned_10_avx2(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);

extern void ff_v210_decode_aligned_10_ssse3(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_aligned_10_avx(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_aligned_10_avx2(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);

extern void ff_v210_decode_unaligned_16_ssse3(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_unaligned_16_avx(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_unaligned_16_avx2(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);

extern void ff_v210_decode_aligned_16_ssse3(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_aligned_16_avx(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);
extern void ff_v210_decode_aligned_16_avx2(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width);

extern void ff_v210_encode_8_ssse3(const uint8_t *y, const uint8_t *u, const uint8_t *v, uint8_t *dst, ptrdiff_t width);
extern void ff_v210_encode_8_avx(const uint8_t *y, const uint8_t *u, const uint8_t *v, uint8_t *dst, ptrdiff_t width);
extern void ff_v210_encode_8_avx2(const uint8_t *y, const uint8_t *u, const uint8_t *v, uint8_t *dst, ptrdiff_t width);
extern void ff_v210_encode_10_ssse3(const uint16_t *y, const uint16_t *u, const uint16_t *v, uint8_t *dst, ptrdiff_t width);
extern void ff_v210_encode_10_avx2(const uint16_t *y, const uint16_t *u, const uint16_t *v, uint8_t *dst, ptrdiff_t width);
extern void ff_v210_encode_16_ssse3(const uint16_t *y, const uint16_t *u, const uint16_t *v, uint8_t *dst, ptrdiff_t width);
extern void ff_v210_encode_16_avx2(const uint16_t *y, const uint16_t *u, const uint16_t *v, uint8_t *dst, ptrdiff_t width);

av_cold void ff_sws_init_v210_x86(SwsContext *c)
{
#if HAVE_X86ASM
    int cpu_flags = av_get_cpu_flags();

    if (c->v210_aligned_input) {
        if (cpu_flags & AV_CPU_FLAG_SSSE3)
            c->v210_decode_line = c->dstBpc > 10 ?
            ff_v210_decode_aligned_16_ssse3 :
            ff_v210_decode_aligned_10_ssse3;

        if (HAVE_AVX_EXTERNAL && cpu_flags & AV_CPU_FLAG_AVX)
            c->v210_decode_line = c->dstBpc > 10 ?
            ff_v210_decode_aligned_16_avx :
            ff_v210_decode_aligned_10_avx;

        if (HAVE_AVX2_EXTERNAL && cpu_flags & AV_CPU_FLAG_AVX2)
            c->v210_decode_line = c->dstBpc > 10 ?
            ff_v210_decode_aligned_16_avx2 :
            ff_v210_decode_aligned_10_avx2;
    } else {
        if (cpu_flags & AV_CPU_FLAG_SSSE3)
            c->v210_decode_line = c->dstBpc > 10 ?
            ff_v210_decode_unaligned_16_ssse3 :
            ff_v210_decode_unaligned_10_ssse3;

        if (HAVE_AVX_EXTERNAL && cpu_flags & AV_CPU_FLAG_AVX)
            c->v210_decode_line = c->dstBpc > 10 ?
            ff_v210_decode_unaligned_16_avx :
            ff_v210_decode_unaligned_10_avx;

        if (HAVE_AVX2_EXTERNAL && cpu_flags & AV_CPU_FLAG_AVX2)
            c->v210_decode_line = c->dstBpc > 10 ?
            ff_v210_decode_unaligned_16_avx2 :
            ff_v210_decode_unaligned_10_avx2;
    }

    if (EXTERNAL_SSSE3(cpu_flags)) {
        c->v210_encode_line_8 = ff_v210_encode_8_ssse3;
        c->v210_encode_line_10 = ff_v210_encode_10_ssse3;
        c->v210_encode_line_16 = ff_v210_encode_16_ssse3;
    }

    if (EXTERNAL_AVX(cpu_flags))
        c->v210_encode_line_8 = ff_v210_encode_8_avx;

    if (EXTERNAL_AVX2(cpu_flags)) {
        c->v210_sample_factor_8 = 2;
        c->v210_encode_line_8 = ff_v210_encode_8_avx2;
        c->v210_sample_factor_10 = 2;
        c->v210_encode_line_10 = ff_v210_encode_10_avx2;
        c->v210_sample_factor_16 = 2;
        c->v210_encode_line_16 = ff_v210_encode_16_avx2;
    }
#endif
}
