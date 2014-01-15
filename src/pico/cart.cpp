/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "cart.h"
#include "pico.h"
#include <QFile>

PicoCart picoCart;

static void memByteSwap(u8 *data, int len) {
	for (; len >= 2; len-=2) {
		u16 *pd = (u16 *)(data);
		u16 value = *pd; // Get 2 bytes
		value = (value<<8) | (value>>8); // Byteswap it
		*pd = value; // Put 2 bytes
		data += 2;
	}
}

/** Interleaves a 16k block and byteswap. */
static void interleaveBlock(u8 *dst, u8 *src) {
	for (int i = 0; i < 0x2000; i++)
		dst[(i<<1)  ] = src[       i]; // Odd
	for (int i = 0; i < 0x2000; i++)
		dst[(i<<1)+1] = src[0x2000+i]; // Even
}

PicoCart::PicoCart() {
	picoRom = 0;
	picoRomSize = 0;
}

PicoCart::~PicoCart() {
	close();
}

bool PicoCart::open(const QString &fileName, QString *error) {
	close();

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		*error = QObject::tr("Could not open cart file (%1)").arg(fileName);
		return false;
	}

	if (file.size() <= 0) {
		*error = QObject::tr("Cart file is empty");
		return false;
	}
	picoRomSize = file.size();
	picoRomSize = (picoRomSize+3) & ~3; // Round up to a multiple of 4

	// Allocate space for the rom plus padding
	if (!allocateMem()) {
		*error = QObject::tr("Could not allocate memory for the cart");
		return false;
	}

	// Load the rom
	if (file.read((char *)picoRom, picoRomSize) <= 0) {
		*error = QObject::tr("IO error while loading cart");
		return false;
	}

	// Check for SMD:
	if ((picoRomSize&0x3FFF) == 0x200)
		decodeSmd(); // Decode and byteswap SMD
	else
		memByteSwap(picoRom, picoRomSize); // Just byteswap


	// TODO remove later
	Pico.rom = picoRom;
	Pico.romsize = picoRomSize;

	// setup correct memory map for loaded ROM
	if (picoMcdOpt & PicoMcdEnabled)
		PicoMemSetupCD();
	else
		PicoMemSetup();
	PicoMemReset();

	if (!(picoMcdOpt & PicoMcdEnabled))
		detect();

	return true;
}

/** Load MegaCD Bios which is a cart. */
bool PicoCart::openMegaCDBios(const QString &fileName, QString *error) {
	if (!open(fileName, error))
		return false;

	// Check the size
	if (picoRomSize != 0x20000) {
		*error = QObject::tr("Invalid size of the bios file, should be 128KB");
		return false;
	}

	// Check for "BOOT" signature, remember it was byte swapped
	if (strncmp((char *)picoRom+0x124, "OBTO", 4) &&
		strncmp((char *)picoRom+0x128, "OBTO", 4)) {
		*error = QObject::tr("Invalid bios file, \"BOOT\" signature not found");
		return false;
	}

	return true;
}

void PicoCart::close() {
//	TODO delete[] picoRom;
//	picoRom = 0;
//	picoRomSize = 0;
}

/** Decodes a SMD file. **/
void PicoCart::decodeSmd() {
	u8 *tmp = new u8[0x4000];
	memset(tmp, 0 ,0x4000);
	// Interleave each 16k block and shift down by 0x200:
	for (int i = 0; i+0x4200 <= picoRomSize; i += 0x4000) {
		interleaveBlock(tmp, picoRom+0x200+i); // Interleave 16k to temporary buffer
		memcpy(picoRom+i, tmp, 0x4000); // Copy back in
	}
	delete[] tmp;

	picoRomSize -= 0x200;
}

bool PicoCart::allocateMem() {
	// TODO
	if (picoRom)
		return true;

	int allocSize = picoRomSize + 0x7FFFF;

	if ((picoRomSize&0x3FFF) == 0x200)
		allocSize -= 0x200;

	allocSize &= ~0x7FFFF; // use alloc size of multiples of 512K, so that memhandlers could be set up more efficiently

	if ((picoRomSize&0x3FFF) == 0x200)
		allocSize += 0x200;
	else if (allocSize-picoRomSize < 4)
		allocSize += 4; // padding for out-of-bound exec protection

	picoRom = new u8[allocSize];

	if (picoRom)
		memset(picoRom+allocSize-0x80000, 0, 0x80000);

	return picoRom != 0;
}

static int strcmpRom(int romOffset, const char *s1) {
	int i, len = strlen(s1);
	const char *s_rom = (const char *)Pico.rom + romOffset;
	for (i = 0; i < len; i++)
		if (s1[i] != s_rom[i^1])
			return 1;
	return 0;
}

static int nameCmp(const char *name) {
	return strcmpRom(0x150, name);
}

/** Various cart-specific things, which can't be handled by generic code. */
void PicoCart::detect() {
	int sram_size = 0, csum;
	if (SRam.data) {
		free(SRam.data);
		SRam.data = 0;
	}
	Pico.m.sram_reg = 0;

	csum = PicoRead32(0x18c) & 0xffff;

	if (Pico.rom[0x1B1] == 'R' && Pico.rom[0x1B0] == 'A') {
		if (Pico.rom[0x1B2] & 0x40) {
			// EEPROM
			SRam.start = PicoRead32(0x1B4) & ~1; // zero address is used for clock by some games
			SRam.end   = PicoRead32(0x1B8);
			sram_size  = 0x2000;
			Pico.m.sram_reg |= 4;
		} else {
			// normal SRAM
			SRam.start = PicoRead32(0x1B4) & ~0xff;
			SRam.end   = PicoRead32(0x1B8) | 1;
			sram_size  = SRam.end - SRam.start + 1;
		}
		SRam.start &= ~0xff000000;
		SRam.end   &= ~0xff000000;
		Pico.m.sram_reg |= 0x10; // SRAM was detected
	}
	if (sram_size <= 0) {
		// some games may have bad headers, like S&K and Sonic3
		// note: majority games use 0x200000 as starting address, but there are some which
		// use something else (0x300000 by HardBall '95). Luckily they have good headers.
		SRam.start = 0x200000;
		SRam.end   = 0x203FFF;
		sram_size  = 0x004000;
	}

	if (sram_size) {
		SRam.data = (unsigned char *) calloc(sram_size, 1);
		if (!SRam.data)
			return;
	}
	SRam.changed = 0;

	// set EEPROM defaults, in case it gets detected
	SRam.eeprom_type   = 0; // 7bit (24C01)
	SRam.eeprom_abits  = 3; // eeprom access must be odd addr for: bit0 ~ cl, bit1 ~ in
	SRam.eeprom_bit_cl = 1;
	SRam.eeprom_bit_in = 0;
	SRam.eeprom_bit_out= 0;

	// some known EEPROM data (thanks to EkeEke)
	if (nameCmp("COLLEGE SLAM") == 0 ||
		nameCmp("FRANK THOMAS BIGHURT BASEBAL") == 0) {
		SRam.eeprom_type = 3;
		SRam.eeprom_abits = 2;
		SRam.eeprom_bit_cl = 0;
	} else if (nameCmp("NBA JAM TOURNAMENT EDITION") == 0 ||
			   nameCmp("NFL QUARTERBACK CLUB") == 0) {
		SRam.eeprom_type = 2;
		SRam.eeprom_abits = 2;
		SRam.eeprom_bit_cl = 0;
	} else if (nameCmp("NBA JAM") == 0) {
		SRam.eeprom_type = 2;
		SRam.eeprom_bit_out = 1;
		SRam.eeprom_abits = 0;
	} else if (nameCmp("NHLPA HOCKEY '93") == 0 ||
			   nameCmp("NHLPA Hockey '93") == 0 ||
			   nameCmp("RINGS OF POWER") == 0) {
		SRam.start = SRam.end = 0x200000;
		Pico.m.sram_reg = 0x14;
		SRam.eeprom_abits = 0;
		SRam.eeprom_bit_cl = 6;
		SRam.eeprom_bit_in = 7;
		SRam.eeprom_bit_out= 7;
	} else if (nameCmp("MICRO MACHINES II") == 0 ||
			  (nameCmp("        ") == 0 && // Micro Machines {Turbo Tournament '96, Military - It's a Blast!}
			   (csum == 0x165e || csum == 0x168b || csum == 0xCEE0 || csum == 0x2C41))) {
		SRam.start = 0x300000;
		SRam.end   = 0x380001;
		Pico.m.sram_reg = 0x14;
		SRam.eeprom_type = 2;
		SRam.eeprom_abits = 0;
		SRam.eeprom_bit_cl = 1;
		SRam.eeprom_bit_in = 0;
		SRam.eeprom_bit_out= 7;
	}

	// Some games malfunction if SRAM is not filled with 0xff
	if (nameCmp("DINO DINI'S SOCCER") == 0 ||
		nameCmp("MICRO MACHINES II") == 0) {
		memset(SRam.data, 0xff, sram_size);
	}

	// Unusual region 'code'
	if (strcmpRom(0x1f0, "EUROPE") == 0)
		*(u32 *) (Pico.rom+0x1f0) = 0x20204520;
}
