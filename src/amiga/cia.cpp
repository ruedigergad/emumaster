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

#include "events.h"
#include "mem.h"
#include "custom.h"
#include "cia.h"
#include "disk.h"
#include "cpu.h"

#define DIV10 (5*CYCLE_UNIT) /* Yes, a bad identifier. */
// TODO use ddra and ddrb
/* battclock stuff */
#define RTC_D_ADJ	  8
#define RTC_D_IRQ	  4
#define RTC_D_BUSY	 2
#define RTC_D_HOLD	 1
#define RTC_E_t1	   8
#define RTC_E_t0	   4
#define RTC_E_INTR	 2
#define RTC_E_MASK	 1
#define RTC_F_TEST	 8
#define RTC_F_24_12	4
#define RTC_F_STOP	 2
#define RTC_F_RSET	 1

AmigaCiaB amigaCiaB;
AmigaCiaA amigaCiaA;
QList<u8> amigaKeys;

static int div10;
static u32 oldCycles;

static u32 rtcD = RTC_D_ADJ + RTC_D_HOLD;
static u32 rtcE = 0;
static u32 rtcF = RTC_F_24_12;

static __inline__ void setclr(u32 *p, u32 val) {
	if (val & 0x80) {
		*p |= val & 0x7F;
	} else {
		*p &= ~val;
	}
}

/* Called to advance all CIA timers to the current time. This expects that
   one of the timer values will be modified, and calcAllTimers() will be called
   in the same cycle.  */

static inline void updateAllTimers() {
	u32 ccount = (amigaCycles - oldCycles + div10);
	u32 clocks = ccount / DIV10;
	div10 = ccount % DIV10;
	amigaCiaA.updateTimers(clocks);
	amigaCiaB.updateTimers(clocks);
}

inline void Cia8520::updateTimers(u32 clocks) {
	bool taOverflowed = false;
	bool tbOverflowed = false;

	if ((cra & 0x21) == 0x01) {
		Q_ASSERT(ta+1 >= clocks);
		if (ta+1 == clocks) {
			taOverflowed = true;
			if ((crb & 0x61) == 0x41) {
				if (tb-- == 0)
					tbOverflowed = true;
			}
		}
		ta -= clocks;
	}
	if ((crb & 0x61) == 0x01) {
		Q_ASSERT(tb+1 >= clocks);
		if (tb+1 == clocks)
			tbOverflowed = true;
		tb -= clocks;
	}
	if (taOverflowed) {
		icr |= 0x01;
		rethink();
		ta = la;
		if (cra & 0x8)
			cra &= ~1;
	}
	if (tbOverflowed) {
		icr |= 0x02;
		rethink();
		tb = lb;
		if (crb & 0x8)
			crb &= ~1;
	}
}

/* Call this only after updateAllTimers() has been called in the same cycle.  */

static inline void calcAllTimers() {
	s32 ciaatimea = -1, ciaatimeb = -1, ciabtimea = -1, ciabtimeb = -1;

	oldCycles = amigaCycles;
	if ((amigaCiaA.cra & 0x21) == 0x01)
		ciabtimea = (DIV10 - div10) + DIV10 * amigaCiaA.ta;
	if ((amigaCiaA.crb & 0x61) == 0x01)
		ciabtimeb = (DIV10 - div10) + DIV10 * amigaCiaA.tb;
	if ((amigaCiaB.cra & 0x21) == 0x01)
		ciabtimea = (DIV10 - div10) + DIV10 * amigaCiaB.ta;
	if ((amigaCiaB.crb & 0x61) == 0x01)
		ciabtimeb = (DIV10 - div10) + DIV10 * amigaCiaB.tb;
	amigaEvents[AmigaEventCia].active = (ciaatimea != -1 || ciaatimeb != -1 || ciabtimea != -1 || ciabtimeb != -1);
	if (amigaEvents[AmigaEventCia].active) {
		u32 ciatime = ~0L;
		if (ciaatimea != -1) ciatime = ciaatimea;
		if (ciaatimeb != -1 && ciaatimeb < ciatime) ciatime = ciaatimeb;
		if (ciabtimea != -1 && ciabtimea < ciatime) ciatime = ciabtimea;
		if (ciabtimeb != -1 && ciabtimeb < ciatime) ciatime = ciabtimeb;
		amigaEvents[AmigaEventCia].time = ciatime + amigaCycles;
	}
	amigaEventsSchedule();
}

inline void Cia8520::reset() {
	pra = prb = ddra = ddrb = 0;
	ta = tb = la = lb = 0xFFFF;
	tod = todLatch = 0;
	todLatched = todOn = false;
	sdr = icr = 0x00;
	cra = crb = 0x04;
	imask = 0;
}

void amigaCiaReset() {
	amigaCiaA.reset();
	amigaCiaB.reset();
	amigaCiaA.pra = 0x03;
	amigaCiaB.pra = 0x8C;
	div10 = 0;

	amigaKeys.clear();
	amigaKeys.append(0xFB);
	amigaKeys.append(0xFD);

	calcAllTimers();
	int i = amigaMemChipSize > 0x200000 ? amigaMemChipSize >> 16 : 32;
	amigaMemBanksMap(&amigaMemKickBank, 0, i, 0x80000);
}

inline void AmigaCiaA::rethink() {
	if (imask & icr) {
		icr |= 0x80;
		INTREQ_0(0x8008);
	} else {
		icr &= 0x7F;
	}
}

inline void AmigaCiaB::rethink() {
	if (imask & icr) {
		icr |= 0x80;
		INTREQ_0(0xA000);
	} else {
		icr &= 0x7F;
	}
}

void amigaCiaRethink() {
	amigaCiaA.rethink();
	amigaCiaB.rethink();
}

/* Figure out how many CIA timer cycles have passed for each timer since the
   last call of CIA_calctimers.  */

inline u32 Cia8520::taPassed() const {
	if ((cra & 0x21) == 0x01) {
		u32 ccount = amigaCycles - oldCycles + div10;
		u32 clocks = ccount / DIV10;
		Q_ASSERT((ta+1) >= clocks);
		return clocks;
	}
	return 0;
}

inline u32 Cia8520::tbPassed() const {
	if ((crb & 0x61) == 0x01) {
		u32 ccount = amigaCycles - oldCycles + div10;
		u32 clocks = ccount / DIV10;
		Q_ASSERT((tb+1) >= clocks);
		return clocks;
	}
	return 0;
}

void amigaCiaHandler() {
	updateAllTimers();
	calcAllTimers();
}

void amigaCiaDiskIndex () {
	amigaCiaB.icr |= 0x10;
	amigaCiaB.rethink();
}

inline void Cia8520::incrementTod() {
	if (todOn)
		tod = (tod+1) & 0xFFFFFF;
	if (tod == alarm) {
		icr |= 0x04;
		rethink();
	}
}

void amigaCiaHSyncHandler() {
	amigaCiaB.incrementTod();
}

void amigaCiaVSyncHandler() {
	amigaCiaA.incrementTod();
	if (!amigaKeys.isEmpty()) {
		amigaCiaA.sdr = ~amigaKeys.takeFirst();
		amigaCiaA.icr |= 0x08;
		amigaCiaA.rethink();
	}
}

inline u8 Cia8520::read(u32 addr) {
	u8 data;
	switch (addr & 0xF) {
	case PRA_Reg: data = readPra(); break;
	case PRB_Reg: data = readPrb(); break;
	case DDRA_Reg: data = ddra; break;
	case DDRB_Reg: data = ddrb; break;
	case TALO_Reg: data = ((ta - taPassed()) >> 0) & 0xFF; break;
	case TAHI_Reg: data = ((ta - taPassed()) >> 8) & 0xFF; break;
	case TBLO_Reg: data = ((tb - tbPassed()) >> 0) & 0xFF; break;
	case TBHI_Reg: data = ((tb - tbPassed()) >> 8) & 0xFF; break;
	case TOD0_Reg:
		data = ((todLatched ? todLatch : tod) >> 0) & 0xFF;
		todLatched = false;
		break;
	case TOD1_Reg:
		data = ((todLatched ? todLatch : tod) >> 8) & 0xFF;
		break;
	case TOD2_Reg:
		todLatch = tod;
		todLatched = true;
		data = (todLatch >> 16) & 0xFF;
		break;
	case TOD3_Reg: data = 0; break;
	case SDR_Reg: data = sdr; break;
	case ICR_Reg:
		data = icr;
		icr = 0;
		rethink();
		break;
	case CRA_Reg: data = cra; break;
	case CRB_Reg: data = crb; break;
	}
	return data;
}

inline u8 AmigaCiaA::readPra() {
	u8 tmp = (amigaDiskStatus() & 0x3C);
	if (!(amigaInputPortButtons[0] & 1))
		tmp |= 0x40;
	if (!(amigaInputPortButtons[1] & 1))
		tmp |= 0x80;
	return tmp;
}

inline u8 AmigaCiaA::readPrb() {
	return 0xFF;
}

inline u8 AmigaCiaB::readPra() {
	/* Returning some 1 bits is necessary for Tie Break - otherwise its joystick
	   code won't work.  */
	return pra | 3;
}

inline u8 AmigaCiaB::readPrb() {
	return prb;
}

inline void Cia8520::write(u32 addr, u8 data) {
	switch (addr & 0xF) {
	case PRA_Reg: writePra(data); break;
	case PRB_Reg: writePrb(data); break;
	case DDRA_Reg: ddra = data; break;
	case DDRB_Reg: ddrb = data; break;
	case TALO_Reg:
		updateAllTimers();
		la = (la & 0xFF00) | data;
		calcAllTimers();
		break;
	case TAHI_Reg:
		updateAllTimers();
		la = (la & 0xFF) | (data << 8);
		if (!(cra & 1))
			ta = la;
		if (cra & 8) {
			ta = la;
			cra |= 1;
		}
		calcAllTimers();
		break;
	case TBLO_Reg:
		updateAllTimers();
		lb = (lb & 0xFF00) | data;
		calcAllTimers();
		break;
	case TBHI_Reg:
		updateAllTimers();
		lb = (lb & 0xFF) | (data << 8);
		if ((crb & 1) == 0)
			tb = lb;
		if (crb & 8) {
			tb = lb;
			crb |= 1;
		}
		calcAllTimers();
		break;
	case TOD0_Reg:
		if (crb & 0x80) {
			alarm = (alarm & ~0xFF) | data;
		} else {
			tod = (tod & ~0xFF) | data;
			todOn = true;
		}
		break;
	case TOD1_Reg:
		if (crb & 0x80) {
			alarm = (alarm & ~(0xFF << 8)) | (data << 8);
		} else {
			tod = (tod & ~(0xFF << 8)) | (data << 8);
			todOn = false;
		}
		break;
	case TOD2_Reg:
		if (crb & 0x80) {
			alarm = (alarm & ~(0xFF << 16)) | (data << 16);
		} else {
			tod = (tod & ~(0xFF << 16)) | (data << 16);
			todOn = false;
		}
		break;
	case TOD3_Reg: break;
	case SDR_Reg: sdr = data; break;
	case ICR_Reg: setclr(&imask, data); break; /* ??? call RethinkICR() ? */
	case CRA_Reg:
		updateAllTimers();
		cra = data;
		if (cra & 0x10) {
			cra &= ~0x10;
			ta = la;
		}
		calcAllTimers();
		break;
	case CRB_Reg:
		updateAllTimers();
		crb = data;
		if (crb & 0x10) {
			crb &= ~0x10;
			tb = lb;
		}
		calcAllTimers();
		break;
	}
}

inline void AmigaCiaA::writePra(u8 data) {
	bool oldovl = pra & 1;
	pra = (pra & ~0x3) | (data & 0x3);
//	TODO gui_ledstate &= ~1;
//	TODO gui_ledstate |= ((~pra & 2) >> 1);
//	TODO gui_data.powerled = ((~pra & 2) >> 1);

	if ((pra & 1) != oldovl) {
		int i = (amigaMemChipSize>>16) > 32 ? amigaMemChipSize >> 16 : 32;
		if (oldovl) {
			amigaMemBanksMap(&amigaMemChipBank, 0, i, amigaMemChipSize);
		} else {
			/* Is it OK to do this for more than 2M of chip? */
			amigaMemBanksMap(&amigaMemKickBank, 0, i, 0x80000);
		}
	}
}

inline void AmigaCiaA::writePrb(u8 data) {
	prb = data;
	icr |= 0x10;
}

inline void AmigaCiaB::writePra(u8 data) {
	pra = data;
}

inline void AmigaCiaB::writePrb(u8 data) {
	prb = data;
	amigaDiskSelect(data);
}

/* CIA memory access */

static u32 cia_lget (u32);
static u32 cia_wget (u32);
static u32 cia_bget (u32);
static void cia_lput (u32, u32);
static void cia_wput (u32, u32);
static void cia_bput (u32, u32);

AmigaAddrBank amigaCiaBank = {
	cia_lget, cia_wget, cia_bget,
	cia_lput, cia_wput, cia_bput,
	0
};

static void cia_wait() {
	if (!div10)
		return;
	amigaEventsHandle(DIV10 - div10 + CYCLE_UNIT);
	amigaCiaHandler();
}

u32 cia_bget (u32 addr) {
	int r = (addr & 0xF00) >> 8;
	cia_wait();
	u8 data;
	switch ((addr >> 12) & 3) {
	case 0: data = ((addr & 1) ? amigaCiaA.read(r) : amigaCiaB.read(r)); break;
	case 1: data = ((addr & 1) ? 0xFF : amigaCiaB.read(r)); break;
	case 2: data = ((addr & 1) ? amigaCiaA.read(r) : 0xFF); break;
	case 3: data = 0xFF; break;
	}
	return data;
}

u32 cia_wget (u32 addr) {
	int r = (addr & 0xF00) >> 8;
	cia_wait();
	u16 data;
	switch ((addr >> 12) & 3) {
	case 0: data = (amigaCiaB.read(r) << 8) | amigaCiaA.read(r); break;
	case 1:	data = (amigaCiaB.read(r) << 8) | (0xFF << 0); break;
	case 2:	data = (amigaCiaA.read(r) << 0) | (0xFF << 8); break;
	case 3: data = 0xFFFF; break;
	}
	return data;
}

u32 cia_lget(u32 addr) {
	
	u32 v = cia_wget(addr) << 16;
	v |= cia_wget(addr + 2);
	return v;
}

void cia_bput(u32 addr, u32 data) {
	int r = (addr & 0xF00) >> 8;
	cia_wait();
	if (!(addr & 0x2000))
		amigaCiaB.write(r, data);
	if (!(addr & 0x1000))
		amigaCiaA.write(r, data);
}

void cia_wput(u32 addr, u32 data) {
	int r = (addr & 0xF00) >> 8;
	cia_wait();
	if (!(addr & 0x2000))
		amigaCiaB.write(r, (data >> 8) & 0xFF);
	if (!(addr & 0x1000))
		amigaCiaA.write(r, (data >> 0) & 0xFF);
}

void cia_lput(u32 addr, u32 data) {
	cia_wput(addr, data >> 16);
	cia_wput(addr + 2, data & 0xFFFF);
}

/* battclock memory access */

static u32 clock_lget (u32);
static u32 clock_wget (u32);
static u32 clock_bget (u32);
static void clock_lput (u32, u32);
static void clock_wput (u32, u32);
static void clock_bput (u32, u32);

AmigaAddrBank amigaClockBank = {
	clock_lget, clock_wget, clock_bget,
	clock_lput, clock_wput, clock_bput,
	0
};

u32 clock_lget(u32 addr) {
	return clock_bget(addr + 3);
}

u32 clock_wget(u32 addr) {
	return clock_bget(addr + 1);
}

u32 clock_bget(u32 addr) {
	time_t t = time(0);
	struct tm *ct = localtime(&t);

	switch (addr & 0x3f) {
	case 0x03: return ct->tm_sec % 10;
	case 0x07: return ct->tm_sec / 10;
	case 0x0b: return ct->tm_min % 10;
	case 0x0f: return ct->tm_min / 10;
	case 0x13: return ct->tm_hour % 10;
	case 0x17: return ct->tm_hour / 10;
	case 0x1b: return ct->tm_mday % 10;
	case 0x1f: return ct->tm_mday / 10;
	case 0x23: return (ct->tm_mon+1) % 10;
	case 0x27: return (ct->tm_mon+1) / 10;
	case 0x2b: return ct->tm_year % 10;
	case 0x2f: return ct->tm_year / 10;

	case 0x33: return ct->tm_wday;  /*Hack by -=SR=- */
	case 0x37: return rtcD;
	case 0x3b: return rtcE;
	case 0x3f: return rtcF;
	}
	return 0;
}

void clock_lput(u32, u32) { }
void clock_wput (u32, u32) { }

void clock_bput(u32 addr, u32 data) {
	switch (addr & 0x3f) {
	case 0x37: rtcD = data; break;
	case 0x3b: rtcE = data; break;
	case 0x3f: rtcF = data; break;
	}
}

void Cia8520::sl(int i) {
	emsl.begin(QString("cia%1").arg(i));
	emsl.var("pra", pra);
	emsl.var("prb", prb);
	emsl.var("ddra", ddra);
	emsl.var("ddrb", ddrb);
	emsl.var("ta", ta);
	emsl.var("tb", tb);
	emsl.var("la", la);
	emsl.var("lb", lb);
	emsl.var("tod", tod);
	emsl.var("todLatch", todLatch);
	emsl.var("todLatched", todLatched);
	emsl.var("todOn", todOn);
	emsl.var("sdr", sdr);
	emsl.var("icr", icr);
	emsl.var("cra", cra);
	emsl.var("crb", crb);
	emsl.var("imask", imask);
	emsl.var("alarm", alarm);
	emsl.end();
}

void amigaCiaSl() {
	emsl.begin("cia cmn");
	emsl.var("div10", div10);
	emsl.var("oldCycles", oldCycles);
	emsl.var("rtcD", rtcD);
	emsl.var("rtcE", rtcE);
	emsl.var("rtcF", rtcF);
	emsl.end();

	amigaCiaA.sl(0);
	amigaCiaB.sl(1);
}
