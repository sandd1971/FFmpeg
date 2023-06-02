/*
 *  Copyright (c) 2013, Stefan Johnson
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this list
 *     of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this
 *     list of conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "instruction_size.h"

#define TRUE true
#define FALSE false

typedef struct {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t scale : 2;
} sib_t;

typedef struct {
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mod : 2;
} mod_rm_t;


typedef enum {
    kOperandEncodingInvalid,
    kOperandEncodingPrefix,
    kOperandEncodingTwoByteInstruction = kOperandEncodingPrefix,
    kOperandEncodingNP,         //no operand
    kOperandEncodingO = kOperandEncodingNP,   //opcode + rd
    kOperandEncodingM,          //modrm:r/m
    kOperandEncodingM1 = kOperandEncodingM,   //modrm:r/m, 1
    kOperandEncodingMC = kOperandEncodingM,   //modrm:r/m, cl
    kOperandEncodingRM = kOperandEncodingM,   //modrm:reg, modrm:r/m
    kOperandEncodingMR = kOperandEncodingM,   //modrm:r/m, modrm:reg
    kOperandEncodingMI8,        //modrm:r/m, imm8
    kOperandEncodingMI16_32,    //modrm:r/m, imm16/32
    kOperandEncodingRMI8 = kOperandEncodingMI8,       //modrm:reg, modrm:r/m, imm8
    kOperandEncodingRMI16_32 = kOperandEncodingMI16_32,   //modrm:reg, modrm:r/m, imm16/32
    kOperandEncodingMRI8 = kOperandEncodingMI8,
    kOperandEncodingMRI16_32 = kOperandEncodingMI16_32,
    kOperandEncodingI8,         //(AL/AX/EAX/RAX), imm8 ; r8, imm8
    kOperandEncodingI16_32,     //(AL/AX/EAX/RAX), imm16/32 ; r16/32, imm16/32
    kOperandEncodingOI8 = kOperandEncodingI8,   //opcode + rd, imm8
    kOperandEncodingOI16_32 = kOperandEncodingI16_32, //opcode + rd, imm8
    kOperandEncodingTD8 = kOperandEncodingI8,         //moffs, AL/AX/EAX/RAX
    kOperandEncodingTD16_32 = kOperandEncodingI16_32, //moffs, AL/AX/EAX/RAX
    kOperandEncodingFD8 = kOperandEncodingI8,         //AL/AX/EAX/RAX, moffs
    kOperandEncodingFD16_32 = kOperandEncodingI16_32, //AL/AX/EAX/RAX, moffs
    kOperandEncodingD8 = kOperandEncodingI8,         //offset
    kOperandEncodingD16_32 = kOperandEncodingI16_32,     //offset
    kOperandEncodingI16,        //imm16
    kOperandEncodingII16_8,     //imm16, imm8


    //specialty flags
    kOperandEncodingSupportsImm64 = 0x10000000,
    kOperandEncodingMultipleEncodings = 0x20000000,
    kOperandEncodingMultipleOpcodes = kOperandEncodingMultipleEncodings | 0x40000000,
    kOperandEncodingFlags = 0xF0000000
} OPERAND_ENCODING;

static const OPERAND_ENCODING Opcode1OperandEncoding[0x1ff] = {
    kOperandEncodingMR, //add r/m8, r8
    kOperandEncodingMR, //add r/m16/32/64, r16/32/64
    kOperandEncodingRM, //add r8, r/m8
    kOperandEncodingRM, //add r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //add AL, imm8
    kOperandEncodingI16_32,  //add rAX, imm16/32
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingMR, //or r/m8, r8
    kOperandEncodingMR, //or r/m16/32/64, r16/32/64
    kOperandEncodingRM, //or r8, r/m8
    kOperandEncodingRM, //or r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //or AL, imm8
    kOperandEncodingI16_32,  //or rAX, imm16/32
    kOperandEncodingInvalid,
    kOperandEncodingTwoByteInstruction,
    kOperandEncodingMR, //adc r/m8, r8
    kOperandEncodingMR, //adc r/m16/32/64, r16/32/64
    kOperandEncodingRM, //adc r8, r/m8
    kOperandEncodingRM, //adc r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //adc AL, imm8
    kOperandEncodingI16_32,  //adc rAX, imm16/32
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingMR, //sbb r/m8, r8
    kOperandEncodingMR, //sbb r/m16/32/64, r16/32/64
    kOperandEncodingRM, //sbb r8, r/m8
    kOperandEncodingRM, //sbb r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //sbb AL, imm8
    kOperandEncodingI16_32,  //sbb rAX, imm16/32
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingMR, //and r/m8, r8
    kOperandEncodingMR, //and r/m16/32/64, r16/32/64
    kOperandEncodingRM, //and r8, r/m8
    kOperandEncodingRM, //and r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //and AL, imm8
    kOperandEncodingI16_32,  //and rAX, imm16/32
    kOperandEncodingPrefix, //Null in 64 bit
    kOperandEncodingInvalid,
    kOperandEncodingMR, //sub r/m8, r8
    kOperandEncodingMR, //sub r/m16/32/64, r16/32/64
    kOperandEncodingRM, //sub r8, r/m8
    kOperandEncodingRM, //sub r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //sub AL, imm8
    kOperandEncodingI16_32,  //sub rAX, imm16/32
    kOperandEncodingPrefix, //Null in 64 bit
    kOperandEncodingInvalid,
    kOperandEncodingMR, //xor r/m8, r8
    kOperandEncodingMR, //xor r/m16/32/64, r16/32/64
    kOperandEncodingRM, //xor r8, r/m8
    kOperandEncodingRM, //xor r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //xor AL, imm8
    kOperandEncodingI16_32,  //xor rAX, imm16/32
    kOperandEncodingPrefix, //Null in 64 bit
    kOperandEncodingInvalid,
    kOperandEncodingMR, //cmp r/m8, r8
    kOperandEncodingMR, //cmp r/m16/32/64, r16/32/64
    kOperandEncodingRM, //cmp r8, r/m8
    kOperandEncodingRM, //cmp r16/32/64, r/m16/32/64
    kOperandEncodingI8,  //cmp AL, imm8
    kOperandEncodingI16_32,  //cmp rAX, imm16/32
    kOperandEncodingPrefix, //Null in 64 bit
    kOperandEncodingInvalid,
    kOperandEncodingPrefix, //REX
    kOperandEncodingPrefix, //REX.B
    kOperandEncodingPrefix, //REX.X
    kOperandEncodingPrefix, //REX.XB
    kOperandEncodingPrefix, //REX.R
    kOperandEncodingPrefix, //REX.RB
    kOperandEncodingPrefix, //REX.RX
    kOperandEncodingPrefix, //REX.RXB
    kOperandEncodingPrefix, //REX.W
    kOperandEncodingPrefix, //REX.WB
    kOperandEncodingPrefix, //REX.WX
    kOperandEncodingPrefix, //REX.WXB
    kOperandEncodingPrefix, //REX.WR
    kOperandEncodingPrefix, //REX.WRB
    kOperandEncodingPrefix, //REX.WRX
    kOperandEncodingPrefix, //REX.WRXB
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //push r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingO, //pop r64/16
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingRM, //movsxd r32/64, r/m32
    kOperandEncodingPrefix, //fs
    kOperandEncodingPrefix, //gs
    kOperandEncodingPrefix, //operand size override ; precision size override (SSE)
    kOperandEncodingPrefix, //address size override
    kOperandEncodingI16_32, //push imm16/32
    kOperandEncodingRMI16_32, //imul r/16/32/64, r/m16/32/64, imm16/32
    kOperandEncodingI8, //push imm8
    kOperandEncodingRMI8, //imul r16/32/64, r/m16/32/64, imm8
    kOperandEncodingNP, //ins m8, dx ; insb
    kOperandEncodingNP, //ins m16/32, dx ; insw ; insd
    kOperandEncodingNP, //outs m8, dx ; outsb
    kOperandEncodingNP, //outs m16/32, dx ; outsw ; outsd
    kOperandEncodingD8, //jo rel8
    kOperandEncodingD8, //jno rel8
    kOperandEncodingD8, //jb/jnae/jc rel8
    kOperandEncodingD8, //jnb/jae/jnz rel8
    kOperandEncodingD8, //jz/je rel8
    kOperandEncodingD8, //jnz/jne rel8
    kOperandEncodingD8, //jbe/jna rel8
    kOperandEncodingD8, //jnbe/ja rel8
    kOperandEncodingD8, //js rel8
    kOperandEncodingD8, //jns rel8
    kOperandEncodingD8, //jp/jpe rel8
    kOperandEncodingD8, //jnp/jpo rel8
    kOperandEncodingD8, //jl/jnge rel8
    kOperandEncodingD8, //jnl/jng rel8
    kOperandEncodingD8, //jle/jng rel8
    kOperandEncodingD8, //jnle/jg rel8
    kOperandEncodingRMI8, //add ; or ; adc ; sbb ; and ; sub ; xor ; cmp r/m8, imm8
    kOperandEncodingRMI16_32, //add ; or ; adc ; sbb ; and ; sub ; xor ; cmp r/m16/32/64, imm16/32
    kOperandEncodingInvalid,
    kOperandEncodingRMI8, //add ; or ; adc ; sbb ; and ; sub ; xor ; cmp r/m16/32/64, imm8
    kOperandEncodingMR, //test r/m8, r8
    kOperandEncodingMR, //test r/m16/32/64, r16/32/64
    kOperandEncodingRM, //xchg r8, r/m8
    kOperandEncodingRM, //xchg r16/32/64, r/m16/32/64
    kOperandEncodingMR, //mov r/m8, r8
    kOperandEncodingMR, //mov r/m16/32/64, r16/32/64
    kOperandEncodingRM, //mov r8, r/m8
    kOperandEncodingRM, //mov r16/32/64, r/m16/32/64
    kOperandEncodingMR, //mov m16, Sreg ; mov r16/32/64, Sreg
    kOperandEncodingRM, //lea r16/32/64, m
    kOperandEncodingRM, //mov Sreg, r/m16
    kOperandEncodingM, //pop r/m16/32/64
    kOperandEncodingNP, //nop ; xchg r16/32/64, rAX ; pause
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingO, //xchg r16/32/64, rAX
    kOperandEncodingNP, //cbw ; cwde ; cdqe
    kOperandEncodingNP, //cwd ; cdq ; cqo
    kOperandEncodingInvalid,
    kOperandEncodingNP, //fwait ; wait
    kOperandEncodingNP, //pushf ; pushfq
    kOperandEncodingNP, //popf ; popfq
    kOperandEncodingNP, //sahf AH
    kOperandEncodingNP, //lahf AH
    kOperandEncodingFD8, //mov AL, moffs8
    kOperandEncodingFD16_32, //mov rAX, moffs16/32/64
    kOperandEncodingTD8, //mov moffs8, AL
    kOperandEncodingTD16_32, //mov moffs16/32/64, rAX
    kOperandEncodingNP, //movs m8, m8 ; movsb
    kOperandEncodingNP, //movs m16/32/64, m16/32/64 ; movsw ; movsd ; movsq
    kOperandEncodingNP, //cmps m8, m8 ; cmpsb
    kOperandEncodingNP, //cmps m16/32/64, m16/32/64 ; cmpsw ; cmpsd ; cmpsq
    kOperandEncodingI8, //test AL, imm8
    kOperandEncodingI16_32, //test rAX, imm16/32
    kOperandEncodingNP, //stos m8, AL ; stosb
    kOperandEncodingNP, //stos m16/32/64, rAX ; stosw ; stosd ; stosq
    kOperandEncodingNP, //lods AL, m8 ; lodsb
    kOperandEncodingNP, //lods rAX, m16/32/64 ; lodsw ; lodsd ; lodsq
    kOperandEncodingNP, //scas m8, AL ; scasb
    kOperandEncodingNP, //scas m16/32/64, rAX ; scasw ; scasd ; scasq
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingI8, //mov r8, imm8
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingSupportsImm64 | kOperandEncodingI16_32, //mov r16/32/64, imm16/32/64
    kOperandEncodingRMI8, //rol ; ror ; rcl ; rcr ; shl ; sal ; shr ; sal ; shl ; sar r/m8, imm8
    kOperandEncodingRMI8, //rol ; ror ; rcl ; rcr ; shl ; sal ; shr ; sal ; shl ; sar r/m16/32/64, imm16/32/64
    kOperandEncodingI16, //retn imm16
    kOperandEncodingNP, //retn
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingMI8, //mov r/m8, imm8
    kOperandEncodingMI16_32, //mov r/m16/32/64, imm16/32
    kOperandEncodingII16_8, //enter rBP, imm16, imm8
    kOperandEncodingNP, //leave rBP
    kOperandEncodingI16, //retf imm16
    kOperandEncodingNP, //retf
    kOperandEncodingNP, //int3, eFlags
    kOperandEncodingI8, //int imm8, eFlags
    kOperandEncodingNP, //into eFlags
    kOperandEncodingNP, //iret ; iretd ; iretq rFlags
    kOperandEncodingM1, //rol ; ror ; rcl ; rcr ; shl ; sal ; shr ; sal ; shl ; sar r/m8, 1
    kOperandEncodingM1, //rol ; ror ; rcl ; rcr ; shl ; sal ; shr ; sal ; shl ; sar r/m16/32/64, 1
    kOperandEncodingMC, //rol ; ror ; rcl ; rcr ; shl ; sal ; shr ; sal ; shl ; sar r/m8, 1
    kOperandEncodingMC, //rol ; ror ; rcl ; rcr ; shl ; sal ; shr ; sal ; shl ; sar r/m16/32/64, 1
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingInvalid,
    kOperandEncodingNP, //xlat AL, m8 ; xlatb
    kOperandEncodingM, //fadd ; fmul ; fcom ; fcomp ; fsub ; fsubr ; fdiv ; fdivr ST(0), m32real/ST(i)
    kOperandEncodingM, //fld ; fxch ST(0), ST(i)/m32real ; fst ; fstp ; fstp1 m32real, ST(0) ; fldenv ; fnstenv ; fstenv m14/28 ; fldcw m16
    kOperandEncodingM, //fiadd ; fimul ; ficom ; ficomp ; fisub ; fisubr ; fidiv ; fidivr ST(0), m32int ; fcmovb ; fcmove ; fcmovebe ; fcmovu ; fucompp ST(0), ST(i)
    kOperandEncodingM, //fild ST(0), m32int ; fcmovnb ; fcmovne ; fcmovnbe ; fcmovnu ; fucomi ; fcomi ST(0), ST(i) ; fisttp ; fist ; fistp m32int, ST(0) ; fld ST(0), m80real ; fstp m80real, ST(0)
    kOperandEncodingM, //fadd ; fmul ; fcom ; fcomp ; fsub ; fsubr ; fdiv ; fdivr ST(0), m64real ; fadd ; fmul ; fcom2 ; fcomp3 ; fsub ; fsubr ; fdiv ; fdivr ST(i), ST(0)
    kOperandEncodingM, //fld ST(0), m64real ; ffree st(i) ; fisttp m64int, ST(0) ; fxch4 ; fst ; fstp ; fucom ; fucomp ST(0), ST(i) ; fst ; fstp m64real, ST(0) ; frstor ST(0), ST(1), ST(2) ; fnsave m94/108, ST(0), ST(1) ; fnstsw m16
    kOperandEncodingM, //0xde
    kOperandEncodingM, //0xdf
    kOperandEncodingD8, //loopnz ; loopne rCX, rel8
    kOperandEncodingD8, //loopz ; loope rCX, rel8
    kOperandEncodingD8, //loop rCX, rel8
    kOperandEncodingD8, //jecxz ; jrcxz rel8, rCX
    kOperandEncodingI8, //in AL, imm8
    kOperandEncodingI8, //in eAX, imm8
    kOperandEncodingI8, //out imm8, AL
    kOperandEncodingI8, //out imm8, eAX
    kOperandEncodingD16_32, //call rel16/32
    kOperandEncodingD16_32, //jmp rel16/32
    kOperandEncodingInvalid,
    kOperandEncodingD8, //jmp rel8
    kOperandEncodingNP, //in AL, DX
    kOperandEncodingNP, //in eAX, DX
    kOperandEncodingNP, //out DX, AL
    kOperandEncodingNP, //out DX, eAX
    kOperandEncodingPrefix, //lock
    kOperandEncodingNP, //int1 ; icebp eFlags
    kOperandEncodingPrefix, //repnz ; repne ; rep rCX ; scalar double precision (SSE)
    kOperandEncodingPrefix, //repz ; repe ; rep rCX ; scalar single precision (SSE)
    kOperandEncodingNP, //hlt
    kOperandEncodingNP, //cmc
    kOperandEncodingMultipleEncodings | kOperandEncodingM, //0xf6 (multiple types of encodings)
    kOperandEncodingM, //0xf7
    kOperandEncodingNP, //clc
    kOperandEncodingNP, //stc
    kOperandEncodingNP, //cli
    kOperandEncodingNP, //sti
    kOperandEncodingNP, //cld
    kOperandEncodingNP, //std
    kOperandEncodingM, //inc ; dec r/m8
    kOperandEncodingM //0xff
};

static const OPERAND_ENCODING Prefix0fOpcode1OperandEncoding[0x1ff] = {
    kOperandEncodingM, //sldt m16, LDTR ; sldt r16/32/64, LDTR ; str m16, TR ; str r16/32/64, TR ; lldt LDTR, r/m16 ; ltr TR, r/m16 ; verr ; verw r/m16
    kOperandEncodingMultipleOpcodes | kOperandEncodingNP, //0x01
    kOperandEncodingM, //lar r16/32/64, m16 ; lar r16/32/64, r16/32
    kOperandEncodingM, //lsl r16/32/64, m16 ; lsl r16/32/64, r16/32
    kOperandEncodingInvalid,
    kOperandEncodingNP, //syscall RCX, R11, SS, ...
    kOperandEncodingNP, //clts CR0
    kOperandEncodingNP, //sysret SS, EFlags, R11
    kOperandEncodingNP, //invd
    kOperandEncodingNP, //wbinvd
    kOperandEncodingInvalid, //0x0a
    kOperandEncodingNP, //ud2
    kOperandEncodingInvalid, //0x0c
    kOperandEncodingNP, //nop r/m16/32
    kOperandEncodingInvalid, //0x0e
    kOperandEncodingInvalid, //0x0f
    kOperandEncodingRM, //0x10
    kOperandEncodingMR, //0x11
    kOperandEncodingRM, //0x12
    kOperandEncodingMR, //0x13
    kOperandEncodingRM, //0x14
    kOperandEncodingRM, //0x15
    kOperandEncodingRM, //0x16
    kOperandEncodingMR, //0x17
    kOperandEncodingM, //0x18
    kOperandEncodingM, //0x19
    kOperandEncodingM, //0x1a
    kOperandEncodingM, //0x1b
    kOperandEncodingM, //0x1c
    kOperandEncodingM, //0x1d
    kOperandEncodingM, //0x1e
    kOperandEncodingM, //0x1f
    kOperandEncodingRM, //0x20
    kOperandEncodingRM, //0x21
    kOperandEncodingMR, //0x22
    kOperandEncodingMR, //0x23
    kOperandEncodingInvalid, //0x24
    kOperandEncodingInvalid, //0x25
    kOperandEncodingInvalid, //0x26
    kOperandEncodingInvalid, //0x27
    kOperandEncodingRM, //0x28
    kOperandEncodingMR, //0x29
    kOperandEncodingRM, //0x2a
    kOperandEncodingMR, //0x2b
    kOperandEncodingRM, //0x2c
    kOperandEncodingRM, //0x2d
    kOperandEncodingRM, //0x2e
    kOperandEncodingRM, //0x2f
    kOperandEncodingNP, //0x30
    kOperandEncodingNP, //0x31
    kOperandEncodingNP, //0x32
    kOperandEncodingNP, //0x33
    kOperandEncodingNP, //0x34
    kOperandEncodingNP, //0x35
    kOperandEncodingInvalid, //0x36
    kOperandEncodingNP, //0x37
    kOperandEncodingMultipleOpcodes | kOperandEncodingRM, //0x38
    kOperandEncodingInvalid, //0x39
    kOperandEncodingMultipleOpcodes | kOperandEncodingRM, //0x3a
    kOperandEncodingInvalid, //0x3b
    kOperandEncodingInvalid, //0x3c
    kOperandEncodingInvalid, //0x3d
    kOperandEncodingInvalid, //0x3e
    kOperandEncodingInvalid, //0x3f
    kOperandEncodingRM, //0x40
    kOperandEncodingRM, //0x41
    kOperandEncodingRM, //0x42
    kOperandEncodingRM, //0x43
    kOperandEncodingRM, //0x44
    kOperandEncodingRM, //0x45
    kOperandEncodingRM, //0x46
    kOperandEncodingRM, //0x47
    kOperandEncodingRM, //0x48
    kOperandEncodingRM, //0x49
    kOperandEncodingRM, //0x4a
    kOperandEncodingRM, //0x4b
    kOperandEncodingRM, //0x4c
    kOperandEncodingRM, //0x4d
    kOperandEncodingRM, //0x4e
    kOperandEncodingRM, //0x4f
    kOperandEncodingRM, //0x50
    kOperandEncodingMR, //0x51
    kOperandEncodingMR, //0x52
    kOperandEncodingMR, //0x53
    kOperandEncodingMR, //0x54
    kOperandEncodingMR, //0x55
    kOperandEncodingMR, //0x56
    kOperandEncodingMR, //0x57
    kOperandEncodingMR, //0x58
    kOperandEncodingMR, //0x59
    kOperandEncodingMR, //0x5a
    kOperandEncodingMR, //0x5b
    kOperandEncodingMR, //0x5c
    kOperandEncodingMR, //0x5d
    kOperandEncodingMR, //0x5e
    kOperandEncodingMR, //0x5f
    kOperandEncodingMR, //0x60
    kOperandEncodingMR, //0x61
    kOperandEncodingMR, //0x62
    kOperandEncodingMR, //0x63
    kOperandEncodingMR, //0x64
    kOperandEncodingMR, //0x65
    kOperandEncodingMR, //0x66
    kOperandEncodingMR, //0x67
    kOperandEncodingMR, //0x68
    kOperandEncodingMR, //0x69
    kOperandEncodingMR, //0x6a
    kOperandEncodingMR, //0x6b
    kOperandEncodingMR, //0x6c
    kOperandEncodingMR, //0x6d
    kOperandEncodingMR, //0x6e
    kOperandEncodingMR, //0x6f
    kOperandEncodingRMI8, //0x70
    kOperandEncodingMI8, //0x71
    kOperandEncodingMI8, //0x72
    kOperandEncodingMI8, //0x73
    kOperandEncodingRM, //0x74
    kOperandEncodingRM, //0x75
    kOperandEncodingRM, //0x76
    kOperandEncodingNP, //0x77
    kOperandEncodingMR, //0x78
    kOperandEncodingRM, //0x79
    kOperandEncodingInvalid, //0x7a
    kOperandEncodingInvalid, //0x7b
    kOperandEncodingRM, //0x7c
    kOperandEncodingRM, //0x7d
    kOperandEncodingMR, //0x7e
    kOperandEncodingMR, //0x7f
    kOperandEncodingD16_32, //0x80
    kOperandEncodingD16_32, //0x81
    kOperandEncodingD16_32, //0x82
    kOperandEncodingD16_32, //0x83
    kOperandEncodingD16_32, //0x84
    kOperandEncodingD16_32, //0x85
    kOperandEncodingD16_32, //0x86
    kOperandEncodingD16_32, //0x87
    kOperandEncodingD16_32, //0x88
    kOperandEncodingD16_32, //0x89
    kOperandEncodingD16_32, //0x8a
    kOperandEncodingD16_32, //0x8b
    kOperandEncodingD16_32, //0x8c
    kOperandEncodingD16_32, //0x8d
    kOperandEncodingD16_32, //0x8e
    kOperandEncodingD16_32, //0x8f
    kOperandEncodingM, //0x90
    kOperandEncodingM, //0x91
    kOperandEncodingM, //0x92
    kOperandEncodingM, //0x93
    kOperandEncodingM, //0x94
    kOperandEncodingM, //0x95
    kOperandEncodingM, //0x96
    kOperandEncodingM, //0x97
    kOperandEncodingM, //0x98
    kOperandEncodingM, //0x99
    kOperandEncodingM, //0x9a
    kOperandEncodingM, //0x9b
    kOperandEncodingM, //0x9c
    kOperandEncodingM, //0x9d
    kOperandEncodingM, //0x9e
    kOperandEncodingM, //0x9f
    kOperandEncodingNP, //0xa0
    kOperandEncodingNP, //0xa1
    kOperandEncodingNP, //0xa2
    kOperandEncodingMR, //0xa3
    kOperandEncodingMRI8, //0xa4
    kOperandEncodingMR, //0xa5
    kOperandEncodingInvalid, //0xa6
    kOperandEncodingInvalid, //0xa7
    kOperandEncodingNP, //0xa8
    kOperandEncodingNP, //0xa9
    kOperandEncodingNP, //0xaa
    kOperandEncodingMR, //0xab
    kOperandEncodingMRI8, //0xac
    kOperandEncodingMR, //0xad
    kOperandEncodingM, //0xae
    kOperandEncodingRM, //0xaf
    kOperandEncodingMR, //0xb0
    kOperandEncodingMR, //0xb1
    kOperandEncodingRM, //0xb2
    kOperandEncodingMR, //0xb3
    kOperandEncodingRM, //0xb4
    kOperandEncodingRM, //0xb5
    kOperandEncodingRM, //0xb6
    kOperandEncodingRM, //0xb7
    kOperandEncodingRM, //0xb8
    kOperandEncodingRM, //0xb9
    kOperandEncodingMI8, //0xba
    kOperandEncodingMR, //0xbb
    kOperandEncodingRM, //0xbc
    kOperandEncodingRM, //0xbd
    kOperandEncodingRM, //0xbe
    kOperandEncodingRM, //0xbf
    kOperandEncodingMR, //0xc0
    kOperandEncodingMR, //0xc1
    kOperandEncodingRMI8, //0xc2
    kOperandEncodingMR, //0xc3
    kOperandEncodingRMI8, //0xc4
    kOperandEncodingRMI8, //0xc5
    kOperandEncodingRMI8, //0xc6
    kOperandEncodingM, //0xc7
    kOperandEncodingO, //0xc8
    kOperandEncodingO, //0xc9
    kOperandEncodingO, //0xca
    kOperandEncodingO, //0xcb
    kOperandEncodingO, //0xcc
    kOperandEncodingO, //0xcd
    kOperandEncodingO, //0xce
    kOperandEncodingO, //0xcf
    kOperandEncodingRM, //0xd0
    kOperandEncodingRM, //0xd1
    kOperandEncodingRM, //0xd2
    kOperandEncodingRM, //0xd3
    kOperandEncodingRM, //0xd4
    kOperandEncodingRM, //0xd5
    kOperandEncodingMR, //0xd6
    kOperandEncodingRM, //0xd7
    kOperandEncodingRM, //0xd8
    kOperandEncodingRM, //0xd9
    kOperandEncodingRM, //0xda
    kOperandEncodingRM, //0xdb
    kOperandEncodingRM, //0xdc
    kOperandEncodingRM, //0xdd
    kOperandEncodingRM, //0xde
    kOperandEncodingRM, //0xdf
    kOperandEncodingRM, //0xe0
    kOperandEncodingRM, //0xe1
    kOperandEncodingRM, //0xe2
    kOperandEncodingRM, //0xe3
    kOperandEncodingRM, //0xe4
    kOperandEncodingRM, //0xe5
    kOperandEncodingRM, //0xe6
    kOperandEncodingMR, //0xe7
    kOperandEncodingRM, //0xe8
    kOperandEncodingRM, //0xe9
    kOperandEncodingRM, //0xea
    kOperandEncodingRM, //0xeb
    kOperandEncodingRM, //0xec
    kOperandEncodingRM, //0xed
    kOperandEncodingRM, //0xee
    kOperandEncodingRM, //0xef
    kOperandEncodingRM, //0xf0
    kOperandEncodingRM, //0xf1
    kOperandEncodingRM, //0xf2
    kOperandEncodingRM, //0xf3
    kOperandEncodingRM, //0xf4
    kOperandEncodingRM, //0xf5
    kOperandEncodingRM, //0xf6
    kOperandEncodingMR, //0xf7
    kOperandEncodingRM, //0xf8
    kOperandEncodingRM, //0xf9
    kOperandEncodingRM, //0xfa
    kOperandEncodingRM, //0xfb
    kOperandEncodingRM, //0xfc
    kOperandEncodingRM, //0xfd
    kOperandEncodingRM, //0xfe
    kOperandEncodingInvalid //0xff
};

static int OperandSize(const mod_rm_t *ModRM, const size_t Length)
{
    switch (ModRM->mod)
    {
        case 0:
            switch (ModRM->rm)
            {
                case 4:
                    if (Length >= 2)
                    {
                        const sib_t SIB = ((sib_t*)ModRM)[1];
                        if ((SIB.base == 5) && (SIB.index == 4)) return 6; //ModRM + SIB + disp32
                        else return 2; //ModRM + SIB
                    }
                    break;

                case 5:
                    return 5; //ModRM (RIP/EIP) + disp32

                default:
                    return 1; //ModRM
            }
            break;

        case 1:
            if (ModRM->rm == 4) return 3; //ModRM + SIB + disp8
            else return 2; //ModRM + disp8

        case 2:
            if (ModRM->rm == 4) return 6; //ModRM + SIB + disp32
            else return 5; //ModRM + disp32

        case 3:
            return 1; //ModRM
    }

    return 0; //failure
}

static int OperandSize16(const mod_rm_t *ModRM, const size_t Length)
{
    switch (ModRM->mod)
    {
        case 0:
            if (ModRM->rm == 6) return 3; //ModRM + disp16
            else return 1; //ModRM

        case 1:
            return 2; //ModRM + disp8

        case 2:
            return 3; //ModRM + disp16

        case 3:
            return 1; //ModRM
    }

    return 0; //failure
}

static OPERAND_ENCODING EncodingForSpecialCase(const uint8_t * const Bytes, const size_t Length, const _Bool Has0fPrefix, const _Bool Is16bit, const _Bool Use16bitAddressing, const _Bool IsREX_W, unsigned int *AdditionalOpcodes)
{
    if (Has0fPrefix)
    {
        switch (*Bytes)
        {
            case 0x01:
                switch (Bytes[1])
                {
                    case 0xc1:
                    case 0xc2:
                    case 0xc3:
                    case 0xc4:
                    case 0xc8:
                    case 0xc9:
                    case 0xd0:
                    case 0xd1:
                    case 0xf8:
                    case 0xf9:
                        *AdditionalOpcodes = 1;
                        break;
                }
                return kOperandEncodingM;

            case 0x38:
                switch (Bytes[1])
                {
                    case 0x80:
                    case 0x81:
                    case 0xf0:
                    case 0xf1:
                        *AdditionalOpcodes = 1;
                        return kOperandEncodingRM;

                    default:
                        return kOperandEncodingInvalid;
                }
                break;

            case 0x3a:
                switch (Bytes[1])
                {
                    case 0x08:
                    case 0x09:
                    case 0x0a:
                    case 0x0b:
                    case 0x0c:
                    case 0x0d:
                    case 0x0e:
                    case 0x14:
                    case 0x15:
                    case 0x16:
                    case 0x17:
                    case 0x20:
                    case 0x21:
                    case 0x22:
                    case 0x42:
                    case 0x60:
                    case 0x61:
                    case 0x62:
                    case 0x63:
                        *AdditionalOpcodes = 1;
                        return kOperandEncodingRMI8;

                    case 0x0f:
                    case 0x40:
                    case 0x41:
                        *AdditionalOpcodes = 1;
                        return kOperandEncodingRM;

                    default:
                        return kOperandEncodingInvalid;
                }
                break;
        }
    }

    else
    {
        switch (*Bytes)
        {
            case 0xf6:
                if (Length >= 2)
                {
                    if (((mod_rm_t*)Bytes)[1].reg > 1) return kOperandEncodingM; //0xf6 2..7
                    else return kOperandEncodingMI8; //0xf6 1, 2
                }
                break;
        }
    }

    return kOperandEncodingInvalid;
}

#define MAX_INSTRUCTION_LENGTH 0xf

int InstructionSize_x86_64(const uint8_t * const Bytes, size_t Length)
{
    if (Length > MAX_INSTRUCTION_LENGTH) Length = MAX_INSTRUCTION_LENGTH; //comment out if you want it to continue regardless even though this is the longest valid instruction length

    int PrefixCount = 0;
    _Bool Has0fPrefix = FALSE, Is16bit = FALSE, Use16bitAddressing = FALSE, IsREX_W = FALSE;

    for (size_t Loop = 0; Loop < Length; Loop++)
    {
        if ((!Has0fPrefix) && (Opcode1OperandEncoding[Bytes[Loop]] == kOperandEncodingPrefix))
        {
            switch (Bytes[Loop])
            {
                //prefixes
                case 0x0f:
                    Has0fPrefix = TRUE;
                    break;

                case 0x66:
                    Is16bit = TRUE;
                    break;

                case 0x67:
                    Use16bitAddressing = TRUE;
                    break;

                case 0x48:
                case 0x49:
                case 0x4a:
                case 0x4b:
                case 0x4c:
                case 0x4d:
                case 0x4e:
                case 0x4f:
                    IsREX_W = TRUE;
                    break;

                    /*
                     case 0x26:
                     case 0x2e:
                     case 0x36:
                     case 0x3e:
                     case 0x40:
                     case 0x41:
                     case 0x42:
                     case 0x43:
                     case 0x44:
                     case 0x46:
                     case 0x47:
                     case 0x64:
                     case 0x65:
                     //case 0x9b:
                     case 0xf0:
                     case 0xf2:
                     case 0xf3:
                     */
            }

            PrefixCount++;
        }

        else
        {
            OPERAND_ENCODING Encoding;
            if (Has0fPrefix) Encoding = Prefix0fOpcode1OperandEncoding[Bytes[Loop++]];
            else Encoding = Opcode1OperandEncoding[Bytes[Loop++]];


            unsigned int AdditionalOpcodes = 0;
            if (Encoding & kOperandEncodingMultipleEncodings) Encoding = EncodingForSpecialCase(&Bytes[Loop - 1], Length - Loop - 1, Has0fPrefix, Is16bit, Use16bitAddressing, IsREX_W, &AdditionalOpcodes);

            Loop += AdditionalOpcodes;

            const _Bool SupportsImmediate64 = Encoding & kOperandEncodingSupportsImm64;
            switch (Encoding & ~kOperandEncodingFlags)
            {
                case kOperandEncodingInvalid:
                    return INSTRUCTION_INVALID;

                case kOperandEncodingNP: //no operand
                    return PrefixCount + 1 + AdditionalOpcodes;

                case kOperandEncodingM: //modrm:r/m
                    if (Loop < Length)
                    {
                        const int OpSize = (Use16bitAddressing? OperandSize16 : OperandSize)((mod_rm_t*)&Bytes[Loop], Length - Loop);

                        if (OpSize) return PrefixCount + 1 + OpSize + AdditionalOpcodes;
                    }
                    break;

                case kOperandEncodingMI8: //modrm:r/m, imm8
                    if (Loop < Length)
                    {
                        const int OpSize = (Use16bitAddressing? OperandSize16 : OperandSize)((mod_rm_t*)&Bytes[Loop], Length - Loop);

                        if (OpSize) return PrefixCount + 2 + OpSize + AdditionalOpcodes;
                    }
                    break;

                case kOperandEncodingMI16_32: //modrm:r/m, imm16/32
                    if (Loop < Length)
                    {
                        const int OpSize = (Use16bitAddressing? OperandSize16 : OperandSize)((mod_rm_t*)&Bytes[Loop], Length - Loop);

                        if (OpSize) return PrefixCount + 1 + OpSize + (int[]){ 4, 2, SupportsImmediate64? 8 : 4 }[Is16bit | (IsREX_W << 1)] + AdditionalOpcodes;
                    }
                    break;

                case kOperandEncodingI8:  //(AL/AX/EAX/RAX), imm8
                    return PrefixCount + 2 + AdditionalOpcodes;

                case kOperandEncodingI16_32:  //(AL/AX/EAX/RAX), imm8
                    return PrefixCount + 1 + (int[]){ 4, 2, SupportsImmediate64? 8 : 4 }[Is16bit | (IsREX_W << 1)] + AdditionalOpcodes;

                case kOperandEncodingI16:
                    return PrefixCount + 3 + AdditionalOpcodes;

                case kOperandEncodingII16_8:
                    return PrefixCount + 4 + AdditionalOpcodes;
            }

            return INSTRUCTION_INVALID;
        }
    }

    return INSTRUCTION_INVALID;
}
