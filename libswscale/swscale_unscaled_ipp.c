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

#include "config.h"
#include "libswscale/swscale.h"
#include "libswscale/swscale_internal.h"
#include "ipp.h"

static void fillPlane(uint8_t *plane, int stride, int width, int height, int y, uint8_t val)
{
    int i;
    uint8_t *ptr = plane + stride * y;
    for (i = 0; i < height; i++) {
        memset(ptr, val, width);
        ptr += stride;
    }
}

static int yuv422pToYuy2_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const Ipp8u* (pYUV[3]) = { src[0], src[1], src[2] };
    Ipp32s pYUVStep[3] = { srcStride[0], srcStride[1], srcStride[2] };
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422_8u_P3C2R(pYUV, pYUVStep, dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int yuv422pToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const Ipp8u* (pYUV[3]) = { src[0], src[1], src[2] };
    Ipp32s pYUVStep[3] = { srcStride[0], srcStride[1], srcStride[2] };
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToCbYCr422_8u_P3C2R(pYUV, pYUVStep, dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int yuyvToYuv420p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDstYVU[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY / 2,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    Ipp32s pDstStepYVU[3] = { dstStride[0], dstStride[2], dstStride[1] };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToYCrCb420_8u_C2P3R(src[0], srcStride[0], pDstYVU, pDstStepYVU, roiSize);

    if (dstParam[3])
        fillPlane(dstParam[3], dstStride[3], c->srcW, srcSliceH, srcSliceY, 255);

    return srcSliceH;
}

static int yuyvToYuv422p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDst[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422_8u_C2P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int yuyvToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToCbYCr422_8u_C2R(src[0], srcStride[0], dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int yuyvToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToBGR_8u_C2C4R(src[0], srcStride[0], dst, dstStride[0], roiSize, 0xff);

    return srcSliceH;
}

static int uyvyToYuv420p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDstYVU[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY / 2,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    Ipp32s pDstStepYVU[3] = { dstStride[0], dstStride[2], dstStride[1] };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiCbYCr422ToYCrCb420_8u_C2P3R(src[0], srcStride[0], pDstYVU, pDstStepYVU, roiSize);

    if (dstParam[3])
        fillPlane(dstParam[3], dstStride[3], c->srcW, srcSliceH, srcSliceY, 255);

    return srcSliceH;
}

static int uyvyToYuv422p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDst[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiCbYCr422ToYCbCr422_8u_C2P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int uyvyToYuyv_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiCbYCr422ToYCbCr422_8u_C2R(src[0], srcStride[0], dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int uyvyToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiCbYCr422ToBGR_8u_C2C4R(src[0], srcStride[0], dst, dstStride[0], roiSize, 0xff);

    return srcSliceH;
}

static int yuv422pToYuv420p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDst[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
        dstParam[2] + dstStride[2] * srcSliceY / 2,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToYCbCr420_8u_P3R(src, srcStride, pDst, dstStride, roiSize);

    return srcSliceH;
}

static int yuv420pToYuv422p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDst[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToYCbCr422_8u_P3R(src, srcStride, pDst, dstStride, roiSize);

    return srcSliceH;
}

static int yuv420pToYuy2_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const Ipp8u* (pYVU[3]) = { src[0], src[2], src[1] };
    Ipp32s pYVUStep[3] = { srcStride[0], srcStride[2], srcStride[1] };
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCrCb420ToYCbCr422_8u_P3C2R(pYVU, pYVUStep, dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int yuv420pToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const Ipp8u* (pYVU[3]) = { src[0], src[2], src[1] };
    Ipp32s pYVUStep[3] = { srcStride[0], srcStride[2], srcStride[1] };
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCrCb420ToCbYCr422_8u_P3C2R(pYVU, pYVUStep, dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int yuv420pToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToBGR_8u_P3C4R(src, srcStride, dst, dstStride[0], roiSize, 0xff);

    return srcSliceH;
}

static int yuv422pToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    Ipp32s pSrcStep[3] = { srcStride[0], srcStride[1] * 2, srcStride[2] * 2 };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToBGR_8u_P3C4R(src, pSrcStep, dst, dstStride[0], roiSize, 0xff);

    return srcSliceH;
}

static int bgraToYuv420p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDst[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
        dstParam[2] + dstStride[2] * srcSliceY / 2,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiBGRToYCbCr420_8u_AC4P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int bgraToYuv422p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* (pDst[3]) =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiBGRToYCbCr422_8u_AC4P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int bgraToYuyv_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiBGRToYCbCr422_8u_AC4C2R(src[0], srcStride[0], dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int bgraToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiBGRToCbYCr422_8u_AC4C2R(src[0], srcStride[0], dst, dstStride[0], roiSize);

    return srcSliceH;
}

void ff_get_unscaled_swscale_ipp(SwsContext *c)
{
    const enum AVPixelFormat srcFormat = c->srcFormat;
    const enum AVPixelFormat dstFormat = c->dstFormat;

    if (srcFormat == AV_PIX_FMT_YUV422P) {
        if (dstFormat == AV_PIX_FMT_YUYV422)
            c->convert_unscaled = yuv422pToYuy2_ipp;
        else if (dstFormat == AV_PIX_FMT_UYVY422)
            c->convert_unscaled = yuv422pToUyvy_ipp;
        else if (dstFormat == AV_PIX_FMT_YUV420P)
            c->convert_unscaled = yuv422pToYuv420p_ipp;
        else if (dstFormat == AV_PIX_FMT_BGRA)
            c->convert_unscaled = yuv422pToBGRA_ipp;
    }

    if (srcFormat == AV_PIX_FMT_YUV420P || srcFormat == AV_PIX_FMT_YUVA420P) {
        if (dstFormat == AV_PIX_FMT_YUYV422)
            c->convert_unscaled = yuv420pToYuy2_ipp;
        else if (dstFormat == AV_PIX_FMT_UYVY422)
            c->convert_unscaled = yuv420pToUyvy_ipp;
        else if (dstFormat == AV_PIX_FMT_YUV422P)
            c->convert_unscaled = yuv420pToYuv422p_ipp;
        else if (dstFormat == AV_PIX_FMT_BGRA)
            c->convert_unscaled = yuv420pToBGRA_ipp;
    }

    if (srcFormat == AV_PIX_FMT_BGRA) {
        if (dstFormat == AV_PIX_FMT_YUV420P)
            c->convert_unscaled = bgraToYuv420p_ipp;
        else if (dstFormat == AV_PIX_FMT_YUV422P)
            c->convert_unscaled = bgraToYuv422p_ipp;
        else if (dstFormat == AV_PIX_FMT_YUYV422)
            c->convert_unscaled = bgraToYuyv_ipp;
        else if (dstFormat == AV_PIX_FMT_UYVY422)
            c->convert_unscaled = bgraToUyvy_ipp;
    }

    if (srcFormat == AV_PIX_FMT_YUYV422)
    {
        if (dstFormat == AV_PIX_FMT_YUV420P || dstFormat == AV_PIX_FMT_YUVA420P)
            c->convert_unscaled = yuyvToYuv420p_ipp;
        else if (dstFormat == AV_PIX_FMT_YUV422P)
            c->convert_unscaled = yuyvToYuv422p_ipp;
        else if (dstFormat == AV_PIX_FMT_UYVY422)
            c->convert_unscaled = yuyvToUyvy_ipp;
        else if (dstFormat == AV_PIX_FMT_BGRA)
            c->convert_unscaled = yuyvToBGRA_ipp;
    }

    if (srcFormat == AV_PIX_FMT_UYVY422)
    {
        if (dstFormat == AV_PIX_FMT_YUV420P || dstFormat == AV_PIX_FMT_YUVA420P)
            c->convert_unscaled = uyvyToYuv420p_ipp;
        else if (dstFormat == AV_PIX_FMT_YUV422P)
            c->convert_unscaled = uyvyToYuv422p_ipp;
        else if (dstFormat == AV_PIX_FMT_YUYV422)
            c->convert_unscaled = uyvyToYuyv_ipp;
        else if (dstFormat == AV_PIX_FMT_BGRA)
            c->convert_unscaled = uyvyToBGRA_ipp;
    }
}
