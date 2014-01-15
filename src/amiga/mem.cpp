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

#include "amiga.h"
#include "mem.h"
#include "custom.h"
#include "events.h"
#include "cpu.h"
#include <stdlib.h>
#include <QFile>

#define SWAP_W(SWAB_DATA) (0xFFFF & (((SWAB_DATA)>>8) | ((SWAB_DATA)<<8)))

#define SWAP_L(SWAB_DATA) \
	((((u32)(SWAB_DATA) & 0xFF000000) >> 24)   | \
	 (((u32)(SWAB_DATA) & 0x00FF0000) >> 8) | \
	 (((u32)(SWAB_DATA) & 0x0000FF00) << 8)  | \
	 (((u32)(SWAB_DATA) & 0x000000FF) << 24))

static u8 *amigaMemKick = 0;
static const int KickMemStart = 0x00F80000;
static const int KickMemSize = 0x080000;
static const u32 KickMemMask = KickMemSize-1;

u8 *amigaMemChip = 0;
static const int ChipMemStart = 0x00000000;
u32 amigaMemChipSize = 0;
static u32 memChipMask = 0;

const AmigaAddrBank *amigaMemBanks[65536];
u8 *amigaMemBaseAddr[65536];

static u32 dummy_lget(u32) { return 0xFFFFFFFF; }
static u32 dummy_wget(u32) { return 0xFFFF; }
static u32 dummy_bget(u32) { return 0xFF; }

static void dummy_lput(u32, u32) { }
static void dummy_wput(u32, u32) { }
static void dummy_bput(u32, u32) {}

static u32 chipmem_lget(u32 addr) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	u32 *m = (u32 *)(amigaMemChip + addr);
	return amigaMemDoGetLong(m);
}

static u32 chipmem_wget(u32 addr) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	return *(u16 *)(amigaMemChip + addr);
}

static u32 chipmem_bget(u32 addr) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	return amigaMemChip[addr];
}

static void chipmem_lput(u32 addr, u32 l) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	u32 *m = (u32 *)(amigaMemChip + addr);
	amigaMemDoPutLong (m, l);
}

static void chipmem_wput(u32 addr, u32 w) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	*(u16 *)(amigaMemChip + addr) = w;
}

static void chipmem_bput(u32 addr, u32 b) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	amigaMemChip[addr] = b;
}

u8 *amigaMemChipXlate(u32 addr, u32 size) {
	addr -= ChipMemStart & memChipMask;
	addr &= memChipMask;
	if (addr+size > amigaMemChipSize) {
		static int count = 0;
		if (!count) {
			count++;
			qDebug("Warning: Bad playfield pointer.");
		}
		return 0;
	}
	return amigaMemChip + addr;
}

static u32 kickmem_lget(u32 addr) {
	addr -= KickMemStart & KickMemMask;
	addr &= KickMemMask;
	u32 *m = (u32 *)(amigaMemKick + addr);
	return amigaMemDoGetLong(m);
}

static u32 kickmem_wget(u32 addr) {
	addr -= KickMemStart & KickMemMask;
	addr &= KickMemMask;
	return *(u16 *)(amigaMemKick + addr);
}

static u32 kickmem_bget(u32 addr) {
	addr -= KickMemStart & KickMemMask;
	addr &= KickMemMask;
	return amigaMemKick[addr];
}

static AmigaAddrBank dummy_bank = {
	dummy_lget, dummy_wget, dummy_bget,
	dummy_lput, dummy_wput, dummy_bput,
	0
};

AmigaAddrBank amigaMemChipBank = {
	chipmem_lget, chipmem_wget, chipmem_bget,
	chipmem_lput, chipmem_wput, chipmem_bput,
	0
};

AmigaAddrBank amigaMemKickBank = {
	kickmem_lget, kickmem_wget, kickmem_bget,
	dummy_lput, dummy_wput, dummy_bput,
	0
};

static bool checkKickstartChecksum() {
	u32 cksum = 0;
	u32 prevck = 0;
	for (int i = 0; i < KickMemSize; i += 4) {
		u32 data = SWAP_L(*(u32 *)&amigaMemKick[i]);
		cksum += data;
		if (cksum < prevck)
			cksum++;
		prevck = cksum;
	}
	if (cksum != 0xFFFFFFFFul) {
		qDebug("Kickstart checksum incorrect. You probably have a corrupted ROM image.");
		return false;
	}
	return true;
}

bool amigaLoadKickstart(const QString &fileName) {
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	int n = file.read((char *)amigaMemKick, KickMemSize);
	if (n == KickMemSize / 2) {
		memcpy (amigaMemKick + KickMemSize / 2, amigaMemKick, n);
	} else if (n != KickMemSize) {
		qDebug("Error while reading Kickstart.");
		return false;
	}
	if (n >= 262144)
		checkKickstartChecksum();
	for (uint i = 0; i < KickMemSize; i+=2)
		qSwap(amigaMemKick[i], amigaMemKick[i+1]);
	return true;
}

void amigaMemReset() {
	for (int i = 0; i < 65536; i++)
		amigaMemBankPut(i << 16, &dummy_bank, 0);

	/* Map the chipmem into all of the lower 8MB */
	int i = amigaMemChipSize > 0x200000 ? (amigaMemChipSize >> 16) : 32;
	amigaMemBanksMap(&amigaMemChipBank, 0x00, i, amigaMemChipSize);

	int custom_start = 0xC0;
	amigaMemBanksMap(&amigaCustomBank, custom_start, 0xE0 - custom_start, 0);
	amigaMemBanksMap(&amigaCiaBank, 0xA0, 32, 0);
	amigaMemBanksMap(&amigaClockBank, 0xDC, 1, 0);

	custom_start = amigaMemChipSize >> 16;
	if (custom_start < 0x20)
		custom_start = 0x20;
	amigaMemBanksMap(&dummy_bank, custom_start, 0xA0 - custom_start, 0);
	amigaMemBanksMap(&amigaMemKickBank, 0xF8, 8, 0);
}

void amigaMemInit() {
	amigaMemKick = new u8[KickMemSize];
	amigaMemKickBank.baseAddr = amigaMemKick;

	amigaMemChip = new u8[amigaMemChipSize];
	amigaMemChipBank.baseAddr = amigaMemChip;
	memset(amigaMemChip, 0, amigaMemChipSize);

	memChipMask = amigaMemChipSize - 1;

	amigaMemReset();
}

void amigaMemShutdown() {
	delete[] amigaMemKick;
	delete[] amigaMemChip;
	amigaMemKick = 0;
	amigaMemChip = 0;
}

void amigaMemBanksMap(AmigaAddrBank *bank, int start, int size, int realSize) {
	u32 realStart = start;
	if (!realSize)
		realSize = size << 16;
	Q_ASSERT((size << 16) >= realSize);
	int real_left = 0;
	for (int bnr = start; bnr < start + size; bnr++) {
		if (!real_left) {
			realStart = bnr;
			real_left = realSize >> 16;
		}
		amigaMemBankPut(bnr << 16, bank, realStart << 16);
		real_left--;
	}
}

void amigaMemSl() {
	emsl.begin("mem");
	// TODO kickstart configuration check
	u32 chipSize = amigaMemChipSize;
	emsl.var("chipSize", chipSize);
	if (!emsl.save) {
		if (chipSize != amigaMemChipSize) {
			emsl.error = "Different Chip Size configured";
		} else {
			emsl.array("chip", amigaMemChip, amigaMemChipSize);
		}
	} else {
		emsl.array("chip", amigaMemChip, amigaMemChipSize);
	}
	emsl.end();
}
