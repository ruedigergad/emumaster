/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <base/emu.h>
#include "amiga.h"
#include "mem.h"
#include "custom.h"
#include "spu.h"
#include "blitter.h"
#include "cpu.h"
#include "events.h"

// note: Shadow of the beast intentionally causes address errors
// Project-X uses trace mode.

struct Cyclone m68kContext;
u32 amigaCpuSpcFlags;
u32 amigaCpuCycleCounter;
u32 amigaCpuInterrupts;

#ifdef USE_CYCLONE_MEMHANDLERS
#define MH_STATIC extern "C"
#else
#define MH_STATIC static
#endif

MH_STATIC u32 cyclone_read8 (u32 a);
MH_STATIC u32 cyclone_read16(u32 a);
MH_STATIC u32 cyclone_read32(u32 a);
MH_STATIC void cyclone_write8 (u32 a, u8  d);
MH_STATIC void cyclone_write16(u32 a, u16 d);
MH_STATIC void cyclone_write32(u32 a, u32 d);

static u32 cpuCheckPC(u32 pc);
static int cpuIrqAck(int level);
static int cpuUnrecognizedCallback(void);

void amigaCpuInit() {
	CycloneInit();
	memset(&m68kContext, 0, sizeof(m68kContext));
	m68kContext.checkpc = cpuCheckPC;
	m68kContext.read8   = m68kContext.fetch8  = cyclone_read8;
	m68kContext.read16  = m68kContext.fetch16 = cyclone_read16;
	m68kContext.read32  = m68kContext.fetch32 = cyclone_read32;
	m68kContext.write8  = cyclone_write8;
	m68kContext.write16 = cyclone_write16;
	m68kContext.write32 = cyclone_write32;
	m68kContext.IrqCallback = cpuIrqAck;
	m68kContext.UnrecognizedCallback = cpuUnrecognizedCallback;
}

void amigaCpuReleaseTimeslice() {
	m68kContext.cycles = -1;
}

static u32 cpuCheckPC(u32 pc) {
	static const int LoopCode = 0x60FE60FE;

	pc -= m68kContext.membase;
	// pc &= ~1; // leave it for address error emulation
	pc &= ~0xFF000000;

	u8 *p = amigaMemBaseAddr[pc >> 16];
	if ((int)p & 1) {
		qDebug("Cyclone problem: branched to unknown memory location: %06x\n", pc);
		p = (u8 *)&LoopCode - pc;
	}
	m68kContext.membase = (uint)p;
	return (uint)p + pc;
}


static int cpuIrqAck(int level) {
	amigaCpuInterrupts &= ~(1 << level);
	amigaCpuIrqUpdate(0);
	return CYCLONE_INT_ACK_AUTOVECTOR;
}

#define RTAREA_BASE 0xF00000

static int cpuUnrecognizedCallback() {
	u32 pc = m68kContext.pc - m68kContext.membase;
	u16 opcode = *(u16 *)m68kContext.pc;

	if (opcode == 0x4E7B && amigaMemGetLong(0x10) == 0 && (pc & 0xF80000) == 0xF80000) {
		qDebug("Your Kickstart requires a 68020 CPU. Giving up.\n");
		amigaCpuSetSpcFlag(SpcFlagBrk);
		m68kContext.cycles = 0;
		return 1;
	}
	if (opcode == 0xFF0D) {
		if ((pc & 0xFFFF0000) == RTAREA_BASE) {
			// User-mode STOP replacement 
			amigaCpuSetSpcFlag(SpcFlagStop);
			m68kContext.state_flags |= 1;
			m68kContext.cycles = 0;
		}
		return 1;
	}
	if ((opcode & 0xF000) == 0xA000 && (pc & 0xFFFF0000) == RTAREA_BASE) {
		// Calltrap.
		m68kContext.pc += 2;
		return 1;
	}
	if ((opcode & 0xF000) == 0xF000) {
		// exp8
		return 0;
	}
	if ((opcode & 0xF000) == 0xA000) {
		// expA
		return 0;
	}
	qDebug("Illegal instruction: %04x at %08x\n", opcode, pc);
	return 0;
}

static int cpuDoSpecialities(int cycles) {
	if (amigaCpuSpcFlags & SpcFlagCopper)
		amigaDoCopper();
	while ((amigaCpuSpcFlags & SpcFlagBltNasty) && cycles > 0) {
		int c = blitnasty();
		if (!c) {
			cycles -= 2 * CYCLE_UNIT;
			if (cycles < CYCLE_UNIT)
				cycles = 0;
			c = 1;
		}
		amigaEventsHandle(c * CYCLE_UNIT);
		if (amigaCpuSpcFlags & SpcFlagCopper)
			amigaDoCopper();
	}
	if (amigaCpuSpcFlags & SpcFlagBrk) {
		amigaCpuClearSpcFlag(SpcFlagBrk);
		return 1;
	}
	return 0;
}

void amigaCpuReset() {
	m68kContext.state_flags = 0;
	m68kContext.osp = 0;
	m68kContext.srh = 0x27; // Supervisor mode
	m68kContext.flags = 0;
	m68kContext.a[7] = amigaMemGetLong(0x00F80000);

	amigaCpuInterrupts = 0;
	amigaCpuSpcFlags = 0;
	amigaCpuIrqUpdate(0);
	amigaCpuSetPC(amigaMemGetLong(0x00F80004));
}

void amigaCpuRun() {
	for (;;) {
		u32 cycles_actual = amigaCpuCycleCounter;
		u32 cycles = amigaNextEvent - amigaCycles;
		cycles >>= 7;
		m68kContext.cycles = cycles - 1;
		CycloneRun(&m68kContext);
		amigaCpuCycleCounter += cycles - 1 - m68kContext.cycles;
		cycles = (amigaCpuCycleCounter - cycles_actual) << 8;
		do {
			amigaEventsHandle(cycles);
			if (amigaCpuSpcFlags)
				if (cpuDoSpecialities(cycles))
					return;
			cycles = 2048;
		} while ((amigaNextEvent-amigaCycles) <= 2048);
	}
}

void amigaCpuSl() {
	emsl.begin("cpu");
	QByteArray cycloneSl(128, '\0');
	if (emsl.save)
		CyclonePack(&m68kContext, cycloneSl.data());
	emsl.var("cyclone", cycloneSl);
	if (!emsl.save)
		CycloneUnpack(&m68kContext, cycloneSl.data());
	emsl.var("spcFlags", amigaCpuSpcFlags);
	emsl.var("cycles", amigaCpuCycleCounter);
	emsl.var("ints", amigaCpuInterrupts);
	emsl.end();
}

/* memory handlers */
#ifndef USE_CYCLONE_MEMHANDLERS
static u8 cyclone_read8(u32 a) {
	a &= ~0xff000000;
	u8 *p = amigaMemBaseAddr[a>>16];
	if ((int)p & 1)
	{
		AmigaAddrBank *ab = (AmigaAddrBank *) ((unsigned)p & ~1);
		u32 ret = ab->bget(a);
		mdprintf("@ %06x, handler, =%02x", a, ret);
		return ret;
	}
	else
	{
		mdprintf("@ %06x, =%02x", a, p[a^1]);
		return p[a^1];
	}
}

static u16 cyclone_read16(u32 a)
{
	a &= ~0xff000000;
	u16 *p = (u16 *) amigaMemBaseAddr[a>>16];
	if ((int)p & 1)
	{
		AmigaAddrBank *ab = (AmigaAddrBank *) ((unsigned)p & ~1);
		u32 ret = ab->wget(a);
		mdprintf("@ %06x, handler, =%04x", a, ret);
		return ret;
	}
	else
	{
		mdprintf("@ %06x, =%04x", a, p[a>>1]);
		return p[a>>1];
	}
}

static u32 cyclone_read32(u32 a)
{
#ifdef SPLIT_32_2_16
	return (read16(a)<<16) | read16(a+2);
#else
	a &= ~0xff000000;
	u16 *p = (u16 *) amigaMemBaseAddr[a>>16];
	if ((int)p & 1)
	{
		AmigaAddrBank *ab = (AmigaAddrBank *) ((unsigned)p & ~1);
		u32 ret = ab->lget(a);
		mdprintf("@ %06x, handler, =%08x", a, ret);
		return ret;
	}
	else
	{
		mdprintf("@ %06x, =%08x", a, (p[a>>1]<<16)|p[(a>>1)+1]);
		a >>= 1;
		return (p[a]<<16)|p[a+1];
	}
#endif
}

static void cyclone_write8(u32 a, u8 d)
{
	a &= ~0xff000000;
	u8 *p = amigaMemBaseAddr[a>>16];
	if ((int)p & 1)
	{
		AmigaAddrBank *ab = (AmigaAddrBank *) ((unsigned)p & ~1);
		mdprintf("@ %06x, handler, =%02x", a, d);
		ab->bput(a, d&0xff);
	}
	else
	{
		mdprintf("@ %06x, =%02x", a, d);
		p[a^1] = d;
	}
}

static void cyclone_write16(u32 a,u16 d)
{
	a &= ~0xff000000;
	u16 *p = (u16 *) amigaMemBaseAddr[a>>16];
	if ((int)p & 1)
	{
		AmigaAddrBank *ab = (AmigaAddrBank *) ((unsigned)p & ~1);
		mdprintf("@ %06x, handler, =%04x", a, d);
		ab->wput(a, d&0xffff);
	}
	else
	{
		mdprintf("@ %06x, =%04x", a, d);
		p[a>>1] = d;
	}
}

static void cyclone_write32(u32 a,u32 d)
{
#ifdef SPLIT_32_2_16
	write16(a, d>>16);
	write16(a+2, d);
#else
	a &= ~0xff000000;
	u16 *p = (u16 *) amigaMemBaseAddr[a>>16];
	if ((int)p & 1)
	{
		AmigaAddrBank *ab = (AmigaAddrBank *) ((unsigned)p & ~1);
		mdprintf("@ %06x, handler, =%08x", a, d);
		ab->lput(a, d);
	}
	else
	{
		mdprintf("@ %06x, =%08x", a, d);
		a >>= 1;
		p[a] = d >> 16;
		p[a+1] = d;
	}
#endif
}
#endif
