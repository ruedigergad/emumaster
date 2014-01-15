/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

.align 4
.global	memset32

/* void memset32(void *r0, u32 r1, int r2) */
memset32:
	cmp r2, #1
	streq r1, [r0], #4
	bxle lr
	cmp r2, #4
	bgt memset32_neon
memset32_small:
	str r1, [r0], #4
	subs r2, r2, #1
	bne memset32_small
	bx lr
memset32_neon:
	cmp r2, #16
	blt memset32_dropthru
	vdup.32 q0, r1
	vmov q1, q0
	cmp r2, #32
	blt memset32_16
	cmp r2, #64
	blt memset32_32
	cmp r2, #128
	blt memset32_64
memset32_128:
	movs r12, r2, lsr #7
memset32_loop128:
	subs r12, r12, #1
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	bne memset32_loop128
	ands r2, r2, #0x7f
	bxeq lr
memset32_64:
	movs r12, r2, lsr #6
	beq memset32_32
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	ands r2, r2, #0x3f
	bxeq lr
memset32_32:
	movs r12, r2, lsr #5
	beq memset32_16
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
	ands r2, r2, #0x1f
	bxeq lr
memset32_16:
	movs r12, r2, lsr #4
	beq memset32_dropthru
	and r2, r2, #0xf
	vst1.64 {q0, q1}, [r0]!
	vst1.64 {q0, q1}, [r0]!
memset32_dropthru:
	rsb r2, r2, #15
	add pc, pc, r2, lsl #2
	nop
	str r1, [r0, #56]
	str r1, [r0, #52]
	str r1, [r0, #48]
	str r1, [r0, #44]
	str r1, [r0, #40]
	str r1, [r0, #36]
	str r1, [r0, #32]
	str r1, [r0, #28]
	str r1, [r0, #24]
	str r1, [r0, #20]
	str r1, [r0, #16]
	str r1, [r0, #12]
	str r1, [r0, #8]
	str r1, [r0, #4]
	str r1, [r0, #0]
	bx lr
