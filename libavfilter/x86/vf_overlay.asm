;*****************************************************************************
;* x86-optimized functions for overlay filter
;*
;* Copyright (C) 2018 Paul B Mahol
;* Copyright (C) 2018 Henrik Gramner
;*
;* This file is part of FFmpeg.
;*
;* FFmpeg is free software; you can redistribute it and/or
;* modify it under the terms of the GNU Lesser General Public
;* License as published by the Free Software Foundation; either
;* version 2.1 of the License, or (at your option) any later version.
;*
;* FFmpeg is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;* Lesser General Public License for more details.
;*
;* You should have received a copy of the GNU Lesser General Public
;* License along with FFmpeg; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
;*****************************************************************************

%include "libavutil/x86/x86util.asm"

SECTION_RODATA

pb_1:     times 32 db 1
pw_128:   times 16 dw 128
pw_255:   times 16 dw 255
pw_257:   times 16 dw 257
pw_1:     times 16 dw 1
pd_512:   times  8 dd 512
pd_1023:  times  8 dd 1023
pd_1025:  times  8 dd 1025

SECTION .text

%macro overlay_row_44 0

cglobal overlay_row_44, 5, 7, 6, 0, d, da, s, a, w, r, x
    xor          xq, xq
    movsxdifnidn wq, wd
    mov          rq, wq
    and          rq, mmsize/2 - 1
    cmp          wq, mmsize/2
    jl .end
    sub          wq, rq
    mova         m3, [pw_255]
    mova         m4, [pw_128]
    mova         m5, [pw_257]
    .loop:
        pmovzxbw    m0, [sq+xq]
        pmovzxbw    m2, [aq+xq]
        pmovzxbw    m1, [dq+xq]
        pmullw      m0, m2
        pxor        m2, m3
        pmullw      m1, m2
        paddw       m0, m4
        paddw       m0, m1
        pmulhuw     m0, m5
        packuswb    m0, m0
%if cpuflag(avx2)
        vpermpd m0, m0, 216
        movdqu [dq+xq], xmm0
%else
        movq   [dq+xq], m0
%endif
        add         xq, mmsize/2
        cmp         xq, wq
        jl .loop

    .end:
    mov    eax, xd
    RET

%endmacro

%macro overlay_row_22 0

cglobal overlay_row_22, 5, 7, 6, 0, d, da, s, a, w, r, x
    xor          xq, xq
    movsxdifnidn wq, wd
    sub          wq, 1
    mov          rq, wq
    and          rq, mmsize/2 - 1
    cmp          wq, mmsize/2
    jl .end
    sub          wq, rq
    mova         m3, [pw_255]
    mova         m4, [pw_128]
    mova         m5, [pw_257]
    .loop:
        pmovzxbw    m0, [sq+xq]
        movu        m1, [aq+2*xq]
        pandn       m2, m3, m1
        psllw       m1, 8
        pavgw       m2, m1
        psrlw       m2, 8
        pmovzxbw    m1, [dq+xq]
        pmullw      m0, m2
        pxor        m2, m3
        pmullw      m1, m2
        paddw       m0, m4
        paddw       m0, m1
        pmulhuw     m0, m5
        packuswb    m0, m0
%if cpuflag(avx2)
        vpermpd m0, m0, 216
        movdqu [dq+xq], xmm0
%else
        movq   [dq+xq], m0
%endif
        add         xq, mmsize/2
        cmp         xq, wq
        jl .loop

    .end:
    mov    eax, xd
    RET

%endmacro

%macro overlay_row_20 0

cglobal overlay_row_20, 6, 7, 7, 0, d, da, s, a, w, r, x
    mov         daq, aq
    add         daq, rmp
    xor          xq, xq
    movsxdifnidn wq, wd
    sub          wq, 1
    mov          rq, wq
    and          rq, mmsize/2 - 1
    cmp          wq, mmsize/2
    jl .end
    sub          wq, rq
    mova         m3, [pw_255]
    mova         m4, [pw_128]
    mova         m5, [pw_257]
    mova         m6, [pb_1]
    .loop:
        pmovzxbw    m0, [sq+xq]
        movu        m2, [aq+2*xq]
        movu        m1, [daq+2*xq]
        pmaddubsw   m2, m6
        pmaddubsw   m1, m6
        paddw       m2, m1
        psrlw       m2, 2
        pmovzxbw    m1, [dq+xq]
        pmullw      m0, m2
        pxor        m2, m3
        pmullw      m1, m2
        paddw       m0, m4
        paddw       m0, m1
        pmulhuw     m0, m5
        packuswb    m0, m0
%if cpuflag(avx2)
        vpermpd m0, m0, 216
        movdqu [dq+xq], xmm0
%else
        movq   [dq+xq], m0
%endif
        add         xq, mmsize/2
        cmp         xq, wq
        jl .loop

    .end:
    mov    eax, xd
    RET

%endmacro

%macro overlay_row_44_10b 0

cglobal overlay_row_44_10b, 5, 7, 6, 0, d, da, s, a, w, r, x
    xor          xq, xq
    movsxdifnidn wq, wd
    mov          rq, wq
    and          rq, mmsize/2 - 1
    cmp          wq, mmsize/2
    jl .end
    sub          wq, rq
    mova         m3, [pd_1023]
    mova         m4, [pd_512]
    mova         m5, [pd_1025]
    .loop:
        pmovzxwd    m0, [sq+xq]
        pmovzxwd    m2, [aq+xq]
        pmovzxwd    m1, [dq+xq]
        pmulld      m0, m2
        pxor        m2, m3
        pmulld      m1, m2
        paddd       m0, m4
        paddd       m0, m1
        pmulld      m0, m5
        psrld       m0, 20
        packusdw    m0, m0
%if cpuflag(avx2)
        vpermpd m0, m0, 216
        movdqu [dq+xq], xmm0
%else
        movq   [dq+xq], m0
%endif
        add         xq, mmsize/2
        cmp         xq, wq
        jl .loop

    .end:
    mov    eax, xd
    RET

%endmacro

%macro overlay_row_22_10b 0

cglobal overlay_row_22_10b, 5, 7, 6, 0, d, da, s, a, w, r, x
    xor          xq, xq
    movsxdifnidn wq, wd
    sub          wq, 1
    mov          rq, wq
    and          rq, mmsize/2 - 1
    cmp          wq, mmsize/2
    jl .end
    sub          wq, rq
    mova         m3, [pd_1023]
    mova         m4, [pd_512]
    mova         m5, [pd_1025]
    .loop:
        pmovzxwd    m0, [sq+xq]
        movu        m1, [aq+2*xq]
        pandn       m2, m3, m1
        psrld       m2, 16
        pand        m1, m3, m1
        pavgw       m2, m1
        pmovzxwd    m1, [dq+xq]
        pmulld      m0, m2
        pxor        m2, m3
        pmulld      m1, m2
        paddd       m0, m4
        paddd       m0, m1
        pmulld      m0, m5
        psrld       m0, 20
        packusdw    m0, m0
%if cpuflag(avx2)
        vpermpd m0, m0, 216
        movdqu [dq+xq], xmm0
%else
        movq   [dq+xq], m0
%endif
        add         xq, mmsize/2
        cmp         xq, wq
        jl .loop

    .end:
    mov    eax, xd
    RET

%endmacro

%macro overlay_row_20_10b 0

cglobal overlay_row_20_10b, 6, 7, 7, 0, d, da, s, a, w, r, x
    mov         daq, aq
    add         daq, rmp
    xor          xq, xq
    movsxdifnidn wq, wd
    sub          wq, 1
    mov          rq, wq
    and          rq, mmsize/2 - 1
    cmp          wq, mmsize/2
    jl .end
    sub          wq, rq
    mova         m3, [pd_1023]
    mova         m4, [pd_512]
    mova         m5, [pd_1025]
    mova         m6, [pw_1]
    .loop:
        pmovzxwd    m0, [sq+xq]
        movu        m2, [aq+2*xq]
        movu        m1, [daq+2*xq]
        pmaddwd     m2, m6
        pmaddwd     m1, m6
        paddd       m2, m1
        psrld       m2, 2
        pmovzxwd    m1, [dq+xq]
        pmulld      m0, m2
        pxor        m2, m3
        pmulld      m1, m2
        paddd       m0, m4
        paddd       m0, m1
        pmulld      m0, m5
        psrld       m0, 20
        packusdw    m0, m0
%if cpuflag(avx2)
        vpermpd m0, m0, 216
        movdqu [dq+xq], xmm0
%else
        movq   [dq+xq], m0
%endif
        add         xq, mmsize/2
        cmp         xq, wq
        jl .loop

    .end:
    mov    eax, xd
    RET

%endmacro

INIT_XMM sse4
overlay_row_44
overlay_row_22
overlay_row_20
overlay_row_44_10b
overlay_row_22_10b
overlay_row_20_10b

%if HAVE_AVX2_EXTERNAL
INIT_YMM avx2
overlay_row_44
overlay_row_22
overlay_row_20
overlay_row_44_10b
overlay_row_22_10b
overlay_row_20_10b
%endif
