#include "gbcpumapper.h"
#include "gbcpu.h"
/*
 * In order to make reads and writes efficient, we keep tables
 * (indexed by the high nibble of the address) specifying which
 * regions can be read/written without a function call. For such
 * ranges, the pointer in the map table points to the base of the
 * region in host system memory. For ranges that require special
 * processing, the pointer is NULL.
 */

GBCpuMapper::GBCpuMapper(QObject *parent) :
	QObject(parent)
{
}

void GBCpuMapper::reset() {
	m_rbanks[0x0] = m_rom;
	m_rbanks[0x1] = m_rom;
	m_rbanks[0x2] = m_rom;
	m_rbanks[0x3] = m_rom;
	m_rbanks[0x4] = 0;
	m_rbanks[0x5] = 0;
	m_rbanks[0x6] = 0;
	m_rbanks[0x7] = 0;

	// TODO
	m_rbanks[0x8] = lcd.vbank[R_VBK & 1] - 0x8000;
	m_rbanks[0x9] = lcd.vbank[R_VBK & 1] - 0x8000;

	m_rbanks[0xA] = 0;
	m_rbanks[0xB] = 0;

	// TODO
	m_rbanks[0xC] = ram.ibank[0] - 0xC000;
	n = R_SVBK & 0x07;
	m_rbanks[0xD] = ram.ibank[n?n:1] - 0xD000;
	m_rbanks[0xE] = ram.ibank[0] - 0xE000;

	m_rbanks[0xF] = 0;

	m_wbanks[0x0] = 0;
	m_wbanks[0x1] = 0;
	m_wbanks[0x2] = 0;
	m_wbanks[0x3] = 0;
	m_wbanks[0x4] = 0;
	m_wbanks[0x5] = 0;
	m_wbanks[0x6] = 0;
	m_wbanks[0x7] = 0;

	m_wbanks[0x8] = 0;
	m_wbanks[0x9] = 0;

	m_wbanks[0xA] = 0;
	m_wbanks[0xB] = 0;

	// TODO
	m_wbanks[0xC] = ram.ibank[0] - 0xC000;
	n = R_SVBK & 0x07;
	m_wbanks[0xD] = ram.ibank[n?n:1] - 0xD000;
	m_wbanks[0xE] = ram.ibank[0] - 0xE000;

	m_wbanks[0xF] = 0;
}

void GBCpuMapper::writeSlowPath(quint16 address, quint8 data) {
	switch ((address >> 12) & 0xE) {
	case 0x0:
	case 0x2:
	case 0x4:
	case 0x6:
		writeLow(address, data);
		break;
	case 0x8:
		m_lcd->vramWrite(address & 0x1FFF, data);
		break;
	case 0xA:
		writeExRam(address, data);
		break;
	case 0xC:
		m_wbanks[address >> 12][address] = data;
		break;
	case 0xE:
		if (address < 0xFE00) {
			writeSlowPath(address & 0xDFFF, data);
		} else if ((address & 0xFF00) == 0xFE00) {
			if (address < 0xFEA0)
				lcd.oam.mem[address & 0xFF] = data;
		} else if (address >= 0xFF10 && address <= 0xFF3F) {
			sound_write(address & 0xFF, data);
		} else if ((address & 0xFF80) == 0xFF80 && address != 0xFFFF) {
			m_cpu->highRam()[address & 0xFF] = data;
		} else {
			writeIO(address & 0xFF, data);
		}
		break;
	}
}


quint8 GBCpuMapper::readSlowPath(quint16 address) {
	switch ((address>>12) & 0xE) {
	case 0x0:
	case 0x2:
	case 0x4:
	case 0x6:
	case 0x8:
		return m_rbanks[address >> 12][address];
		break;
	case 0xA:
		return readExRam(address);
		break;
	case 0xC:
		return m_rbanks[address >> 12][address];
		break;
	case 0xE:
		if (address < 0xFE00) {
			return readSlowPath(address & 0xDFFF);
		} else if ((address & 0xFF00) == 0xFE00) {
			/* if (R_STAT & 0x02) return 0xFF; */
			if (address < 0xFEA0)
				return lcd.oam.mem[address & 0xFF];
			return 0xFF;
		} else if (address == 0xFFFF || (address & 0xFF80) == 0xFF80) {
			return m_cpu->highRam()[0xFF];
		} else if (address >= 0xFF10 && address <= 0xFF3F) {
			return sound_read(address & 0xFF);
		} else {
			return readIO(address & 0xFF);
		}
		break;
	}
}

void GBCpuMapper::writeLow(quint16 address, quint8 data) {
	Q_UNUSED(address)
	Q_UNUSED(data)
}

void GBCpuMapper::writeExRam(quint16 address, quint8 data) {
	Q_UNUSED(address)
	Q_UNUSED(data)
}

quint8 GBCpuMapper::readExRam(quint16 address)
{ return readDirect(address); }

void GBCpuMapper::writeIO(quint8 address, quint8 data) {
	// TODO write to LY should reset LY
	if (!hw.cgb) {
		switch (r) {
		case RI_VBK:
		case RI_BCPS:
		case RI_OCPS:
		case RI_BCPD:
		case RI_OCPD:
		case RI_SVBK:
		case RI_KEY1:
		case RI_HDMA1:
		case RI_HDMA2:
		case RI_HDMA3:
		case RI_HDMA4:
		case RI_HDMA5:
			return;
		}
	}

	switch (address) {
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		m_cpu->timer().write(address, data);
		break;

	case RI_SCY:
	case RI_SCX:
	case RI_WY:
	case RI_WX:
		REG(r) = data;
		break;
	case RI_BGP:
		if (R_BGP == data) break;
		pal_write_dmg(0, 0, data);
		pal_write_dmg(8, 1, data);
		R_BGP = data;
		break;
	case RI_OBP0:
		if (R_OBP0 == data) break;
		pal_write_dmg(64, 2, data);
		R_OBP0 = data;
		break;
	case RI_OBP1:
		if (R_OBP1 == data) break;
		pal_write_dmg(72, 3, data);
		R_OBP1 = data;
		break;
	case RI_IF:
	case RI_IE:
		REG(r) = data & 0x1F;
		break;
	case RI_P1:
		REG(r) = data;
		pad_refresh();
		break;
	case RI_SC:
		/* FIXME - this is a hack for stupid roms that probe serial */
		if ((data & 0x81) == 0x81) {
			R_SB = 0xff;
			hw_interrupt(IF_SERIAL, IF_SERIAL);
			hw_interrupt(0, IF_SERIAL);
		}
		R_SC = data; /* & 0x7f; */
		break;
	case RI_LCDC:
		lcdc_change(data);
		break;
	case RI_STAT:
		stat_write(data);
		break;
	case RI_LYC:
		REG(r) = data;
		stat_trigger();
		break;
	case RI_VBK:
		REG(r) = data | 0xFE;
		mem_updatemap();
		break;
	case RI_BCPS:
		R_BCPS = data & 0xBF;
		R_BCPD = lcd.pal[data & 0x3F];
		break;
	case RI_OCPS:
		R_OCPS = data & 0xBF;
		R_OCPD = lcd.pal[64 + (data & 0x3F)];
		break;
	case RI_BCPD:
		R_BCPD = data;
		pal_write(R_BCPS & 0x3F, data);
		if (R_BCPS & 0x80) R_BCPS = (R_BCPS+1) & 0xBF;
		break;
	case RI_OCPD:
		R_OCPD = data;
		pal_write(64 + (R_OCPS & 0x3F), data);
		if (R_OCPS & 0x80) R_OCPS = (R_OCPS+1) & 0xBF;
		break;
	case RI_SVBK:
		REG(r) = data & 0x07;
		mem_updatemap();
		break;
	case RI_DMA:
		hw_dma(data);
		break;
	case RI_KEY1:
		REG(r) = (REG(r) & 0x80) | (data & 0x01);
		break;
	case RI_HDMA1:
		REG(r) = data;
		break;
	case RI_HDMA2:
		REG(r) = data & 0xF0;
		break;
	case RI_HDMA3:
		REG(r) = data & 0x1F;
		break;
	case RI_HDMA4:
		REG(r) = data & 0xF0;
		break;
	case RI_HDMA5:
		hw_hdma_cmd(data);
		break;
	}
}

quint8 GBCpuMapper::readIO(quint8 address) {
	switch (address) {
	case RI_SC:
		r = R_SC;
		R_SC &= 0x7F;
		return r;
	case RI_P1:
	case RI_SB:

	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		return m_cpu->timer().read(address);
		break;
	case RI_LCDC:
	case RI_STAT:
	case RI_SCY:
	case RI_SCX:
	case RI_LY:
	case RI_LYC:
	case RI_BGP:
	case RI_OBP0:
	case RI_OBP1:
	case RI_WY:
	case RI_WX:

	case 0x0F:
		return m_cpu->highRam()[0x0F];
	case RI_VBK:
	case RI_BCPS:
	case RI_OCPS:
	case RI_BCPD:
	case RI_OCPD:
	case RI_SVBK:
	case RI_KEY1:
	case RI_HDMA1:
	case RI_HDMA2:
	case RI_HDMA3:
	case RI_HDMA4:
	case RI_HDMA5:
		if (hw.cgb) return REG(r);
	default:
		return 0xFF;
	}
}
