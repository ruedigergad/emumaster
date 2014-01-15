#include "ggmapper.h"

void GGMapper::writeLow(quint16 address, quint8 data) {
	switch (address & 0x6000) {
	case 0x0000: // RAM enable register
		break;
	case 0x2000: // GameGenie has only a half bank
		break;
	case 0x4000: // GameGenie has no RAM
		if (address >= 0x4001 && address <= 0x4020) // GG Hardware Registers
			writeDirect(address, data);
		break;
	case 0x6000: // GameGenie has only a half bank
		break;
	}
}
