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
#include "libavutil/bswap.h"
#include "libavutil/intreadwrite.h"

#define READ_PIXELS(a, b, c, bitdepth)                 \
    do {                                               \
        val  = av_le2ne32(*src++);                     \
        *a++ = (val & 0x000003FF) << (bitdepth - 10);  \
        *b++ = (val & 0x000FFC00) >> (20 - bitdepth);  \
        *c++ = (val & 0x3FF00000) >> (30 - bitdepth);  \
    } while (0)

static void v210_decode_line_10_c(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width)
{
    uint32_t val;
    int i;

    for (i = 0; i < width - 5; i += 6) {
        READ_PIXELS(u, y, v, 10);
        READ_PIXELS(y, u, y, 10);
        READ_PIXELS(v, y, u, 10);
        READ_PIXELS(y, v, y, 10);
    }
}

static void v210_decode_line_16_c(const uint32_t *src, uint16_t *y, uint16_t *u, uint16_t *v, ptrdiff_t width)
{
    uint32_t val;
    int i;

    for (i = 0; i < width - 5; i += 6) {
        READ_PIXELS(u, y, v, 16);
        READ_PIXELS(y, u, y, 16);
        READ_PIXELS(v, y, u, 16);
        READ_PIXELS(y, v, y, 16);
    }
}

#define TYPE uint8_t
#define DEPTH 8
#define BYTES_PER_PIXEL 1
#define RENAME(a) a ## _ ## 8
#include "v210_template.c"
static void v210_encode_line_8_c(const uint8_t *y, const uint8_t *u, const uint8_t *v, uint8_t *dst, ptrdiff_t width)
{
    uint32_t val;
    int i;

    /* unroll this to match the assembly */
    for (i = 0; i < width - 11; i += 12) {
        WRITE_PIXELS(u, y, v, DEPTH);
        WRITE_PIXELS(y, u, y, DEPTH);
        WRITE_PIXELS(v, y, u, DEPTH);
        WRITE_PIXELS(y, v, y, DEPTH);
        WRITE_PIXELS(u, y, v, DEPTH);
        WRITE_PIXELS(y, u, y, DEPTH);
        WRITE_PIXELS(v, y, u, DEPTH);
        WRITE_PIXELS(y, v, y, DEPTH);
    }
}
#undef RENAME
#undef DEPTH
#undef BYTES_PER_PIXEL
#undef TYPE

#define TYPE uint16_t
#define DEPTH 10
#define BYTES_PER_PIXEL 2
#define RENAME(a) a ## _ ## 10
#include "v210_template.c"
static void v210_encode_line_10_c(const uint16_t *y, const uint16_t *u, const uint16_t *v, uint8_t *dst, ptrdiff_t width)
{
    uint32_t val;
    int i;

    for (i = 0; i < width - 5; i += 6) {
        WRITE_PIXELS(u, y, v, DEPTH);
        WRITE_PIXELS(y, u, y, DEPTH);
        WRITE_PIXELS(v, y, u, DEPTH);
        WRITE_PIXELS(y, v, y, DEPTH);
    }
}
#undef RENAME
#undef DEPTH
#undef BYTES_PER_PIXEL
#undef TYPE

#define TYPE uint16_t
#define DEPTH 16
#define BYTES_PER_PIXEL 2
#define RENAME(a) a ## _ ## 16
#include "v210_template.c"
static void v210_encode_line_16_c(const uint16_t *y, const uint16_t *u, const uint16_t *v, uint8_t *dst, ptrdiff_t width)
{
    uint32_t val;
    int i;

    for (i = 0; i < width - 5; i += 6) {
        WRITE_PIXELS(u, y, v, DEPTH);
        WRITE_PIXELS(y, u, y, DEPTH);
        WRITE_PIXELS(v, y, u, DEPTH);
        WRITE_PIXELS(y, v, y, DEPTH);
    }
}
#undef RENAME
#undef DEPTH
#undef BYTES_PER_PIXEL
#undef TYPE

extern void ff_sws_init_v210_x86(SwsContext *c);

av_cold void ff_sws_init_v210(SwsContext *c)
{
    c->v210_decode_line = c->dstBpc > 10 ? v210_decode_line_16_c : v210_decode_line_10_c;

    c->v210_encode_line_8 = v210_encode_line_8_c;
    c->v210_encode_line_10 = v210_encode_line_10_c;
    c->v210_encode_line_16 = v210_encode_line_16_c;
    c->v210_sample_factor_8 = 2;
    c->v210_sample_factor_10 = 1;
    c->v210_sample_factor_16 = 1;

    if (ARCH_X86)
        ff_sws_init_v210_x86(c);
}
