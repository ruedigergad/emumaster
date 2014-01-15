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

#include "spu_null.h"
#include "decode_xa.h"
#include "spu_registers.h"
#include <QDataStream>

PsxSpuNull psxSpuNull;

static u16 regArea[10000];
static u16 spuMem[256*1024];

static u16 spuCtrl, spuStat, spuIrq=0;             // some vars to store psx reg infos
static u32 spuAddr = 0xffffffff;                     // address into spu mem

static void spuNullWriteRegister(u32 reg, u16 data) {
	reg &= 0xfff;
	regArea[(reg-0xc00)>>1] = data;
	if (reg >= 0x0c00 && reg < 0x0d80)
		return;
	switch (reg) {
	case H_SPUaddr:
		spuAddr = data << 3;
		break;
	case H_SPUdata:
		spuMem[spuAddr>>1] = data;
		spuAddr+=2;
		if (spuAddr>0x7ffff)
			spuAddr = 0;
		break;
	case H_SPUctrl:
		spuCtrl = data;
		break;
	case H_SPUstat:
		spuStat = data & 0xf800;
		break;
	case H_SPUirqAddr:
		spuIrq = data;
		break;
	}
}

static u16 spuNullReadRegister(u32 reg) {
	reg &= 0xfff;
	if (reg >= 0x0c00 && reg < 0x0d80) {
		switch (reg & 0x0f) {
		case 12: {
			static u16 adsr_dummy_vol = 0;
			adsr_dummy_vol = !adsr_dummy_vol;
			return adsr_dummy_vol;
		}
		case 14:
			return 0;
		}
	}
	switch (reg) {
	case H_SPUctrl:
		return spuCtrl;
	case H_SPUstat:
		return spuStat;
	case H_SPUaddr:
		return spuAddr >> 3;
	case H_SPUdata: {
		u16 s = spuMem[spuAddr >> 1];
		spuAddr += 2;
		if (spuAddr>0x7ffff)
			spuAddr=0;
		return s;
	}
	case H_SPUirqAddr:
		return spuIrq;
	}
	return regArea[(reg-0xc00)>>1];
}

static u16 spuNullReadDMA() {
	u16 s = spuMem[spuAddr >> 1];
	spuAddr += 2;
	if (spuAddr > 0x7ffff)
		spuAddr = 0;
	return s;
}

static void spuNullWriteDMA(u16 val) {
	spuMem[spuAddr >> 1] = val;
	spuAddr += 2;
	if (spuAddr > 0x7ffff)
		spuAddr = 0;
}

static void spuNullWriteDMAMem(u16 *pusPSXMem, int iSize) {
	for (int i = 0; i < iSize; i++) {
		spuMem[spuAddr >> 1] = *pusPSXMem++;
		spuAddr += 2;
		if (spuAddr > 0x7ffff)
			spuAddr = 0;
	}
}

static void spuNullReadDMAMem(u16 *pusPSXMem, int iSize) {
	for (int i = 0; i < iSize; i++) {
		*pusPSXMem++ = spuMem[spuAddr>>1];
		spuAddr += 2;
		if (spuAddr > 0x7ffff)
			spuAddr = 0;
	}
}

static void spuNullPlayADPCMchannel(xa_decode_t *) {
}
static void spuNullPlayCDDAchannel(s16 *, int) {
}

int PsxSpuNull::fillBuffer(char *stream, int size) {
	Q_UNUSED(stream)
	Q_UNUSED(size)
	return 0;
}

bool PsxSpuNull::init() {
	SPU_writeRegister		= spuNullWriteRegister;
	SPU_readRegister		= spuNullReadRegister;
	SPU_writeDMA			= spuNullWriteDMA;
	SPU_readDMA				= spuNullReadDMA;
	SPU_writeDMAMem			= spuNullWriteDMAMem;
	SPU_readDMAMem			= spuNullReadDMAMem;
	SPU_playADPCMchannel	= spuNullPlayADPCMchannel;
	SPU_playCDDAchannel		= spuNullPlayCDDAchannel;
	return true;
}

void PsxSpuNull::sl() {
	emsl.begin("spu");
	emsl.array("regArea", regArea, sizeof(regArea));
	emsl.array("mem", spuMem, sizeof(spuMem));
	emsl.var("addr", spuAddr);
	if (!emsl.save) { \
		for (int i = 0; i < 0x100; i++) { \
			if (i != H_SPUon1-0xc00 && i != H_SPUon2-0xc00) \
				spuNullWriteRegister(0x1f801c00+i*2,regArea[i]); \
		} \
		spuNullWriteRegister(H_SPUon1,regArea[(H_SPUon1-0xc00)/2]); \
		spuNullWriteRegister(H_SPUon2,regArea[(H_SPUon2-0xc00)/2]); \
	} \
	emsl.end();
}
