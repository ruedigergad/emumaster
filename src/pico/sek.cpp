/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2004 by Dave
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"

int SekCycleCnt = 0; // cycles done in this frame
int SekCycleAim = 0; // cycle aim
uint SekCycleCntT = 0;
struct Cyclone PicoCpuCM68k;

// interrupt acknowledgment
static int SekIntAck(int level) {
	// try to emulate VDP's reaction to 68000 int ack
	if (level == 4) {
		Pico.video.pending_ints = 0;
		picoDebugSek("hack: @ %06x [%i]", SekPc, SekCycleCnt);
	} else if (level == 6) {
		Pico.video.pending_ints &= ~0x20;
		picoDebugSek("vack: @ %06x [%i]", SekPc, SekCycleCnt);
	}
	PicoCpuCM68k.irq = 0;
	return CYCLONE_INT_ACK_AUTOVECTOR;
}

static void SekResetAck(void) {
	picoDebugSek("Reset encountered @ %06x", SekPc);
}

static int SekUnrecognizedOpcode() {
	uint pc = SekPc;
	uint op = PicoCpuCM68k.read16(pc);
	picoDebugSek("Unrecognized Opcode %04x @ %06x", op, pc);
	// see if we are not executing trash
	if (pc < 0x200 || (pc > Pico.romsize+4 && (pc&0xe00000)!=0xe00000)) {
		PicoCpuCM68k.cycles = 0;
		PicoCpuCM68k.state_flags |= 1;
		return 1;
	}
	return 0;
}

int SekInit() {
	CycloneInit();
	memset(&PicoCpuCM68k, 0, sizeof(PicoCpuCM68k));
	PicoCpuCM68k.IrqCallback = SekIntAck;
	PicoCpuCM68k.ResetCallback = SekResetAck;
	PicoCpuCM68k.UnrecognizedCallback = SekUnrecognizedOpcode;
	PicoCpuCM68k.flags = 4;   // Z set
	return 0;
}

// Reset the 68000:
int SekReset() {
	if (!Pico.rom)
		return 1;

	PicoCpuCM68k.state_flags = 0;
	PicoCpuCM68k.osp = 0;
	PicoCpuCM68k.srh = 0x27; // Supervisor mode
	PicoCpuCM68k.irq = 0;
	PicoCpuCM68k.a[7] = PicoCpuCM68k.read32(0); // Stack Pointer
	PicoCpuCM68k.membase = 0;
	PicoCpuCM68k.pc = PicoCpuCM68k.checkpc(PicoCpuCM68k.read32(4)); // Program Counter
	return 0;
}

// data must be word aligned
void SekState(int *data) {
	memcpy32(data, (int *)PicoCpuCM68k.d, 0x44/4);
}

void SekSetRealTAS(int use_real) {
	CycloneSetRealTAS(use_real);
}
