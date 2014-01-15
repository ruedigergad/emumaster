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

#ifndef PSXSPU_H
#define PSXSPU_H

#include "common.h"
#include "decode_xa.h"

typedef void (* SPUwriteRegister)(u32, u16);
typedef u16 (* SPUreadRegister)(u32);
typedef void (* SPUwriteDMA)(u16);
typedef u16 (* SPUreadDMA)();
typedef void (* SPUwriteDMAMem)(u16 *, int);
typedef void (* SPUreadDMAMem)(u16 *, int);
typedef void (* SPUplayADPCMchannel)(xa_decode_t *);
typedef void (* SPUplayCDDAchannel)(s16 *, int);

#if defined(__cplusplus)

class PsxSpu {
public:
	virtual bool init() = 0;
	virtual void shutdown();

	virtual int fillBuffer(char *stream, int size) = 0;
	virtual void setEnabled(bool on);

	virtual void sl() = 0;
};

extern PsxSpu *psxSpu;

extern "C" {
#endif

extern SPUwriteRegister    SPU_writeRegister;
extern SPUreadRegister     SPU_readRegister;
extern SPUwriteDMA         SPU_writeDMA;
extern SPUreadDMA          SPU_readDMA;
extern SPUwriteDMAMem      SPU_writeDMAMem;
extern SPUreadDMAMem       SPU_readDMAMem;
extern SPUplayADPCMchannel SPU_playADPCMchannel;
extern SPUplayCDDAchannel  SPU_playCDDAchannel;

extern void SPUirq();

#if defined(__cplusplus)
}
#endif

#endif // PSXSPU_H
