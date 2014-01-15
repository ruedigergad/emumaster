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

#ifndef AMIGAMEM_H
#define AMIGAMEM_H

#include <base/emu.h>

typedef u32 (*AmigaMemGetFunc)(u32);
typedef void (*AmigaMemPutFunc)(u32, u32);

class AmigaAddrBank {
public:
	AmigaMemGetFunc lget, wget, bget;
	AmigaMemPutFunc lput, wput, bput;
	/* For those banks that refer to real memory, we can save the whole trouble
	   of going through function calls, and instead simply grab the memory
	   ourselves. This holds the memory address where the start of memory is
	   for this particular bank. */
	u8 *baseAddr;
};

extern void amigaMemInit();
extern void amigaMemShutdown();
extern void amigaMemReset();
extern bool amigaLoadKickstart(const QString &fileName);
extern void amigaMemSl();

extern u8 *amigaMemChip;
extern u32 amigaMemChipSize;

extern const AmigaAddrBank *amigaMemBanks[65536];
extern u8 *amigaMemBaseAddr[65536];

extern AmigaAddrBank amigaMemChipBank;
extern AmigaAddrBank amigaMemKickBank;
extern AmigaAddrBank amigaCustomBank;
extern AmigaAddrBank amigaClockBank;
extern AmigaAddrBank amigaCiaBank;

extern u8 *amigaMemChipXlate(u32 addr, u32 size);

static inline int amigaMemBankIndex(u32 addr)
{ return addr >> 16; }

static inline const AmigaAddrBank &amigaMemBankGet(u32 addr)
{ return *amigaMemBanks[amigaMemBankIndex(addr)]; }

static inline void amigaMemBankPut(u32 addr, const AmigaAddrBank *bank, u32 realStart) {
	amigaMemBanks[amigaMemBankIndex(addr)] = bank;
	if (bank->baseAddr)
		amigaMemBaseAddr[amigaMemBankIndex(addr)] = bank->baseAddr - realStart;
	else
		amigaMemBaseAddr[amigaMemBankIndex(addr)] = (u8*)(((long)bank)+1);
}

extern void amigaMemBanksMap(AmigaAddrBank *bank, int first, int count, int realsize);

static inline u32 amigaMemGetLong(u32 addr)
{ return (*(amigaMemBankGet(addr).lget))(addr); }
static inline u32 amigaMemGetWord(u32 addr)
{ return (*(amigaMemBankGet(addr).wget))(addr); }
static inline void amigaMemPutWord(u32 addr, u32 w)
{ (*(amigaMemBankGet(addr).wput))(addr, w); }

static inline u16 amigaMemChipGetWord(u32 addr)
{ return *(u16 *)&amigaMemChip[addr & ~0xFFF00000]; }
static inline void amigaMemChipPutWord(u32 addr, u16 data)
{ *(u16 *)&amigaMemChip[addr & ~0xFFF00000] = data; }

static inline u32 amigaMemDoGetLong(u32 *a) {
	u16 *w = (u16 *)a;
	return ((w[0]<<16) | w[1]);
}

static inline void amigaMemDoPutLong(u32 *a, u32 v) {
	u16 *w = (u16 *)a;
	w[0] =(v>>16)&0xFFFF;
	w[1] =(v>> 0)&0xFFFF;
}

#endif // AMIGAMEM_H
