#include "mbc1mapper.h"

void MBC1Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
	m_ramBank = 0;
	m_memoryModel = 0;
	m_romHighAddress = 0;
	m_ramAddress = 0;
	m_romBank0Remapping = 0;
}

void MBC1Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress = 0;

	switch(address & 0x6000) {
	case 0x0000: // RAM enable register
		m_ramEnable = ((data & 0x0A) == 0x0A);
		break;
	case 0x2000: // ROM bank select
		if ((data == 1) && (address == 0x2100))
			m_romBank0Remapping = 1;

		if ((data & 0x1f) == 0)
			data += 1;
		if (data == m_romBank)
			break;

		tmpAddress = data << 14;

		// check current model
		if (m_romBank0Remapping == 3) {
			tmpAddress = (data & 0xf) << 14;
			tmpAddress |= m_romHighAddress << 18;
		} else if (m_memoryModel == 0) {
			// model is 16/8, so we have a high address in use
			tmpAddress |= m_romHighAddress << 19;
		}
		tmpAddress &= romSizeMask();
		m_romBank = data;
		m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
		m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
		m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
		m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		break;
	case 0x4000: // RAM bank select
		if (m_memoryModel == 1) {
			if (!ramSize()) {
				if (m_romBank0Remapping == 3) {
					m_romHighAddress = data & 0x03;
					tmpAddress = (m_romHighAddress) << 18;
					tmpAddress &= romSizeMask();
					m_banks[0x00] = &m_rom[tmpAddress + 0x0000];
					m_banks[0x01] = &m_rom[tmpAddress + 0x1000];
					m_banks[0x02] = &m_rom[tmpAddress + 0x2000];
					m_banks[0x03] = &m_rom[tmpAddress + 0x3000];
					m_banks[0x04] = &m_rom[tmpAddress + 0x4000];
					m_banks[0x05] = &m_rom[tmpAddress + 0x5000];
					m_banks[0x06] = &m_rom[tmpAddress + 0x6000];
					m_banks[0x07] = &m_rom[tmpAddress + 0x7000];
				} else {
					m_romBank0Remapping = 0;
				}
			}
			// 4/32 model, RAM bank switching provided
			data = data & 0x03;
			if (data == m_ramBank)
				break;
			tmpAddress = data << 13;
			tmpAddress &= ramSizeMask();
			if (ramSize()) {
				m_banks[0x0a] = &m_ram[tmpAddress + 0x0000];
				m_banks[0x0b] = &m_ram[tmpAddress + 0x1000];
			}
			m_ramBank = data;
			m_ramAddress = tmpAddress;

			if (m_romBank0Remapping != 3)
				m_romHighAddress = 0;
		} else {
			// 16/8, set the high address
			m_romHighAddress = data & 0x03;
			tmpAddress = m_romBank << 14;
			tmpAddress |= (m_romHighAddress) << 19;
			tmpAddress &= romSizeMask();
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
			m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
			m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
			if (ramSize()) {
				m_banks[0x0a] = &m_ram[0];
				m_banks[0x0b] = &m_ram[0x1000];
			}

			m_ramBank = 0;
		}
		break;
	case 0x6000: // memory model select
		m_memoryModel = data & 1;

		if (m_memoryModel == 1) {
			// 4/32 model, RAM bank switching provided

			data = m_ramBank & 0x03;
			tmpAddress = data << 13;
			tmpAddress &= ramSizeMask();
			if (ramSize()) {
				m_banks[0x0a] = &m_ram[m_ramAddress + 0x0000];
				m_banks[0x0b] = &m_ram[m_ramAddress + 0x1000];
				m_romBank0Remapping = 0;
			} else {
				m_romBank0Remapping |=2;
			}

			m_ramBank = data;
			m_ramAddress = tmpAddress;

			tmpAddress = m_romBank << 14;

			tmpAddress &= romSizeMask();
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
			m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
			m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		} else {
			// 16/8, set the high address

			tmpAddress = m_romBank << 14;
			tmpAddress |= (m_romHighAddress) << 19;
			tmpAddress &= romSizeMask();
			m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
			m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
			m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
			m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
			if (ramSize()) {
				m_banks[0x0a] = &m_ram[0x0000];
				m_banks[0x0b] = &m_ram[0x1000];
			}
		}
		break;
	}
}

void MBC1Mapper::writeExRam(quint16 address, quint8 data) {
	if (m_ramEnable) {
		if (ramSize())
			writeDirect(address, data);
	}
}

quint8 MBC1Mapper::readExRam(quint16 address) {
	if (m_ramEnable)
		return readDirect(address);
	return 0xff;
}
