@ vim:filetype=armasm

@	Free for non-commercial use.
@	For commercial use, separate licencing terms must be obtained.
@	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
@	(c) Copyright 2011, elemental

.text
.align 4
@ TODO rewrite
@ limit
@ reg=int_sample, lr=1, r3=tmp, kills flags
.macro Limit reg
    add     r3, lr, \reg, asr #15
    bics    r3, r3, #1			@ in non-overflow conditions r3 is 0 or 1
    movne   \reg, #0x8000
    subpl   \reg, \reg, #1
.endm

@ limit and shift up by 16
@ reg=int_sample, lr=1, r3=tmp, kills flags
.macro Limitsh reg
@    movs    r4, r3, asr #16
@    cmnne   r4, #1
@    beq     c32_16_no_overflow
@    tst     r4, r4
@    mov     r3, #0x8000
@    subpl   r3, r3, #1

    add     r3, lr, \reg, asr #15
    bics    r3, r3, #1			@ in non-overflow conditions r3 is 0 or 1
    moveq   \reg, \reg, lsl #16
    movne   \reg, #0x80000000
    subpl   \reg, \reg, #0x00010000
.endm


@ mix 32bit audio (with 16bits really used, upper bits indicate overflow) with normal 16 bit audio with left channel only
@ warning: this function assumes dest is word aligned
.global mix_32_to_16l_stereo @ short *dest, int *src, int count

mix_32_to_16l_stereo:
    stmfd   sp!, {r4-r8,lr}

    mov     lr, #1

    mov     r2, r2, lsl #1
    subs    r2, r2, #4
    bmi     m32_16l_st_end

m32_16l_st_loop:
    @ TODO only left is sampled from dst - it is mono !
    ldmia   r0,  {r8,r12}
    ldmia   r1!, {r4-r7}
    mov     r8, r8, lsl #16
    mov     r12,r12,lsl #16
    add     r4, r4, r8, asr #16
    add     r5, r5, r8, asr #16
    add     r6, r6, r12,asr #16
    add     r7, r7, r12,asr #16
    Limitsh r4
    Limitsh r5
    Limitsh r6
    Limitsh r7
    subs    r2, r2, #4
    orr     r4, r5, r4, lsr #16
    orr     r5, r7, r6, lsr #16
    stmia   r0!, {r4,r5}
    bpl     m32_16l_st_loop

m32_16l_st_end:
    @ check for remaining bytes to convert
    tst     r2, #2
    beq     m32_16l_st_no_unal2
    ldrsh   r6, [r0]
    ldmia   r1!,{r4,r5}
    add     r4, r4, r6
    add     r5, r5, r6
    Limitsh r4
    Limitsh r5
    orr     r4, r5, r4, lsr #16
    str     r4, [r0], #4

m32_16l_st_no_unal2:
    ldmfd   sp!, {r4-r8,lr}
    bx      lr
