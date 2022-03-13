/*
 * Copyright (C) 2009 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2009 Baptiste Coudurier <baptiste dot coudurier at gmail dot com>
 *
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

#include "libavcodec/bytestream.h"
#include "libavcodec/internal.h"

#define CLIP(v, depth) av_clip(v, 1<<(depth-8), ((1<<depth)-(1<<(depth-8))-1))
#ifdef WRITE_PIXELS
#undef WRITE_PIXELS
#endif
#if (DEPTH <= 10)
#define WRITE_PIXELS(a, b, c, depth)                      \
    do {                                                  \
        val  =  CLIP(*a++, depth)  << (10-depth);         \
        val |=  (CLIP(*b++, depth) << (20-depth)) |       \
                (CLIP(*c++, depth) << (30-depth));        \
        AV_WL32(dst, val);                                \
        dst += 4;                                         \
    } while (0)
#else
#define WRITE_PIXELS(a, b, c, depth)                      \
    do {                                                  \
        val  =  CLIP(*a++, depth)  >> (depth-10);         \
        val |=  (CLIP(*b++, depth) << (20-depth)) |       \
                (CLIP(*c++, depth) << (30-depth));        \
        AV_WL32(dst, val);                                \
        dst += 4;                                         \
    } while (0)
#endif

#ifdef V210_CONVERT_FUNC
static int RENAME(v210_encode_slice)(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const int sample_size = 6 * c->RENAME(v210_sample_factor);
    const int sample_w    = c->dstW / sample_size;

    for (int h = 0; h < srcSliceH; h++) {
        const TYPE *y = (const TYPE *)(src[0] + srcStride[0] * h);
        const TYPE *u = (const TYPE *)(src[1] + srcStride[1] * (h >> c->chrSrcVSubSample));
        const TYPE *v = (const TYPE *)(src[2] + srcStride[2] * (h >> c->chrSrcVSubSample));
        uint8_t *dst = dstParam[0] + dstStride[0] * (srcSliceY + h);
        int w = sample_w * sample_size;
        uint32_t val;
        c->RENAME(v210_encode_line)(y, u, v, dst, w);

        y += w;
        u += w >> 1;
        v += w >> 1;
        dst += sample_w * 16 * c->RENAME(v210_sample_factor);

        for (; w < c->dstW - 5; w += 6) {
            WRITE_PIXELS(u, y, v, DEPTH);
            WRITE_PIXELS(y, u, y, DEPTH);
            WRITE_PIXELS(v, y, u, DEPTH);
            WRITE_PIXELS(y, v, y, DEPTH);
        }
        if (w < c->dstW - 1) {
            WRITE_PIXELS(u, y, v, DEPTH);

#if (DEPTH <= 10)
            val = CLIP(*y++, DEPTH) << (10-DEPTH);
#else
            val = CLIP(*y++, DEPTH) >> (DEPTH-10);
#endif
            if (w == c->dstW - 2) {
                AV_WL32(dst, val);
                dst += 4;
            }
        }
        if (w < c->dstW - 3) {
            val |= (CLIP(*u++, DEPTH) << (20-DEPTH)) | (CLIP(*y++, DEPTH) << (30-DEPTH));
            AV_WL32(dst, val);
            dst += 4;

#if (DEPTH <= 10)
            val = CLIP(*v++, DEPTH) << (10-DEPTH) | (CLIP(*y++, DEPTH) << (20-DEPTH));
#else
            val = CLIP(*v++, DEPTH) >> (DEPTH-10) | (CLIP(*y++, DEPTH) << (20-DEPTH));
#endif
            AV_WL32(dst, val);
            dst += 4;
        }
    }

    return srcSliceH;
}
#endif
