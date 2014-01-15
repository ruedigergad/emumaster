#include "mbc3mapper.h"

void MBC3Mapper::reset() {
	GBCpuMapper::reset();
	m_ramEnable = false;
	m_romBank = 1;
	m_ramBank = 0;
	m_ramAddress = 0;

	m_clockRegister = 0;
	m_clockLatch = 0;

	m_seconds = 0;
	m_minutes = 0;
	m_hours = 0;
	m_days = 0;
	m_control = 0;

	m_latchSeconds = 0;
	m_latchMinutes = 0;
	m_latchHours = 0;
	m_latchDays = 0;
	m_latchControl = 0;

	m_lastTime = (time_t)-1;
}

void MBC3Mapper::updateClock() {
	time_t now = time(NULL);
	time_t diff = now - m_lastTime;
	if (diff > 0) {
		// update the clock according to the last update time
		m_seconds += (int)(diff % 60);
		if (m_seconds > 59) {
			m_seconds -= 60;
			m_minutes++;
		}

		diff /= 60;

		m_minutes += (int)(diff % 60);
		if (m_minutes > 59) {
			m_minutes -= 60;
			m_hours++;
		}

		diff /= 60;

		m_hours += (int)(diff % 24);
		if (m_hours > 23) {
			m_hours -= 24;
			m_days++;
		}
		diff /= 24;

		m_days += (int)(diff & 0xffffffff);
		if (m_days > 255) {
			if (m_days > 511) {
				m_days %= 512;
				m_control |= 0x80;
			}
			m_control = (m_control & 0xfe) | (m_days>255 ? 1 : 0);
		}
	}
	m_lastTime = now;
}

void MBC3Mapper::writeLow(quint16 address, quint8 data) {
	int tmpAddress = 0;

	switch(address & 0x6000) {
	case 0x0000: // RAM enable register
		m_ramEnable = ((data & 0x0a) == 0x0a);
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
	  m_banks[0x04] = &m_rom[tmpAddress + 0x0000];
	  m_banks[0x05] = &m_rom[tmpAddress + 0x1000];
	  m_banks[0x06] = &m_rom[tmpAddress + 0x2000];
	  m_banks[0x07] = &m_rom[tmpAddress + 0x3000];

	  break;
	case 0x4000: // RAM bank select
		if (data < 8) {
			if (data == m_ramBank)
				break;
			tmpAddress = data << 13;
			tmpAddress &= ramSizeMask();
			m_banks[0x0a] = &m_ram[tmpAddress + 0x0000];
			m_banks[0x0b] = &m_ram[tmpAddress + 0x1000];
			m_ramBank = data;
			m_ramAddress = tmpAddress;
		} else {
			if (m_ramEnable) {
				m_ramBank = -1;
				m_clockRegister = data;
			}
		}
		break;
	case 0x6000: // clock latch
		if (m_clockLatch == 0 && data == 1) {
			updateClock();
			m_latchSeconds = m_seconds;
			m_latchMinutes = m_minutes;
			m_latchHours   = m_hours;
			m_latchDays    = m_days;
			m_latchControl = m_control;
		}
		if (data == 0x00 || data == 0x01)
			m_clockLatch = data;
		break;
	}
}

void MBC3Mapper::writeExRam(quint16 address, quint8 data) {
	if (m_ramEnable) {
		if (m_ramBank != -1) {
			if (ramSize())
			  writeDirect(address, data);
		} else {
			time(&m_lastTime);
			switch (m_clockRegister) {
			case 0x08: m_seconds = data; break;
			case 0x09: m_minutes = data; break;
			case 0x0a: m_hours = data; break;
			case 0x0b: m_days = data; break;
			case 0x0c: m_control = (m_control & 0x80) | data; break;
			}
		}
	}
}

quint8 MBC3Mapper::readExRam(quint16 address) {
	if (m_ramEnable) {
		if (m_ramBank != -1)
			return readDirect(address);

		switch (m_clockRegister) {
		case 0x08: return m_latchSeconds; break;
		case 0x09: return m_latchMinutes; break;
		case 0x0a: return m_latchHours; break;
		case 0x0b: return m_latchDays; break;
		case 0x0c: return m_latchControl; break;
		}
	}
	return 0xff;
}
