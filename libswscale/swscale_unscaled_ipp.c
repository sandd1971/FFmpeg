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
#include "libswscale/rgb2rgb.h"
#include "libavutil/imgutils.h"
#include "ipp.h"

extern void ff_sws_init_v210(SwsContext *c);

#define IPP_STRIDE_ALIGN    64
#define BPP_M_TO_N(m, n) ((1 << n) - 1.0) / ((1 << m) - 1.0)

static void fillPlane(uint8_t *plane, int stride, int width, int height, int y, uint8_t val)
{
    int i;
    uint8_t *ptr = plane + stride * y;
    for (i = 0; i < height; i++) {
        memset(ptr, val, width);
        ptr += stride;
    }
}

static void copyPlane(const uint8_t *src, int srcStride,
                      int srcSliceY, int srcSliceH, int width,
                      uint8_t *dst, int dstStride)
{
    dst += dstStride * srcSliceY;
    int i;
    for (i = 0; i < srcSliceH; i++) {
        memcpy(dst, src, width);
        src += srcStride;
        dst += dstStride;
    }
}

static int yuv42xpXToYuv420pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    int needInterleave = (c->interlSrc[0] && c->interlSrc[1]);
    int hFactor = (2 >> c->chrCvtSrcVSubSample);
    if (c->cvtSrcBpc == c->cvtDstBpc)
    {
        int planeWidth = c->cvtDstBpc > 8 ? c->dstW * 2 : c->dstW;
        copyPlane(src[0], srcStride[0], srcSliceY, srcSliceH, planeWidth, dstParam[0], dstStride[0]);
        if (needInterleave)
        {
            copyPlane(src[1], srcStride[1] * hFactor, srcSliceY / 2, srcSliceH / 2, planeWidth / 2, c->interlSrc[0], c->interlSrcStride[0]);
            copyPlane(src[2], srcStride[2] * hFactor, srcSliceY / 2, srcSliceH / 2, planeWidth / 2, c->interlSrc[1], c->interlSrcStride[1]);
        }
        else
        {
            copyPlane(src[1], srcStride[1] * hFactor, srcSliceY / 2, srcSliceH / 2, planeWidth / 2, dstParam[1], dstStride[1]);
            copyPlane(src[2], srcStride[2] * hFactor, srcSliceY / 2, srcSliceH / 2, planeWidth / 2, dstParam[2], dstStride[2]);
        }
    }
    else
    {
        Ipp8u* pDst[3] =
        {
            dstParam[0] + dstStride[0] * srcSliceY,
            needInterleave ?
                c->interlSrc[0] + c->interlSrcStride[0] * srcSliceY / 2 :
                dstParam[1] + dstStride[1] * srcSliceY / 2,
            needInterleave ?
                c->interlSrc[1] + c->interlSrcStride[1] * srcSliceY / 2 :
                dstParam[2] + dstStride[2] * srcSliceY / 2,
        };
        int nSrcStride[3] = { srcStride[0], srcStride[1] * hFactor, srcStride[2] * hFactor };
        int nDstStride[3] =
        {
            dstStride[0],
            needInterleave ? c->interlSrcStride[0] : dstStride[1],
            needInterleave ? c->interlSrcStride[1] : dstStride[2],
        };
        IppiSize roiSize[3] =
        {
            { c->srcW    , srcSliceH     },
            { c->srcW / 2, srcSliceH / 2 },
            { c->srcW / 2, srcSliceH / 2 },
        };
        Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

        if (c->cvtSrcBpc == 8)
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_8u16u_C1R(src[i], nSrcStride[i], mVal, 0, (Ipp16u*)pDst[i], nDstStride[i], roiSize[i], ippAlgHintFast);
        }
        else if (c->cvtDstBpc == 8)
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_16u8u_C1R((const Ipp16u*)src[i], nSrcStride[i], mVal, 0, pDst[i], nDstStride[i], roiSize[i], ippAlgHintFast);
        }
        else
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_16u_C1R((const Ipp16u*)src[i], nSrcStride[i], mVal, 0, (Ipp16u*)pDst[i], nDstStride[i], roiSize[i], ippAlgHintFast);
        }
    }

    if (needInterleave)
    {
        if (c->cvtDstBpc == 8)
            interleaveBytes(
                c->interlSrc[0] + c->interlSrcStride[0] * srcSliceY / 2,
                c->interlSrc[1] + c->interlSrcStride[1] * srcSliceY / 2,
                dstParam[1] + dstStride[1] * srcSliceY / 2,
                c->srcW / 2, srcSliceH / 2,
                c->interlSrcStride[0], c->interlSrcStride[1], dstStride[1]);
        else
            interleaveWords(
                c->interlSrc[0] + c->interlSrcStride[0] * srcSliceY / 2,
                c->interlSrc[1] + c->interlSrcStride[1] * srcSliceY / 2,
                dstParam[1] + dstStride[1] * srcSliceY / 2,
                c->srcW / 2, srcSliceH / 2,
                c->interlSrcStride[0], c->interlSrcStride[1], dstStride[1]);
    }

    return srcSliceH;
}

static int yuv420pXToYuv422pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    int planeWidth = c->cvtDstBpc > 8 ? c->dstW * 2 : c->dstW;
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    int nDstStride[3] =
    {
        dstStride[0], dstStride[1] * 2, dstStride[2] * 2,
    };

    if (c->cvtSrcBpc == c->cvtDstBpc)
    {
        copyPlane(src[0], srcStride[0], srcSliceY, srcSliceH, planeWidth, dstParam[0], nDstStride[0]);
        copyPlane(src[1], srcStride[1], srcSliceY / 2, srcSliceH / 2, planeWidth / 2, dstParam[1], nDstStride[1]);
        copyPlane(src[2], srcStride[2], srcSliceY / 2, srcSliceH / 2, planeWidth / 2, dstParam[2], nDstStride[2]);
    }
    else
    {
        IppiSize roiSize[3] =
        {
            { c->srcW    , srcSliceH     },
            { c->srcW / 2, srcSliceH / 2 },
            { c->srcW / 2, srcSliceH / 2 },
        };
        Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

        if (c->cvtSrcBpc == 8)
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_8u16u_C1R(src[i], srcStride[i], mVal, 0, (Ipp16u*)pDst[i], nDstStride[i], roiSize[i], ippAlgHintFast);
        }
        else if (c->cvtDstBpc == 8)
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_16u8u_C1R((const Ipp16u*)src[i], srcStride[i], mVal, 0, pDst[i], nDstStride[i], roiSize[i], ippAlgHintFast);
        }
        else
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_16u_C1R((const Ipp16u*)src[i], srcStride[i], mVal, 0, (Ipp16u*)pDst[i], nDstStride[i], roiSize[i], ippAlgHintFast);
        }
    }

    // Duplicate U/V Lines
    for (int i = 0; i < srcSliceH / 2; i++)
    {
        for (int p = 1; p < 3; p++)
        {
            memcpy(pDst[p] + dstStride[p], pDst[p], planeWidth / 2);
            pDst[p] += nDstStride[p];
        }
    }

    return srcSliceH;
}

static int yuv422pXToYuv422pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    if (c->cvtSrcBpc == c->cvtDstBpc)
    {
        int planeWidth = c->cvtDstBpc > 8 ? c->dstW * 2 : c->dstW;
        copyPlane(src[0], srcStride[0], srcSliceY, srcSliceH, planeWidth, dstParam[0], dstStride[0]);
        copyPlane(src[1], srcStride[1], srcSliceY, srcSliceH, planeWidth / 2, dstParam[1], dstStride[1]);
        copyPlane(src[2], srcStride[2], srcSliceY, srcSliceH, planeWidth / 2, dstParam[2], dstStride[2]);
    }
    else
    {
        Ipp8u* pDst[3] =
        {
            dstParam[0] + dstStride[0] * srcSliceY,
            dstParam[1] + dstStride[1] * srcSliceY,
            dstParam[2] + dstStride[2] * srcSliceY,
        };
        IppiSize roiSize[3] =
        {
            { c->srcW, srcSliceH },
            { c->srcW / 2, srcSliceH },
            { c->srcW / 2, srcSliceH },
        };
        Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

        if (c->cvtSrcBpc == 8)
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_8u16u_C1R(src[i], srcStride[i], mVal, 0, (Ipp16u*)pDst[i], dstStride[i], roiSize[i], ippAlgHintFast);
        }
        else if (c->cvtDstBpc == 8)
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_16u8u_C1R((const Ipp16u*)src[i], srcStride[i], mVal, 0, pDst[i], dstStride[i], roiSize[i], ippAlgHintFast);
        }
        else
        {
            for (int i = 0; i < 3; i++)
                ippiScaleC_16u_C1R((const Ipp16u*)src[i], srcStride[i], mVal, 0, (Ipp16u*)pDst[i], dstStride[i], roiSize[i], ippAlgHintFast);
        }
    }

    return srcSliceH;
}

static int yuv422pToYuy2_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const Ipp8u* pYUV[3] = { src[0], src[1], src[2] };
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
    const Ipp8u* pYUV[3] = { src[0], src[1], src[2] };
    Ipp32s pYUVStep[3] = { srcStride[0], srcStride[1], srcStride[2] };
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToCbYCr422_8u_P3C2R(pYUV, pYUVStep, dst, dstStride[0], roiSize);

    return srcSliceH;
}

static int yuv420pToNv12_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[2] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420_8u_P3P2R(src, srcStride, pDst[0], dstStride[0], pDst[1], dstStride[1], roiSize);

    return srcSliceH;
}

static int yuyvToYuv420p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDstYVU[3] =
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
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422_8u_C2P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int yuyvToYuv420pX_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuyvToYuv420p_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv42xpXToYuv420pX_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuyvToYuv422pX_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuyvToYuv422p_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* yuv422p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY,
    };
    return yuv422pXToYuv422pX_ipp(c, yuv422p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
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

static int yuyvToNv12_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *pY = dstParam[0] + dstStride[0] * srcSliceY;
    uint8_t *pCbCr = dstParam[1] + dstStride[1] * srcSliceY / 2;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToYCbCr420_8u_C2P2R(src[0], srcStride[0], pY, dstStride[0], pCbCr, dstStride[1], roiSize);

    return srcSliceH;
}

static int uyvyToYuv420p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDstYVU[3] =
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
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiCbYCr422ToYCbCr422_8u_C2P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int uyvyToYuv420pX_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uyvyToYuv420p_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv42xpXToYuv420pX_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int uyvyToYuv422pX_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uyvyToYuv422p_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* yuv422p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY,
    };
    return yuv422pXToYuv422pX_ipp(c, yuv422p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int uyvyToYuy2_ipp(SwsContext *c, const uint8_t *src[],
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

static int uyvyToNv12_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *pY = dstParam[0] + dstStride[0] * srcSliceY;
    uint8_t *pCbCr = dstParam[1] + dstStride[1] * srcSliceY / 2;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiCbYCr422ToYCbCr420_8u_C2P2R(src[0], srcStride[0], pY, dstStride[0], pCbCr, dstStride[1], roiSize);

    return srcSliceH;
}

static int yuv422pToNv12_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[2] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToYCbCr420_8u_P3P2R(src, srcStride, pDst[0], dstStride[0], pDst[1], dstStride[1], roiSize);

    return srcSliceH;
}

static int yuv422pToYuv420p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
        dstParam[2] + dstStride[2] * srcSliceY / 2,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr422ToYCbCr420_8u_P3R(src, srcStride, pDst, dstStride, roiSize);

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

static int yuv420pToYuv422p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
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
    const Ipp8u* pYVU[3] = { src[0], src[2], src[1] };
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
    const Ipp8u* pYVU[3] = { src[0], src[2], src[1] };
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

static int nv12ToYuv420p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
        dstParam[2] + dstStride[2] * srcSliceY / 2,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420_8u_P2P3R(src[0], srcStride[0], src[1], srcStride[1], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int nv12ToYuv422p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToYCbCr422_8u_P2P3R(src[0], srcStride[0], src[1], srcStride[1], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int nv12ToYuy2_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToYCbCr422_8u_P2C2R(src[0], srcStride[0], src[1], srcStride[1],
        dstParam[0] + dstStride[0] * srcSliceY, dstStride[0], roiSize);

    return srcSliceH;
}

static int nv12ToUyvy_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToCbYCr422_8u_P2C2R(src[0], srcStride[0], src[1], srcStride[1],
        dstParam[0] + dstStride[0] * srcSliceY, dstStride[0], roiSize);

    return srcSliceH;
}

static int nv12ToP0xx_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[2] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize[3] =
    {
        { c->srcW, srcSliceH     },
        { c->srcW, srcSliceH / 2 },
    };
    Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

    for (int i = 0; i < 2; i++)
        ippiScaleC_8u16u_C1R(src[i], srcStride[i], mVal, 0, (Ipp16u*)pDst[i], dstStride[i], roiSize[i], ippAlgHintFast);

    return srcSliceH;
}

static int nv12ToYuv420pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const uint8_t *yuv420p[3] =
    {
        src[0],
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2
    };
    int yuv420pStride[3] =
    {
        srcStride[0],
        c->ippConvertStride[1],
        c->ippConvertStride[2]
    };

    deinterleaveBytes(src[1], (uint8_t*)yuv420p[1], (uint8_t*)yuv420p[2],
        c->srcW / 2, srcSliceH / 2,
        srcStride[1], yuv420pStride[1], yuv420pStride[2]);

    return yuv42xpXToYuv420pX_ipp(c, yuv420p, yuv420pStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int nv12ToYuv422pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    const uint8_t *yuv420p[3] =
    {
        src[0],
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2
    };
    int yuv420pStride[3] =
    {
        srcStride[0],
        c->ippConvertStride[1],
        c->ippConvertStride[2]
    };

    deinterleaveBytes(src[1], (uint8_t*)yuv420p[1], (uint8_t*)yuv420p[2],
        c->srcW / 2, srcSliceH / 2,
        srcStride[1], yuv420pStride[1], yuv420pStride[2]);

    return yuv420pXToYuv422pX_ipp(c, yuv420p, yuv420pStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int nv12ToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiYCbCr420ToBGR_8u_P2C4R(src[0], srcStride[0], src[1], srcStride[1], dst, dstStride[0], roiSize, 0xff);

    return srcSliceH;
}

static int p0xxToNv12_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[2] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize[2] =
    {
        { c->dstW, srcSliceH },
        { c->dstW, srcSliceH / 2 },
    };
    Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

    // Y Plane
    ippiScaleC_16u8u_C1R((const Ipp16u*)src[0], srcStride[0], mVal, 0, pDst[0], dstStride[0], roiSize[0], ippAlgHintFast);
    // UV Plane
    ippiScaleC_16u8u_C1R((const Ipp16u*)src[1], srcStride[1], mVal, 0, pDst[1], dstStride[1], roiSize[1], ippAlgHintFast);

    return srcSliceH;
}

static int p0xxToP0xx_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[2] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize[2] =
    {
        { c->dstW, srcSliceH },
        { c->dstW, srcSliceH / 2 },
    };
    Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

    // Y Plane
    ippiScaleC_16u_C1R((const Ipp16u*)src[0], srcStride[0], mVal, 0, (Ipp16u*)pDst[0], dstStride[0], roiSize[0], ippAlgHintFast);
    // UV Plane
    ippiScaleC_16u_C1R((const Ipp16u*)src[1], srcStride[1], mVal, 0, (Ipp16u*)pDst[1], dstStride[1], roiSize[1], ippAlgHintFast);

    return srcSliceH;
}

static int p0xxToYuv420p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize[2] =
    {
        { c->dstW, srcSliceH },
        { c->dstW, srcSliceH / 2 },
    };
    Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

    // Y Plane
    ippiScaleC_16u8u_C1R((const Ipp16u*)src[0], srcStride[0], mVal, 0, pDst[0], dstStride[0], roiSize[0], ippAlgHintFast);
    // UV Plane
    ippiScaleC_16u8u_C1R((const Ipp16u*)src[1], srcStride[1], mVal, 0, pDst[1], c->ippConvertStride[1], roiSize[1], ippAlgHintFast);

    pDst[1] = dstParam[1] + dstStride[1] * srcSliceY / 2; // dst U
    pDst[2] = dstParam[2] + dstStride[2] * srcSliceY / 2; // dst V
    deinterleaveBytes(
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2, // src UV
        pDst[1], // dst U
        pDst[2], // dst V
        c->srcW / 2,
        srcSliceH / 2, // src H
        c->ippConvertStride[1], // src stride
        dstStride[1], // dst U stride
        dstStride[2]  // dst V stride
    );

    return srcSliceH;
}

static int p0xxToYuv422p_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
    };
    IppiSize roiSize[2] =
    {
        { c->dstW, srcSliceH },
        { c->dstW, srcSliceH / 2 },
    };
    Ipp64f mVal = BPP_M_TO_N(c->cvtSrcBpc, c->cvtDstBpc);

    // Y Plane
    ippiScaleC_16u8u_C1R((const Ipp16u*)src[0], srcStride[0], mVal, 0, pDst[0], dstStride[0], roiSize[0], ippAlgHintFast);
    // UV Plane
    ippiScaleC_16u8u_C1R((const Ipp16u*)src[1], srcStride[1], mVal, 0, pDst[1], c->ippConvertStride[1], roiSize[1], ippAlgHintFast);

    pDst[1] = dstParam[1] + dstStride[1] * srcSliceY; // dst U
    pDst[2] = dstParam[2] + dstStride[2] * srcSliceY; // dst V
    deinterleaveBytes(
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2, // src UV
        pDst[1], // dst U
        pDst[2], // dst V
        c->srcW / 2,
        srcSliceH / 2, // src H
        c->ippConvertStride[1], // src stride
        dstStride[1] * 2, // dst U stride
        dstStride[2] * 2  // dst V stride
    );

    // Duplicate U/V Lines
    for (int i = 0; i < srcSliceH / 2; i++)
    {
        for (int p = 1; p < 3; p++)
        {
            memcpy(pDst[p] + dstStride[p], pDst[p], c->dstW / 2);
            pDst[p] += dstStride[p] * 2;
        }
    }

    return srcSliceH;
}

static int p0xxToYuv420pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    int directConvert = c->cvtSrcBpc == c->cvtDstBpc;
    int planeWidth = c->cvtDstBpc > 8 ? c->dstW * 2 : c->dstW;
    const uint8_t *yuv420p[3] =
    {
        src[0],
        directConvert ? dstParam[1] + dstStride[1] * srcSliceY / 2 :
            c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        directConvert ? dstParam[2] + dstStride[2] * srcSliceY / 2 :
            c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2
    };
    int yuv420pStride[3] =
    {
        srcStride[0],
        directConvert ? dstStride[1] : c->ippConvertStride[1],
        directConvert ? dstStride[2] : c->ippConvertStride[2]
    };

    deinterleaveWords(src[1], (uint8_t*)yuv420p[1], (uint8_t*)yuv420p[2],
        c->srcW / 2, srcSliceH / 2,
        srcStride[1], yuv420pStride[1], yuv420pStride[2]);

    if (directConvert)
        copyPlane(src[0], srcStride[0], srcSliceY, srcSliceH, planeWidth, dstParam[0], dstStride[0]);
    else
        yuv42xpXToYuv420pX_ipp(c, yuv420p, yuv420pStride, srcSliceY, srcSliceH, dstParam, dstStride);

    return srcSliceH;
}

static int p0xxToYuv422pX_ipp(
    SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    int directConvert = c->cvtSrcBpc == c->cvtDstBpc;
    int planeWidth = c->cvtDstBpc > 8 ? c->dstW * 2 : c->dstW;
    const uint8_t *yuv420p[3] =
    {
        src[0],
        directConvert ? dstParam[1] + dstStride[1] * srcSliceY :
            c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        directConvert ? dstParam[2] + dstStride[2] * srcSliceY :
            c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2
    };
    int yuv420pStride[3] =
    {
        srcStride[0],
        directConvert ? dstStride[1] * 2 : c->ippConvertStride[1],
        directConvert ? dstStride[2] * 2 : c->ippConvertStride[2]
    };

    deinterleaveWords(src[1], (uint8_t*)yuv420p[1], (uint8_t*)yuv420p[2],
        c->srcW / 2, srcSliceH / 2,
        srcStride[1], yuv420pStride[1], yuv420pStride[2]);

    if (directConvert)
    {
        copyPlane(src[0], srcStride[0], srcSliceY, srcSliceH, planeWidth, dstParam[0], dstStride[0]);

        // Duplicate U/V Lines
        for (int i = 0; i < srcSliceH / 2; i++)
        {
            for (int p = 1; p < 3; p++)
            {
                memcpy((uint8_t*)yuv420p[p] + dstStride[p], (uint8_t*)yuv420p[p], planeWidth / 2);
                yuv420p[p] += dstStride[p] * 2;
            }
        }
    }
    else
        yuv420pXToYuv422pX_ipp(c, yuv420p, yuv420pStride, srcSliceY, srcSliceH, dstParam, dstStride);

    return srcSliceH;
}

static int p0xxToYuy2_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    p0xxToNv12_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* pSrc[2] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
    };
    return nv12ToYuy2_ipp(c, pSrc, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int p0xxToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    p0xxToNv12_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* pSrc[2] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
    };
    return nv12ToUyvy_ipp(c, pSrc, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int p0xxToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    p0xxToNv12_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* pSrc[2] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
    };
    return nv12ToBGRA_ipp(c, pSrc, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int bgraToYuv420p_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    Ipp8u* pDst[3] =
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
    Ipp8u* pDst[3] =
    {
        dstParam[0] + dstStride[0] * srcSliceY,
        dstParam[1] + dstStride[1] * srcSliceY,
        dstParam[2] + dstStride[2] * srcSliceY,
    };
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiBGRToYCbCr422_8u_AC4P3R(src[0], srcStride[0], pDst, dstStride, roiSize);

    return srcSliceH;
}

static int bgraToYuy2_ipp(SwsContext *c, const uint8_t *src[],
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

static int bgraToNv12_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    uint8_t *dst = dstParam[0] + dstStride[0] * srcSliceY;
    IppiSize roiSize = { c->srcW, srcSliceH };

    ippiBGRToYCbCr420_8u_AC4P2R(src[0], srcStride[0], dstParam[0] + dstStride[0] * srcSliceY, dstStride[0],
        dstParam[1] + dstStride[1] * srcSliceY / 2, dstStride[1], roiSize);

    return srcSliceH;
}

static int bgraToYuv420pX_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    bgraToYuv420p_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv42xpXToYuv420pX_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int bgraToYuv422pX_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    bgraToYuv422p_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t* yuv422p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY,
    };
    return yuv422pXToYuv422pX_ipp(c, yuv422p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuv420pXToYuy2_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuv42xpXToYuv420pX_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t *yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv420pToYuy2_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuv420pXToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuv42xpXToYuv420pX_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t *yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv420pToUyvy_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuv420pXToBGRA_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuv42xpXToYuv420pX_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t *yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv420pToBGRA_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuv420pXToNv12_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuv42xpXToYuv420pX_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t *yuv420p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY / 2,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY / 2,
    };
    return yuv420pToNv12_ipp(c, yuv420p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuv422pXToYuy2_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuv422pXToYuv422pX_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t *yuv422p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY,
    };
    return yuv422pToYuy2_ipp(c, yuv422p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

static int yuv422pXToUyvy_ipp(SwsContext *c, const uint8_t *src[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    yuv422pXToYuv422pX_ipp(c, src, srcStride, srcSliceY, srcSliceH, c->ippConvert, c->ippConvertStride);
    const uint8_t *yuv422p[3] =
    {
        c->ippConvert[0] + c->ippConvertStride[0] * srcSliceY,
        c->ippConvert[1] + c->ippConvertStride[1] * srcSliceY,
        c->ippConvert[2] + c->ippConvertStride[2] * srcSliceY,
    };
    return yuv422pToUyvy_ipp(c, yuv422p, c->ippConvertStride, srcSliceY, srcSliceH, dstParam, dstStride);
}

#define V210_CONVERT_FUNC

#define TYPE uint8_t
#define DEPTH 8
#define RENAME(a) a ## _ ## 8
#include "v210_template.c"
#undef RENAME
#undef DEPTH
#undef TYPE

#define TYPE uint16_t
#define DEPTH 10
#define RENAME(a) a ## _ ## 10
#include "v210_template.c"
#undef RENAME
#undef DEPTH
#undef TYPE

#define TYPE uint16_t
#define DEPTH 16
#define RENAME(a) a ## _ ## 16
#include "v210_template.c"
#undef RENAME
#undef DEPTH
#undef TYPE

#define READ_PIXELS(a, b, c, bitdepth)                 \
    do {                                               \
        val  = av_le2ne32(*src++);                     \
        *a++ = (val & 0x000003FF) << (bitdepth - 10);  \
        *b++ = (val & 0x000FFC00) >> (20 - bitdepth);  \
        *c++ = (val & 0x3FF00000) >> (30 - bitdepth);  \
    } while (0)

static int v210_decode_slice(SwsContext *c, const uint8_t *psrc[],
    int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t *dstParam[], int dstStride[])
{
    int aligned_input = !((uintptr_t)psrc[0] & 0x1f) && !(srcStride[0] & 0x1f);
    if (aligned_input != c->v210_aligned_input) {
        c->v210_aligned_input = aligned_input;
        ff_sws_init_v210(c);
    }

    for (int h = 0; h < srcSliceH; h++) {
        const uint32_t *src = (const uint32_t*)(psrc[0] + h * srcStride[0]);
        uint16_t *y = (uint16_t *)(dstParam[0] + dstStride[0] * (srcSliceY + h));
        uint16_t *u = (uint16_t *)(dstParam[1] + dstStride[1] * (srcSliceY + h));
        uint16_t *v = (uint16_t *)(dstParam[2] + dstStride[2] * (srcSliceY + h));
        uint32_t val;

        int w = (c->dstW / 12) * 12;
        c->v210_decode_line(src, y, u, v, w);

        y += w;
        u += w >> 1;
        v += w >> 1;
        src += (w << 1) / 3;

        if (w < c->srcW - 5) {
            READ_PIXELS(u, y, v, c->dstBpc);
            READ_PIXELS(y, u, y, c->dstBpc);
            READ_PIXELS(v, y, u, c->dstBpc);
            READ_PIXELS(y, v, y, c->dstBpc);
            w += 6;
        }

        if (w < c->srcW - 1) {
            READ_PIXELS(u, y, v, c->dstBpc);

            val = av_le2ne32(*src++);
            *y++ = (val & 0x000003FF) << (c->dstBpc - 10);
            if (w < c->srcW - 3) {
                *u++ = (val & 0x000FFC00) >> (20 - c->dstBpc);
                *y++ = (val & 0x3FF00000) >> (30 - c->dstBpc);

                val = av_le2ne32(*src++);
                *v++ = (val & 0x000003FF) << (c->dstBpc - 10);
                *y++ = (val & 0x000FFC00) >> (20 - c->dstBpc);
            }
        }
    }

    return srcSliceH;
}

void ff_get_unscaled_swscale_ipp(SwsContext *c)
{
    const enum AVPixelFormat srcFormat = c->srcFormat;
    const enum AVPixelFormat dstFormat = c->dstFormat;
    SwsFunc oldConvertFunc = c->convert_unscaled;
    if (srcFormat == dstFormat ||
        (srcFormat == AV_PIX_FMT_YUV420P && dstFormat == AV_PIX_FMT_YUVA420P) ||
        (srcFormat == AV_PIX_FMT_YUVA420P && dstFormat == AV_PIX_FMT_YUV420P))
        return;

    if (srcFormat == AV_PIX_FMT_V210 || dstFormat == AV_PIX_FMT_V210)
    {
        c->v210_aligned_input = 1;
        ff_sws_init_v210(c);

        if (srcFormat == AV_PIX_FMT_V210) {
            switch (dstFormat) {
            case AV_PIX_FMT_YUV422P10:
            case AV_PIX_FMT_YUV422P16:
                c->convert_unscaled = v210_decode_slice;
                break;
            }
        }
        else if (dstFormat == AV_PIX_FMT_V210) {
            switch (srcFormat) {
            case AV_PIX_FMT_YUV420P:
            case AV_PIX_FMT_YUV422P:
                c->convert_unscaled = v210_encode_slice_8;
                break;
            case AV_PIX_FMT_YUV420P10:
            case AV_PIX_FMT_YUV422P10:
                c->convert_unscaled = v210_encode_slice_10;
                break;
            case AV_PIX_FMT_YUV420P16:
            case AV_PIX_FMT_YUV422P16:
                c->convert_unscaled = v210_encode_slice_16;
                break;
            }
        }
    }
    else
    {
        if (c->use_ipp == 0)
            return;
    }

    c->cvtSrcFmt = c->srcFormat;
    c->cvtDstFmt = c->dstFormat;
    switch (srcFormat)
    {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVA420P:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUYV422:
            c->convert_unscaled = yuv420pToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            c->convert_unscaled = yuv420pToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            c->convert_unscaled = yuv420pToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            c->convert_unscaled = yuv420pToNv12_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
            c->convert_unscaled = yuv420pToYuv422p_ipp;
            break;
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            c->convert_unscaled = yuv42xpXToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            c->convert_unscaled = yuv420pXToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_YUV422P:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUYV422:
            c->convert_unscaled = yuv422pToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            c->convert_unscaled = yuv422pToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv422pToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            c->convert_unscaled = yuv422pToNv12_ipp;
            break;
        case AV_PIX_FMT_YUV420P:
            c->convert_unscaled = yuv422pToYuv420p_ipp;
            break;
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            c->convert_unscaled = yuv42xpXToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            c->convert_unscaled = yuv422pXToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_NV12:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUYV422:
            c->convert_unscaled = nv12ToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            c->convert_unscaled = nv12ToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            c->convert_unscaled = nv12ToBGRA_ipp;
            break;
        case AV_PIX_FMT_YUV420P:
            c->convert_unscaled = nv12ToYuv420p_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
            c->convert_unscaled = nv12ToYuv422p_ipp;
            break;
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = nv12ToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            c->convert_unscaled = nv12ToP0xx_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = nv12ToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_P010:
    case AV_PIX_FMT_P016:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUYV422:
            c->convert_unscaled = p0xxToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            c->convert_unscaled = p0xxToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            c->convert_unscaled = p0xxToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            c->convert_unscaled = p0xxToNv12_ipp;
            break;
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            c->convert_unscaled = p0xxToP0xx_ipp;
            break;
        case AV_PIX_FMT_YUV420P:
            c->convert_unscaled = p0xxToYuv420p_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
            c->convert_unscaled = p0xxToYuv422p_ipp;
            break;
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P16;
            c->convert_unscaled = p0xxToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P16;
            c->convert_unscaled = p0xxToYuv422pX_ipp;
            break;
        }
        if (c->convert_unscaled != oldConvertFunc)
        {
            if (c->dstBpc == 8)
            {
                if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_NV12, IPP_STRIDE_ALIGN) < 0)
                {
                    c->convert_unscaled = oldConvertFunc;
                    return;
                }
            }
            else
            {
                if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P16, IPP_STRIDE_ALIGN) < 0)
                {
                    c->convert_unscaled = oldConvertFunc;
                    return;
                }
            }
        }
        break;
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_BGR0:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUV420P:
            c->convert_unscaled = bgraToYuv420p_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
            c->convert_unscaled = bgraToYuv422p_ipp;
            break;
        case AV_PIX_FMT_YUYV422:
            c->convert_unscaled = bgraToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            c->convert_unscaled = bgraToUyvy_ipp;
            break;
        case AV_PIX_FMT_NV12:
            c->convert_unscaled = bgraToNv12_ipp;
            break;
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = bgraToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV422P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV422P;
            c->convert_unscaled = bgraToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_YUYV422:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVA420P:
            c->convert_unscaled = yuyvToYuv420p_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
            c->convert_unscaled = yuyvToYuv422p_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            c->convert_unscaled = yuyvToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            c->convert_unscaled = yuyvToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            c->convert_unscaled = yuyvToNv12_ipp;
            break;
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuyvToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV422P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV422P;
            c->convert_unscaled = yuyvToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_UYVY422:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVA420P:
            c->convert_unscaled = uyvyToYuv420p_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
            c->convert_unscaled = uyvyToYuv422p_ipp;
            break;
        case AV_PIX_FMT_YUYV422:
            c->convert_unscaled = uyvyToYuy2_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            c->convert_unscaled = uyvyToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            c->convert_unscaled = uyvyToNv12_ipp;
            break;
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = uyvyToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV422P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtSrcFmt = AV_PIX_FMT_YUV422P;
            c->convert_unscaled = uyvyToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_YUV420P10:
    case AV_PIX_FMT_YUV420P12:
    case AV_PIX_FMT_YUV420P14:
    case AV_PIX_FMT_YUV420P16:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUYV422:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv420pXToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv420pXToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv420pXToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv420pXToNv12_ipp;
            break;
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            c->convert_unscaled = yuv42xpXToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            c->convert_unscaled = yuv420pXToYuv422pX_ipp;
            break;
        }
        break;
    case AV_PIX_FMT_YUV422P10:
    case AV_PIX_FMT_YUV422P12:
    case AV_PIX_FMT_YUV422P14:
    case AV_PIX_FMT_YUV422P16:
        switch (dstFormat)
        {
        case AV_PIX_FMT_YUYV422:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV422P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV422P;
            c->convert_unscaled = yuv422pXToYuy2_ipp;
            break;
        case AV_PIX_FMT_UYVY422:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV422P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV422P;
            c->convert_unscaled = yuv422pXToUyvy_ipp;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv420pXToBGRA_ipp;
            break;
        case AV_PIX_FMT_NV12:
            if (av_image_alloc(c->ippConvert, c->ippConvertStride, c->dstW, c->dstH, AV_PIX_FMT_YUV420P, IPP_STRIDE_ALIGN) < 0)
                return;
            c->cvtDstFmt = AV_PIX_FMT_YUV420P;
            c->convert_unscaled = yuv420pXToNv12_ipp;
            break;
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            c->convert_unscaled = yuv42xpXToYuv420pX_ipp;
            break;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            c->convert_unscaled = yuv422pXToYuv422pX_ipp;
            break;
        }
        break;
    }

    if (c->convert_unscaled != oldConvertFunc)
    {
        const AVPixFmtDescriptor *descSrc = av_pix_fmt_desc_get(c->cvtSrcFmt);
        const AVPixFmtDescriptor *descDst = av_pix_fmt_desc_get(c->cvtDstFmt);
        c->cvtSrcBpc = descSrc->comp[0].depth;
        if (c->cvtSrcBpc < 8)   c->cvtSrcBpc = 8;
        c->cvtDstBpc = descDst->comp[0].depth;
        if (c->cvtDstBpc < 8)   c->cvtDstBpc = 8;
        if (c->cvtSrcFmt == AV_PIX_FMT_P010)   c->cvtSrcBpc = 16;
        if (c->cvtDstFmt == AV_PIX_FMT_P010)   c->cvtDstBpc = 16;
        av_pix_fmt_get_chroma_sub_sample(c->cvtSrcFmt, &c->chrCvtSrcHSubSample, &c->chrCvtSrcVSubSample);
        av_pix_fmt_get_chroma_sub_sample(c->cvtDstFmt, &c->chrCvtDstHSubSample, &c->chrCvtDstVSubSample);

        int cvt_slice_align = 1 << FFMAX(c->chrCvtSrcVSubSample, c->chrCvtDstVSubSample);
        if (c->dst_slice_align < cvt_slice_align)
            c->dst_slice_align = cvt_slice_align;

        if (dstFormat == AV_PIX_FMT_P010 || dstFormat == AV_PIX_FMT_P016)
        {
            if (srcFormat != AV_PIX_FMT_NV12 && srcFormat != AV_PIX_FMT_P010 && srcFormat != AV_PIX_FMT_P016)
            {
                c->interlSrcStride[0] = c->interlSrcStride[1] = (c->dstW + 63) & ~63;
                c->interlSrc[0] = av_malloc(c->dstH * c->interlSrcStride[0]);
                c->interlSrc[1] = c->interlSrc[0] + (c->dstH / 2) * c->interlSrcStride[0];
            }
        }
    }
}
