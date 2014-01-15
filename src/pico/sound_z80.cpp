/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"

struct DrZ80 drZ80;

static unsigned int DrZ80_rebasePC(unsigned short a)
{
	drZ80.Z80PC_BASE = (unsigned int) Pico.zram;
	return drZ80.Z80PC_BASE + a;
}

static unsigned int DrZ80_rebaseSP(unsigned short a)
{
	drZ80.Z80SP_BASE = (unsigned int) Pico.zram;
	return drZ80.Z80SP_BASE + a;
}

static void DrZ80_irq_callback()
{
	drZ80.Z80_IRQ = 0; // lower irq when accepted
}

static unsigned char z80_in(unsigned short p)
{
	elprintf(EL_ANOMALY, "Z80 port %04x read", p);
	return 0xff;
}

static void z80_out(unsigned short p,unsigned char d)
{
	elprintf(EL_ANOMALY, "Z80 port %04x write %02x", p, d);
}

// z80 functionality wrappers
void z80_init()
{
	memset(&drZ80, 0, sizeof(struct DrZ80));
	drZ80.z80_rebasePC=DrZ80_rebasePC;
	drZ80.z80_rebaseSP=DrZ80_rebaseSP;
	drZ80.z80_read8   =z80_read;
	drZ80.z80_read16  =z80_read16;
	drZ80.z80_write8  =z80_write;
	drZ80.z80_write16 =z80_write16;
	drZ80.z80_in      =z80_in;
	drZ80.z80_out     =z80_out;
	drZ80.z80_irq_callback=DrZ80_irq_callback;
}

void z80_reset(void)
{
	memset(&drZ80, 0, 0x54);
	drZ80.Z80F  = (1<<2);  // set ZFlag
	drZ80.Z80F2 = (1<<2);  // set ZFlag
	drZ80.Z80IX = 0xFFFF << 16;
	drZ80.Z80IY = 0xFFFF << 16;
	drZ80.Z80IM = 0; // 1?
	drZ80.Z80PC = drZ80.z80_rebasePC(0);
	drZ80.Z80SP = drZ80.z80_rebaseSP(0x2000); // 0xf000 ?

	Pico.m.z80_fakeval = 0; // for faking when Z80 is disabled
}


void z80Sl(const QString &name)
{
	static const int Size = 0x54;
	if (!emsl.save)
		emsl.array(name, &drZ80, Size);
	drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
	drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
	if (emsl.save)
		emsl.array(name, &drZ80, Size);
}

void z80_debug(char *dstr)
{
	sprintf(dstr, "Z80 state: PC: %04x SP: %04x\n", drZ80.Z80PC-drZ80.Z80PC_BASE, drZ80.Z80SP-drZ80.Z80SP_BASE);
}
