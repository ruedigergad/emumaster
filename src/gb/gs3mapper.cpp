#include "gs3mapper.h"

void GS3Mapper::reset() {
	GBCpuMapper::reset();
	m_romBank = 1;
}

void GS3Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress;

	switch (address & 0x6000) {
	case 0x0000: // GS has no ram
		break;
	case 0x2000: // GS has no 'classic' ROM bank select
		break;
	case 0x4000: // GS has no ram
		break;
	case 0x6000: // 0x6000 area is RW, and used for GS hardware registers
		if (address == 0x7FE1) { // This is the (half) ROM bank select register
			if (data == m_romBank)
				break;
			m_romBank = data;

			tmpAddress = data << 13;
			tmpAddress &= romSizeMask();
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
		} else {
			writeDirect(address, data);
		}
		break;
	}
}
