#include "huc1mapper.h"

void HuC1Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
	m_ramBank = 0;
	m_memoryModel = 0;
	m_romHighAddress = 0;
	m_ramAddress = 0;
}

void HuC1Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress = 0;

	switch (address & 0x6000) {
	case 0x0000: // RAM enable register
	  m_ramEnable = ((data & 0x0a) == 0x0a);
	  break;
	case 0x2000: // ROM bank select
		data = data & 0x3f;
		if (data == 0)
			data = 1;
		if (data == m_romBank)
			break;

		tmpAddress = data << 14;

		tmpAddress &= romSizeMask();
		m_romBank = data;
		m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
		m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
		m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
		m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		break;
	case 0x4000: // RAM bank select
		if (m_memoryModel == 1) {
			// 4/32 model, RAM bank switching provided
			data = data & 0x03;
			if (data == m_ramBank)
				break;
			tmpAddress = data << 13;
			tmpAddress &= ramSizeMask();
			m_banks[0x0a] = &m_ram[tmpAddress + 0x0000];
			m_banks[0x0b] = &m_ram[tmpAddress + 0x1000];
			m_ramBank = data;
			m_ramAddress = tmpAddress;
		} else {
			// 16/8, set the high address
			m_romHighAddress = data & 0x03;
			tmpAddress = m_romBank << 14;
			tmpAddress |= m_romHighAddress << 19;
			tmpAddress &= romSizeMask();
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
			m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
			m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		}
		break;
	case 0x6000: // memory model select
		m_memoryModel = data & 1;
		break;
	}
}

void HuC1Mapper::writeExRam(quint16 address, quint8 data) {
	if (m_ramEnable) {
		if (ramSize())
			writeDirect(address, data);
	}
}
