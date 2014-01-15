/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "cheat.h"
#include "pico.h"

PicoCheat::PicoCheat() {
	m_addr = 0;
	m_data = 0;
	m_oldData = 0;
	m_active = false;
}

bool PicoCheat::parse(const QString &code) {
	if (code.size() < 8 || code.size() > 11)
		return false;

	/* Just assume 8 char long string to be Game Genie code */
	if (code.size() == 8)
		return decodeGenie(code);

	/* If it's 9 chars long and the 5th is a hyphen, we have a Game Genie code. */
	if (code.size() == 9 && code.at(4) == '-')
		return decodeGenie(code.left(4) + code.right(4));

	/* Otherwise, we assume it's a hex code.
	 * Find the colon so we know where address ends and data starts. If there's
	 * no colon, then we haven't a code at all! */
	int addrLen = code.indexOf(':');
	int dataLen = code.size() - addrLen - 1;
	if (addrLen <= 0 || addrLen > 6 || dataLen <= 0 || dataLen > 4)
		return false;

	/* Pad the address with zeros, then fill it with the value */
	QString hexCode(10, '0');
	int i, j;
	for (i = 0; i < (6-addrLen); i++)
		hexCode[i] = '0';
	for (j = 0; i < 6; i++, j++)
		hexCode[i] = code.at(j);

	/* Do the same for data */
	for (i = 6; i < (10-dataLen); i++)
		hexCode[i] = '0';
	int dataStart = addrLen + 1;
	for (j = 0; i < 10; i++, j++)
		hexCode[i] = code.at(dataStart + j);

	return decodeHex(hexCode);
}

static const char genieChars[] = "AaBbCcDdEeFfGgHhJjKkLlMmNnPpRrSsTtVvWwXxYyZz0O1I2233445566778899";

bool PicoCheat::decodeGenie(const QString &code) {
	int n[8];
	for (int i = 0; i < 8; i++) {
		const char *x = strchr(genieChars, code.at(i).toLatin1());
		if (!x)
			return false;
		n[i] = (x-genieChars) >> 1;
	}
	/* ____ ____ ____ ____ ____ ____ : ____ ____ ABCD E___ */
	m_data |= n[0] << 3;
	/* ____ ____ DE__ ____ ____ ____ : ____ ____ ____ _ABC */
	m_data |= n[1] >> 2;
	m_addr |= (n[1] & 3) << 14;
	/* ____ ____ __AB CDE_ ____ ____ : ____ ____ ____ ____ */
	m_addr |= n[2] << 9;
	/* BCDE ____ ____ ___A ____ ____ : ____ ____ ____ ____ */
	m_addr |= (n[3] & 0xF) << 20 | (n[3] >> 4) << 8;
	/* ____ ABCD ____ ____ ____ ____ : ___E ____ ____ ____ */
	m_data |= (n[4] & 1) << 12;
	m_addr |= (n[4] >> 1) << 16;
	/* ____ ____ ____ ____ ____ ____ : E___ ABCD ____ ____ */
	m_data |= (n[5] & 1) << 15 | (n[5] >> 1) << 8;
	/* ____ ____ ____ ____ CDE_ ____ : _AB_ ____ ____ ____ */
	m_data |= (n[6] >> 3) << 13;
	m_addr |= (n[6] & 7) << 5;
	/* ____ ____ ____ ____ ___A BCDE : ____ ____ ____ ____ */
	m_addr |= n[7];

	m_addr &= ~1;
	return true;
}

static const char hexChars[] = "00112233445566778899AaBbCcDdEeFf";

/**
	This is for "012345:ABCD" type codes. You're more likely to find Genie
	codes circulating around, but there's a chance you could come on to one
	of these.
*/
bool PicoCheat::decodeHex(const QString &code) {
	/* 6 digits for address */
	for (int i = 0; i < 6; i++) {
		const char *x = strchr(hexChars, code.at(i).toLatin1());
		if (!x)
			return false;
		m_addr = (m_addr << 4) | ((x-hexChars) >> 1);
	}
	/* 4 digits for data */
	for (int i = 6; i < 10; ++i) {
		const char *x = strchr(hexChars, code.at(i).toLatin1());
		if (!x)
			return false;
		m_data = (m_data << 4) | ((x-hexChars) >> 1);
	}
	m_addr &= ~1;
	return true;
}

bool PicoCheat::active() const {
	return m_active;
}

void PicoCheat::setActive(bool active) {
	if (m_active == active)
		return;
	m_active = active;

	if (!active) {
		write(m_oldData);
	} else {
		m_oldData = PicoRead16(m_addr);
		write(m_data);
	}
}

void PicoCheat::write(u16 data) {
	if (m_addr < Pico.romsize) {
		*(u16 *)(Pico.rom + m_addr) = data;
	} else {
		/* RAM or some other weird patch */
		PicoWrite16(m_addr, data);
	}
}
