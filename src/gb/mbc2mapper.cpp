#include "mbc2mapper.h"

void MBC2Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
}

void MBC2Mapper::writeLow(quint16 address, quint8 data) {
	switch(address & 0x6000) {
	case 0x0000: // RAM enable
		if (!(address & 0x0100))
			m_ramEnable = ((data & 0x0f) == 0x0a);
		break;
	case 0x2000: // ROM bank select
		if (address & 0x0100) {
			data &= 0x0f;

			if (data == 0)
				data = 1;
			if (m_romBank != data) {
				m_romBank = data;

				int tmpAddress = data << 14;

				tmpAddress &= romSizeMask();

				m_banks[0x04] = &m_rom[tmpAddress];
				m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
				m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
				m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
			}
		}
		break;
	}
}

void MBC2Mapper::writeExRam(quint16 address, quint8 data) {
	if (m_ramEnable) {
		if (ramSize() && address < 0xa200)
			writeDirect(address, data);
	}
}
