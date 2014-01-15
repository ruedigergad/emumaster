#include "mbc7mapper.h"

void MBC7Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
	m_ramBank = 0;
	m_ramAddress = 0;
	m_chipSelect = 0;
	m_sk = 0;
	m_state = 0;
	m_buffer = 0;
	m_idle = false;
	m_count = 0;
	m_code = 0;
	m_address = 0;
	m_writeEnable = false;
	m_retValue = 0;
}

void MBC7Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress = 0;

	switch (address & 0x6000) {
	case 0x0000:
		break;
	case 0x2000: // ROM bank select
		data = data & 0x7f;
		if (data == 0)
			data = 1;

		if (data == m_romBank)
			break;

		tmpAddress = (data << 14);

		tmpAddress &= romSizeMask();
		m_romBank = data;
		m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
		m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
		m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
		m_banks[0x07] = &m_rom[tmpAddress + 0x3000];
		break;
	case 0x4000: // RAM bank select/enable
		if (data < 8) {
			tmpAddress = (data&3) << 13;
			tmpAddress &= gbRamSizeMask;
// TODO check it			m_banks[0x0a] = &gbMemory[0xa000];
//			m_banks[0x0b] = &gbMemory[0xb000];

			m_ramBank = data;
			m_ramAddress = tmpAddress;
			m_ramEnable = 0;
		} else {
			m_ramEnable = 0;
		}
		break;
	}
}

void MBC7Mapper::writeExRam(quint16 address, quint8 data) {
	if (address != 0xa080)
		return;
	// special processing needed
	int oldCs = m_chipSelect;
	int oldSk = m_sk;

	m_chipSelect=data>>7;
	m_sk=(data>>6)&1;

	if (!oldCs && m_chipSelect) {
		if (m_state==5) {
			if (m_writeEnable) {
				writeDirect(0xa000+m_address*2+0, (m_buffer>>8) & 0xFF);
				writeDirect(0xa000+m_address*2+1, (m_buffer>>0) & 0xFF);
			}
			m_state=0;
			m_retValue=1;
		} else {
			m_idle=true;
			m_state=0;
		}
	}

	if (!oldSk && m_sk) {
		if (m_idle) {
			if (data & 0x02) {
				m_idle=false;
				m_count=0;
				m_state=1;
			}
		} else {
			switch (m_state) {
			case 1:
				// receiving command
				m_buffer <<= 1;
				m_buffer |= (data & 0x02)?1:0;
				m_count++;
				if (m_count==2) {
					// finished receiving command
					m_state=2;
					m_count=0;
					m_code=m_buffer & 3;
				}
				break;
			case 2:
				// receive address
				m_buffer <<= 1;
				m_buffer |= (data&0x02)?1:0;
				m_count++;
				if (m_count==8) {
					// finish receiving
					m_state=3;
					m_count=0;
					m_address=m_buffer&0xff;
					if (m_code==0) {
						if ((m_address>>6)==0) {
							m_writeEnable=0;
							m_state=0;
						} else if ((m_address>>6) == 3) {
							m_writeEnable=1;
							m_state=0;
						}
					}
				}
				break;
			case 3:
				m_buffer <<= 1;
				m_buffer |= (data&0x02)?1:0;
				m_count++;

				switch (m_code) {
				case 0:
					if (m_count==16) {
						if ((m_address>>6)==0) {
							m_writeEnable = 0;
							m_state=0;
						} else if ((m_address>>6)==1) {
							if (m_writeEnable) {
								for (int i=0;i<256;i++) {
									writeDirect(0xa000+i*2+0, (m_buffer >> 8) & 0xff);
									writeDirect(0xa000+i*2+1, (m_buffer >> 0) & 0xff);
								}
							}
							m_state=5;
						} else if ((m_address>>6) == 2) {
							if (m_writeEnable) {
								quint8 *dst = m_banks[0xa];
								qMemSet(dst, 0xff, 256 * 2);
							}
							m_state=5;
						} else if ((m_address>>6)==3) {
							m_writeEnable = 1;
							m_state=0;
						}
						m_count=0;
					}
					break;
				case 1:
					if (m_count==16) {
						m_count=0;
						m_state=5;
						m_retValue=0;
					}
					break;
				case 2:
					if (m_count==1) {
						m_state=4;
						m_count=0;
						m_buffer = (readDirect(0xa000+m_address*2)<<8) | (readDirect(0xa000+m_address*2+1));
					}
					break;
				case 3:
					if (m_count==16) {
						m_count=0;
						m_state=5;
						m_retValue=0;
						m_buffer=0xffff;
					}
					break;
				}
				break;
			}
		}
	}

	if (oldSk && !m_sk) {
		if (m_state==4) {
			m_retValue = (m_buffer & 0x8000)?1:0;
			m_buffer <<= 1;
			m_count++;
			if (m_count==16) {
				m_count=0;
				m_state=0;
			}
		}
	}
}

quint8 MBC7Mapper::readExRam(quint16 address) {
	switch (address & 0xa0f0) {
	case 0xa000:
	case 0xa010:
	case 0xa060:
	case 0xa070:
		return 0;
	/* TODO tilt sensor case 0xa020:
		// sensor X low byte
		return systemGetSensorX() & 255;
	case 0xa030:
		// sensor X high byte
		return systemGetSensorX() >> 8;
	case 0xa040:
		// sensor Y low byte
		return systemGetSensorY() & 255;
	case 0xa050:
		// sensor Y high byte
		return systemGetSensorY() >> 8;*/
	case 0xa080:
		return m_retValue;
	}
	return 0xff;
}
