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

#ifndef AMIGACPU_H
#define AMIGACPU_H

#include <base/emu.h>
#include "cyclone.h"

enum SpcFlag {
	SpcFlagStop			= 0x002,
	SpcFlagCopper		= 0x004,
	SpcFlagBrk			= 0x010,
	SpcFlagBltNasty		= 0x100
};

extern u32 amigaCpuSpcFlags;
extern u32 amigaCpuCycleCounter;
extern u32 amigaCpuInterrupts;
extern struct Cyclone m68kContext;

extern void amigaCpuRun();
extern void amigaCpuInit();
extern void amigaCpuReset();
extern void amigaCpuReleaseTimeslice();
extern void amigaCpuSl();

static inline void amigaCpuIrqUpdate(bool endTimeslice) {
	int level = 7;
	for (; level; level--) {
		if (amigaCpuInterrupts & (1 << level))
			break;
	}
	m68kContext.irq = level;

	if (endTimeslice && m68kContext.cycles >= 0 && !(m68kContext.state_flags & 1)) {
		amigaCpuCycleCounter += 24 - 1 - m68kContext.cycles;
		m68kContext.cycles = 24 - 1;
	}
}

static __inline__ u32 amigaCpuIntMask() { return m68kContext.srh & 7; }
static __inline__ u32 amigaCpuGetPC() { return m68kContext.pc - m68kContext.membase; }

static __inline__ void amigaCpuSetPC(u32 pc) {
	m68kContext.membase = 0;
	m68kContext.pc = m68kContext.checkpc(pc);
}

static __inline__ void amigaCpuSetSpcFlag(int x) { amigaCpuSpcFlags |= x; }
static __inline__ void amigaCpuClearSpcFlag(int x) { amigaCpuSpcFlags &= ~x; }

#endif // AMIGACPU_H
