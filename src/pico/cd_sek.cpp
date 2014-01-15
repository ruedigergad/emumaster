/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"

int SekCycleCntS68k = 0; // cycles done in this frame
int SekCycleAimS68k = 0; // cycle aim
struct Cyclone PicoCpuCS68k;

static int newIrqLevel(int level)
{
	int newLevel = 0;
	Pico_mcd->m.s68k_pend_ints &= ~(1 << level);
	int irqs = Pico_mcd->m.s68k_pend_ints;
	irqs &= Pico_mcd->s68k_regs[0x33];
	while ((irqs >>= 1))
		newLevel++;
	return newLevel;
}

// interrupt acknowledgement
static int SekIntAckS68k(int level)
{
	int newLevel = newIrqLevel(level);
	picoDebugMcdSek("s68kACK %i -> %i", level, newLevel);
	PicoCpuCS68k.irq = newLevel;
	return CYCLONE_INT_ACK_AUTOVECTOR;
}

static void SekResetAckS68k()
{
	picoDebugMcdSek("s68k: Reset encountered @ %06x", SekPcS68k);
}

static int SekUnrecognizedOpcodeS68k()
{
	uint pc = SekPcS68k;
	uint op = PicoCpuCS68k.read16(pc);
	picoDebugMcdSek("Unrecognized Opcode %04x @ %06x", op, pc);
	return 0;
}

void SekInitS68k()
{
	memset(&PicoCpuCS68k, 0, sizeof(PicoCpuCS68k));
	PicoCpuCS68k.IrqCallback = SekIntAckS68k;
	PicoCpuCS68k.ResetCallback = SekResetAckS68k;
	PicoCpuCS68k.UnrecognizedCallback = SekUnrecognizedOpcodeS68k;
}

void SekResetS68k()
{
	PicoCpuCS68k.state_flags = 0;
	PicoCpuCS68k.osp = 0;
	PicoCpuCS68k.srh = 0x27; // Supervisor mode
	PicoCpuCS68k.flags = 4;   // Z set
	PicoCpuCS68k.irq = 0;
	PicoCpuCS68k.a[7] = PicoCpuCS68k.read32(0); // Stack Pointer
	PicoCpuCS68k.membase = 0;
	PicoCpuCS68k.pc = PicoCpuCS68k.checkpc(PicoCpuCS68k.read32(4)); // Program Counter
}

int SekInterruptS68k(int irq)
{
	Pico_mcd->m.s68k_pend_ints |= 1 << irq;
	int irqs = Pico_mcd->m.s68k_pend_ints >> 1;
	int realIrq = 1;
	while ((irqs >>= 1))
		realIrq++;
	PicoCpuCS68k.irq = realIrq;
	return 0;
}

