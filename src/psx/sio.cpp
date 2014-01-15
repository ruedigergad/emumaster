/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#include "sio.h"
#include "pad.h"
#include "cpu.h"
#include "mem.h"
#include <QDataStream>
#include <QTextCodec>

// Status Flags
#define TX_RDY		0x0001
#define RX_RDY		0x0002
#define TX_EMPTY	0x0004
#define PARITY_ERR	0x0008
#define RX_OVERRUN	0x0010
#define FRAMING_ERR	0x0020
#define SYNC_DETECT	0x0040
#define DSR			0x0080
#define CTS			0x0100
#define IRQ			0x0200

// Control Flags
#define TX_PERM		0x0001
#define DTR			0x0002
#define RX_PERM		0x0004
#define BREAK		0x0008
#define RESET_ERR	0x0010
#define RTS			0x0020
#define SIO_RESET	0x0040

// *** FOR WORKS ON PADS AND MEMORY CARDS *****

PsxMcd psxMcd1;
PsxMcd psxMcd2;

static u8 buf[256];
static u8 cardh1[4] = { 0xff, 0x08, 0x5a, 0x5d };
static u8 cardh2[4] = { 0xff, 0x08, 0x5a, 0x5d };

// Transfer Ready and the Buffer is Empty
static u16 StatReg = TX_RDY | TX_EMPTY;
static u16 ModeReg;
static u16 CtrlReg;
static u16 BaudReg;

static u32 bufcount;
static u32 parp;
static u32 mcdst, rdwr;
static u8 adrH, adrL;
static u32 padst;

#define SIO_INT(eCycle) { \
	if (!Config.Sio) { \
		psxRegs.interrupt |= (1 << PSXINT_SIO); \
		psxRegs.intCycle[PSXINT_SIO].cycle = eCycle; \
		psxRegs.intCycle[PSXINT_SIO].sCycle = psxRegs.cycle; \
		new_dyna_set_event(PSXINT_SIO, eCycle); \
	} \
}

// clk cycle byte
// 4us * 8bits = (PSXCLK / 1000000) * 32; (linuzappz)
// TODO: add SioModePrescaler and BaudReg
#define SIO_CYCLES		535

void sioWrite8(u8 data) {
#ifdef PAD_LOG
	PAD_LOG("sio write8 %x\n", value);
#endif
	switch (padst) {
	case 1: SIO_INT(SIO_CYCLES);
		if ((data & 0x40) == 0x40) {
			padst = 2; parp = 1;
			switch (CtrlReg & 0x2002) {
			case 0x0002:
				buf[parp] = pad1Poll(data);
				break;
			case 0x2002:
				buf[parp] = pad2Poll(data);
				break;
			}

			if (!(buf[parp] & 0x0f)) {
				bufcount = 2 + 32;
			} else {
				bufcount = 2 + (buf[parp] & 0x0f) * 2;
			}
			if (buf[parp] == 0x41) {
				switch (data) {
					case 0x43:
						buf[1] = 0x43;
						break;
					case 0x45:
						buf[1] = 0xf3;
						break;
				}
			}
		}
		else padst = 0;
		return;
	case 2:
		parp++;
/*		if (buf[1] == 0x45) {
		buf[parp] = 0;
		SIO_INT(SIO_CYCLES);
		return;
		}*/
		switch (CtrlReg & 0x2002) {
		case 0x0002: buf[parp] = pad1Poll(data); break;
		case 0x2002: buf[parp] = pad2Poll(data); break;
		}

		if (parp == bufcount) { padst = 0; return; }
		SIO_INT(SIO_CYCLES);
		return;
	}

	switch (mcdst) {
	case 1:
		SIO_INT(SIO_CYCLES);
		if (rdwr) { parp++; return; }
		parp = 1;
		switch (data) {
			case 0x52: rdwr = 1; break;
			case 0x57: rdwr = 2; break;
			default: mcdst = 0;
		}
		return;
	case 2: // address H
		SIO_INT(SIO_CYCLES);
		adrH = data;
		*buf = 0;
		parp = 0;
		bufcount = 1;
		mcdst = 3;
		return;
	case 3: // address L
		SIO_INT(SIO_CYCLES);
		adrL = data;
		*buf = adrH;
		parp = 0;
		bufcount = 1;
		mcdst = 4;
		return;
	case 4:
		SIO_INT(SIO_CYCLES);
		parp = 0;
		switch (rdwr) {
			case 1: // read
				buf[0] = 0x5c;
				buf[1] = 0x5d;
				buf[2] = adrH;
				buf[3] = adrL;
				switch (CtrlReg & 0x2002) {
					case 0x0002:
						memcpy(&buf[4], psxMcd1.memory + (adrL | (adrH << 8)) * 128, 128);
						break;
					case 0x2002:
						memcpy(&buf[4], psxMcd2.memory + (adrL | (adrH << 8)) * 128, 128);
						break;
				}
				{
				char xorVal = 0;
				int i;
				for (i = 2; i < 128 + 4; i++)
					xorVal ^= buf[i];
				buf[132] = xorVal;
				}
				buf[133] = 0x47;
				bufcount = 133;
				break;
			case 2: // write
				buf[0] = adrL;
				buf[1] = data;
				buf[129] = 0x5c;
				buf[130] = 0x5d;
				buf[131] = 0x47;
				bufcount = 131;
				break;
		}
		mcdst = 5;
		return;
	case 5:
		parp++;
		if ((rdwr == 1 && parp == 132) ||
			(rdwr == 2 && parp == 129)) {
			// clear "new card" flags
			if (CtrlReg & 0x2000)
				cardh2[1] &= ~8;
			else
				cardh1[1] &= ~8;
		}
		if (rdwr == 2) {
			if (parp < 128) buf[parp + 1] = data;
		}
		SIO_INT(SIO_CYCLES);
		return;
	}

	switch (data) {
	case 0x01: // start pad
		StatReg |= RX_RDY;		// Transfer is Ready

		switch (CtrlReg & 0x2002) {
		case 0x0002: buf[0] = pad1StartPoll(1); break;
		case 0x2002: buf[0] = pad2StartPoll(2); break;
		}
		bufcount = 2;
		parp = 0;
		padst = 1;
		SIO_INT(SIO_CYCLES);
		return;
	case 0x81: // start memcard
		if (CtrlReg & 0x2000) {
			if (!psxMcd2.isEnabled())
				goto no_device;
			memcpy(buf, cardh2, 4);
		} else {
			if (!psxMcd1.isEnabled())
				goto no_device;
			memcpy(buf, cardh1, 4);
		}
		StatReg |= RX_RDY;
		parp = 0;
		bufcount = 3;
		mcdst = 1;
		rdwr = 0;
		SIO_INT(SIO_CYCLES);
		return;
	default:
	no_device:
		StatReg |= RX_RDY;
		buf[0] = 0xff;
		parp = 0;
		bufcount = 0;
		return;
	}
}

void sioWriteStat16(u16 data) {
	Q_UNUSED(data)
}

void sioWriteMode16(u16 data) {
	ModeReg = data;
}

void sioWriteCtrl16(u16 data) {
	CtrlReg = data & ~RESET_ERR;
	if (data & RESET_ERR) StatReg &= ~IRQ;
	if ((CtrlReg & SIO_RESET) || !(CtrlReg & DTR)) {
		padst = 0; mcdst = 0; parp = 0;
		StatReg = TX_RDY | TX_EMPTY;
		psxRegs.interrupt &= ~(1 << PSXINT_SIO);
	}
}

void sioWriteBaud16(u16 data) {
	BaudReg = data;
}

u8 sioRead8() {
	u8 ret = 0;

	if ((StatReg & RX_RDY)/* && (CtrlReg & RX_PERM)*/) {
//		StatReg &= ~RX_OVERRUN;
		ret = buf[parp];
		if (parp == bufcount) {
			StatReg &= ~RX_RDY;		// Receive is not Ready now
			if (mcdst == 5) {
				mcdst = 0;
				if (rdwr == 2) {
					switch (CtrlReg & 0x2002) {
					case 0x0002:
						memcpy(psxMcd1.memory + (adrL | (adrH << 8)) * 128, &buf[1], 128);
						psxMcd1.write((adrL | (adrH << 8)) * 128, 128);
						break;
					case 0x2002:
						memcpy(psxMcd2.memory + (adrL | (adrH << 8)) * 128, &buf[1], 128);
						psxMcd2.write((adrL | (adrH << 8)) * 128, 128);
						break;
					}
				}
			}
			if (padst == 2)
				padst = 0;
			if (mcdst == 1) {
				mcdst = 2;
				StatReg|= RX_RDY;
			}
		}
	}
#ifdef PAD_LOG
	PAD_LOG("sio read8 ;ret = %x\n", ret);
#endif
	return ret;
}

u16 sioReadStat16() {
	return StatReg;
}

u16 sioReadMode16() {
	return ModeReg;
}

u16 sioReadCtrl16() {
	return CtrlReg;
}

u16 sioReadBaud16() {
	return BaudReg;
}

void sioInterrupt() {
#ifdef PAD_LOG
	PAD_LOG("Sio Interrupt (CP0.Status = %x)\n", psxRegs.CP0.n.Status);
#endif
	if (!(StatReg & IRQ)) {
		StatReg |= IRQ;
		psxHu32ref(0x1070) |= SWAPu32(0x80);
	}
}

PsxMcd::PsxMcd() {
	m_enabled = false;
}

void PsxMcd::init(const QString &path) {
	cardh1[1] |= 8; // mark as new
	cardh2[1] |= 8;

	m_file.setFileName(path);
	if (!m_file.open(QIODevice::ReadWrite|QIODevice::Unbuffered))
		return;
	if (m_file.size() < Size) {
		m_file.write("MC");
		m_file.write(QByteArray(125, '\x00'));
		m_file.write("\x0E");
		for (int i = 0; i < 15; i++) {
			static const char block[] = { 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
			m_file.write(block, 10);
			m_file.write(QByteArray(117, '\x00'));
			m_file.write("\xA0");
		}
		for (int i = 0; i < 20; i++) {
			static const char block[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
			m_file.write(block, 10);
			m_file.write(QByteArray(118, '\x00'));
		}
		m_file.write(QByteArray(Size - m_file.size(), '\x00'));
		m_file.seek(0);
	}
	m_file.read((char *)memory, Size);
	m_enabled = true;
}

void PsxMcd::write(u32 address, int size) {
	Q_ASSERT(size >= 0);
	Q_ASSERT(address + size < Size);
	if (m_enabled) {
		m_file.seek(address);
		m_file.write((const char *)(memory + address), size);
	}
}

PsxMcdBlockInfo PsxMcd::blockInfo(int block) {
	Q_ASSERT(block >= 0 && block < NumBlocks);

	PsxMcdBlockInfo info;
	if (!m_enabled)
		return info;

	u8 *ptr = memory + block * BlockSize + 2;
	int iconCount = *ptr & 0x03;

	u16 clut[16];
	ptr = memory + block * BlockSize + 0x60; // icon palette data
	for (int i = 0; i < 16; i++) {
		clut[i] = *((u16 *)ptr);
		ptr += 2;
	}
	// TODO check format
	QImage icon(16, 16, QImage::Format_RGB16);
	for (int i = 0; i < iconCount; i++) {
		ptr = memory + block * BlockSize + 128 + 128 * i; // icon data
		u16 *iconData = (u16 *)icon.bits();
		for (int x = 0; x < 16 * 16; x++) {
			iconData[x++] = clut[*ptr & 0x0F];
			iconData[x] = clut[*ptr >> 4];
			ptr++;
		}
		info.icons.append(icon);
	}
	ptr = memory + block * 128;
	info.flags = *ptr;
	ptr += 10;
	info.id = QString::fromLocal8Bit((const char *)ptr, 12);
	ptr += 12;
	info.name = QString::fromLocal8Bit((const char *)ptr, 16);

	ptr = memory + block * BlockSize + 4;
	QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
	if (!codec)
		return info;
	info.title = codec->toUnicode((const char *)ptr, 48*2);
	return info;
}

void psxSioSl() {
	emsl.begin("sio");
	emsl.array("buf", buf, sizeof(buf));
	emsl.var("statReg", StatReg);
	emsl.var("modeReg", ModeReg);
	emsl.var("ctrlreg", CtrlReg);
	emsl.var("baudReg", BaudReg);
	emsl.var("bufcount", bufcount);
	emsl.var("parp", parp);
	emsl.var("mcdst", mcdst);
	emsl.var("rdwr", rdwr);
	emsl.var("adrH", adrH);
	emsl.var("adrL", adrL);
	emsl.var("padst", padst);
	emsl.end();
}
