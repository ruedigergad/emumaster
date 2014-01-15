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

#ifndef AMIGACIA_H
#define AMIGACIA_H

#include <base/emu.h>
#include <QList>

class Cia8520 {
public:
	enum Reg {
		PRA_Reg = 0,
		PRB_Reg,
		DDRA_Reg,
		DDRB_Reg,
		TALO_Reg,
		TAHI_Reg,
		TBLO_Reg,
		TBHI_Reg,
		TOD0_Reg,
		TOD1_Reg,
		TOD2_Reg,
		TOD3_Reg,
		SDR_Reg,
		ICR_Reg,
		CRA_Reg,
		CRB_Reg
	};

	void reset();
	void sl(int i);

	u8 read(u32 addr);
	virtual u8 readPra() = 0;
	virtual u8 readPrb() = 0;

	void write(u32 addr, u8 data);
	virtual void writePra(u8 data) = 0;
	virtual void writePrb(u8 data) = 0;

	void updateTimers(u32 clocks);
	virtual void rethink() = 0;
	void incrementTod();
	u32 taPassed() const;
	u32 tbPassed() const;

	u32 pra;
	u32 prb;
	u32 ddra;
	u32 ddrb;
	u32 ta;
	u32 tb;
	u32 la;
	u32 lb;

	u32 tod;
	u32 todLatch;
	bool todLatched;
	bool todOn;

	u32 sdr;
	u32 icr;
	u32 cra;
	u32 crb;
	u32 imask;
	u32 alarm;
};

class AmigaCiaA : public Cia8520 {
public:
	u8 readPra();
	u8 readPrb();
	void writePra(u8 data);
	void writePrb(u8 data);
	void rethink();
};

class AmigaCiaB : public Cia8520 {
public:
	u8 readPra();
	u8 readPrb();
	void writePra(u8 data);
	void writePrb(u8 data);
	void rethink();
};

extern AmigaCiaA amigaCiaA;
extern AmigaCiaB amigaCiaB;
extern QList<u8> amigaKeys;

extern void amigaCiaReset();
extern void amigaCiaVSyncHandler();
extern void amigaCiaHSyncHandler();
extern void amigaCiaHandler();
extern void amigaCiaDiskIndex();
extern void amigaCiaRethink();
extern void amigaRecordKey(int hostKey, bool down);
extern void amigaCiaSl();

#endif // AMIGACIA_H
