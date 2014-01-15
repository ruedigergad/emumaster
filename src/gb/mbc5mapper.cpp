#include "mbc5mapper.h"

void MBC5Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
	m_ramBank = 0;
	m_romHighAddress = 0;
	m_ramAddress = 0;
	m_rumble = (romType() >= 0x1C && romType() < 0x20);
}

void MBC5Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress = 0;

	switch (address & 0x6000) {
	case 0x0000: // RAM enable register
		m_ramEnable = ((data & 0x0a) == 0x0a);
		break;
	case 0x2000: // ROM bank select
		if (address < 0x3000) {
			data = data & 0xff;
			if (data == m_romBank)
				break;

			tmpAddress = (data << 14) | (m_romHighAddress << 22) ;

			tmpAddress &= romSizeMask();
			m_romBank = data;
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
			m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
			m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		} else {
			data &= 0x01;
			if (data == m_romHighAddress)
				break;

			tmpAddress = (m_romBank << 14) | (data << 22);

			tmpAddress &= romSizeMask();
			m_romHighAddress = data;
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
			m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
			m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		}
		break;
	case 0x4000: // RAM bank select
		data &= (m_ramble ? 0x07 : 0x0F);
		if (data == m_ramBank)
			break;
		tmpAddress = data << 13;
		tmpAddress &= ramSizeMask();
		if (ramSize()) {
			m_banks[0x0a] = &m_ram[tmpAddress + 0x0000];
			m_banks[0x0b] = &m_ram[tmpAddress + 0x1000];

			m_ramBank = data;
			m_ramAddress = tmpAddress;
		}
		break;
	}
}

void MBC5Mapper::writeExRam(quint16 address, quint8 data) {
	if (m_ramEnable) {
		if (ramSize())
			writeDirect(address, data);
	}
}

quint8 MBC5Mapper::readExRam(quint16 address) {
	if (m_ramEnable)
		return readDirect(address);
	return 0xff;
}
