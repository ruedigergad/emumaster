.global gpuUnaiConvertBgr555ToRgb565

@ r0: destination buffer
@ r1: source buffer
@ r2: width
@ r3: height

.align 8
0:
	.hword (0x1F << 5)
	.hword (0x1F << 5)
	.hword (0x1F << 5)
	.hword (0x1F << 5)

gpuUnaiConvertBgr555ToRgb565:
	stmdb sp!, { r4 }
	mov r12, r3						@ r12 = y counter

	adr r3, 0b
	vld1.u16 { d30 }, [ r3 ]
	vmov d31, d30

	mov r4, #2048
	sub r4, r2, lsl #1

	mov r2, r2, lsr #5
	mov r3, r2						@ r3 = x counter
1:
	vldm r1!, { q0, q1, q2, q3 }

	vshr.u16 q4, q0, #10			@ isolate red elements
	vshr.u16 q5, q1, #10
	vshr.u16 q6, q2, #10
	vshr.u16 q7, q3, #10

	vshl.u16 q8, q0, #11			@ isolate blue elements
	vshl.u16 q9, q1, #11
	vshl.u16 q10, q2, #11
	vshl.u16 q11, q3, #11

	vand.u16 q0, q0, q15			@ isolate green elements
	vand.u16 q1, q1, q15
	vand.u16 q2, q2, q15
	vand.u16 q3, q3, q15

	vshl.u16 q0, q0, #1				@ shift green elements
	vshl.u16 q1, q1, #1
	vshl.u16 q2, q2, #1
	vshl.u16 q3, q3, #1

	vorr.u16 q0, q0, q4				@ combine green/red elements
	vorr.u16 q1, q1, q5
	vorr.u16 q2, q2, q6
	vorr.u16 q3, q3, q7

	vorr.u16 q0, q0, q8				@ combine rgb elements
	vorr.u16 q1, q1, q9
	vorr.u16 q2, q2, q10
	vorr.u16 q3, q3, q11

	vstm r0!, { q0, q1, q2, q3 }	@ write out 64 pixels on row

	subs r3, r3, #1
	bne 1b

	add r1, r4

	mov r3, r2
	subs r12, r12, #1
	bne 1b

	ldmia sp!, { r4 }
	bx lr
