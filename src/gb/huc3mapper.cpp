#include "huc3mapper.h"

void HuC3Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
	m_ramBank = 0;
	m_ramAddress = 0;
	m_address = 0;
	m_ramFlag = 0;
	m_retValue = 0;
	for (int  i = 0; i < 8; i++)
		m_reg[i] = 0;
}

void HuC3Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress = 0;

	switch (address & 0x6000) {
	case 0x0000: // RAM enable register
		m_ramEnable = (data == 0x0a);
		m_ramFlag = data;
		if (m_ramFlag != 0x0a)
			m_ramBank = -1;
		break;
	case 0x2000: // ROM bank select
		data = data & 0x7f;
		if (data == 0)
			data = 1;
		if (data == m_romBank)
			break;

		tmpAddress = data << 14;

		tmpAddress &= romSizeMask();
		m_romBank = data;
		m_banks[0x04] = &m_rom[tmpAddress];
		m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
		m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
		m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		break;
	case 0x4000: // RAM bank select
		data = data & 0x03;
		if (data == m_ramBank)
			break;
		tmpAddress = data << 13;
		tmpAddress &= ramSizeMask();
		m_banks[0x0a] = &m_ram[tmpAddress];
		m_banks[0x0b] = &m_ram[tmpAddress + 0x1000];
		m_ramBank = data;
		m_ramAddress = tmpAddress;
		break;
	case 0x6000: // nothing to do!
		break;
	}
}

void HuC3Mapper::writeExRam(quint16 address, quint8 data) {
	quint8 *p;

	if (m_ramFlag < 0x0b || m_ramFlag > 0x0e) {
		if (m_ramEnable) {
			if (ramSize())
				writeDirect(address, data);
		}
	} else {
		if (m_ramFlag == 0x0b) {
			if (data == 0x62) {
				m_retValue = 1;
			} else {
				switch (data & 0xf0) {
				case 0x10:
					p = &m_reg[1];
					m_retValue = *(p+m_reg[0]++);
					if (m_reg[0] > 6)
						m_reg[0] = 0;
					break;
				case 0x30:
					p = &m_reg[1];
					*(p+m_reg[0]++) = data & 0x0f;
					if (m_reg[0] > 6)
						m_reg[0] = 0;
					m_address =
						(m_reg[5] << 24) |
						(m_reg[4] << 16) |
						(m_reg[3] << 8) |
						(m_reg[2] << 4) |
						(m_reg[1]);
					break;
				case 0x40:
					m_reg[0] = (m_reg[0] & 0xf0) |
						(data & 0x0f);
					m_reg[1] = ((m_address>> 0)&0x0f);
					m_reg[2] = ((m_address>> 4)&0x0f);
					m_reg[3] = ((m_address>> 8)&0x0f);
					m_reg[4] = ((m_address>>16)&0x0f);
					m_reg[5] = ((m_address>>24)&0x0f);
					m_reg[6] = 0;
					m_reg[7] = 0;
					m_retValue = 0;
					break;
				case 0x50:
					m_reg[0] = (m_reg[0] & 0x0f) |
						((data << 4)&0x0f);
					break;
				default:
					m_retValue = 1;
					break;
				}
			}
		}
	}
}

quint8 HuC3Mapper::readExRam(quint16 address) {
	if (m_ramFlag > 0x0b && m_ramFlag < 0x0e) {
		if (m_ramFlag != 0x0c)
			return 0x01;
		return m_retValue;
	} else {
		return readDirect(address);
	}
}
