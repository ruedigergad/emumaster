#include "gblcd.h"

#define DEF_PAL { 0x98d0e0, 0x68a0b0, 0x60707C, 0x2C3C3C }

static int dmg_pal[4][4] = { DEF_PAL, DEF_PAL, DEF_PAL, DEF_PAL };

GBLcd::GBLcd(QObject *parent) :
    QObject(parent)
{
}

void GBLcd::reset() {
	m_cycles = 40;
}

void GBLcd::hw_dma(quint8 b) {
	int i;
	quint16 a;

	a = ((addr)b) << 8;
	for (i = 0; i < 160; i++, a++)
		lcd.oam.mem[i] = readb(a);
}

void GBLcd::hw_hdma() {
	int cnt;
	quint16 sa;
	int da;

	sa = ((quint16)RH->HDMA1 << 8) | (RH->HDMA2&0xf0);
	da = 0x8000 | ((int)(RH->HDMA3&0x1f) << 8) | (RH->HDMA4&0xf0);
	cnt = 16;
	while (cnt--)
		writeb(da++, readb(sa++));
	cpu_timers(16); /* SEE COMMENT A ABOVE */
	RH->HDMA1 = sa >> 8;
	RH->HDMA2 = sa & 0xF0;
	RH->HDMA3 = 0x1F & (da >> 8);
	RH->HDMA4 = da & 0xF0;
	RH->HDMA5--;
	hw.hdma--;
}


void GBLcd::hw_hdma_cmd(quint8 c) {
	int cnt;
	quint16 sa;
	int da;

	/* Begin or cancel HDMA */
	if ((hw.hdma|c) & 0x80) {
		hw.hdma = c;
		RH->HDMA5 = c & 0x7f;
		if ((R->STAT&0x03) == 0x00)
			hw_hdma(); /* SEE COMMENT A ABOVE */
		return;
	}

	/* Perform GDMA */
	sa = ((quint16)RH->HDMA1 << 8) | (RH->HDMA2&0xf0);
	da = 0x8000 | ((int)(RH->HDMA3&0x1f) << 8) | (RH->HDMA4&0xf0);
	cnt = ((int)c)+1;
	/* FIXME - this should use cpu time! */
	/*cpu_timers(102 * cnt);*/
	cpu_timers((460>>cpu.speed)+cnt*16); /*dalias*/
	/*cpu_timers(228 + (16*cnt));*/ /* this should be right according to no$ */
	cnt <<= 4;
	while (cnt--)
		writeb(da++, readb(sa++));
	RH->HDMA1 = sa >> 8;
	RH->HDMA2 = sa & 0xF0;
	RH->HDMA3 = 0x1F & (da >> 8);
	RH->HDMA4 = da & 0xF0;
	RH->HDMA5 = 0xFF;
}

void GBLcd::emulate() {
	if (!isDisplayEnabled()) {
		if (m_cycles <= 0) {
			switch (R->STAT & 3) {
			case 0:
			case 1:
				setStatusIntBits(2);
				m_cycles += 40;
				break;
			case 2:
				setStatusIntBits(3);
				m_cycles += 86;
				break;
			case 3:
				setStatusIntBits(0);
				if (hw.hdma & 0x80)
					hw_hdma();
				else
					m_cycles += 102;
				break;
			}
		}
		return;
	}
	while (m_cycles <= 0) {
		switch (R->STAT & 3) {
		case 1:
			if (!(hw.ilines & IF_VBLANK)) {
				m_cycles += 218;
				hw_interrupt(IF_VBLANK, IF_VBLANK);
				break;
			}
			if (R->LY == 0) {
				m_wy = R->WY;
				setStatusIntBits(2);
				m_cycles += 40;
				break;
			} else if (R->LY < 152) {
				m_cycles += 228;
			} else if (R->LY == 152) {
				m_cycles += 28;
			} else {
				R->LY = -1;
				m_cycles += 200;
			}
			R->LY++;
			updateStatusInt();
			break;
		case 2:
			processScanline();
			setStatusIntBits(3);
			m_cycles += 86;
			break;
		case 3:
			setStatusIntBits(0);
			if (hw.hdma & 0x80)
				hw_hdma();
			/* FIXME -- how much of the hblank does hdma use?? */
			m_cycles += 102;
			break;
		case 0:
			if (++R_LY >= 144) {
				if (cpu.halt) {
					hw_interrupt(IF_VBLANK, IF_VBLANK);
					m_cycles += 228;
				} else {
					m_cycles += 10;
				}
				setStatusIntBits(1);
			} else {
				setStatusIntBits(2);
				m_cycles += 40;
			}
			break;
		}
	}
}

void GBLcd::setStatusIntBits(quint8 newStatus) {
	Q_ASSERT(newStatus <= 3);
	R->STAT = (R->STAT & ~3) | newStatus;
	if (newStatus != 1)
		hw_interrupt(0, IF_VBLANK);
	updateStatusInt();
}

void GBLcd::setLCDC(quint8 data) {
	quint8 old = R->LCDC;
	R->LCDC = data;
	if ((R->LCDC ^ old) & 0x80) {
		/* lcd on/off change */
		R->LY = 0x00;
		setStatusIntBits(2);
		C = 40;
		m_wy = R->WY;
	}
}

void GBLcd::setSTAT(quint8 data) {
	R->STAT = (R->STAT & 0x07) | (data & 0x78);
	if (!hw.cgb && !(R->STAT & 0x02)) /* DMG STAT write bug => interrupt */
		hw_interrupt(IF_STAT, IF_STAT);
	updateStatusInt();
}

/*
	stat_trigger updates the STAT interrupt line to reflect whether any
	of the conditions set to be tested (by bits 3-6 of R_STAT) are met.
	This function should be called whenever any of the following occur:
	1) LY or LYC changes.
	2) A state transition affects the low 2 bits of R_STAT (see below).
	3) The program writes to the upper bits of R_STAT.
	stat_trigger also updates bit 2 of R_STAT to reflect whether LY=LYC.
 */
void GBLcd::updateStatusInt() {
	static const int condbits[4] = { 0x08, 0x30, 0x20, 0x00 };
	int flag = 0;

	if ((R->LY < 0x91) && (R->LY == R->LYC)) {
		R->STAT |= 0x04;
		if (R->STAT & 0x40)
			flag = IF_STAT;
	} else {
		R->STAT &= ~0x04;
	}
	if (R->STAT & condbits[R->STAT&3])
		flag = IF_STAT;

	if (!isDisplayEnabled())
		flag = 0;
	hw_interrupt(flag, IF_STAT);
}

void GBLcd::updatepalette(int i) {
	int c, r, g, b, rr, gg;

	c = (m_pal[i<<1] | ((int)m_pal[(i<<1)|1] << 8)) & 0x7FFF;
	r = (c & 0x001F) << 3;
	g = (c & 0x03E0) >> 2;
	b = (c & 0x7C00) >> 7;
	r |= (r >> 5);
	g |= (g >> 5);
	b |= (b >> 5);

//	TODO if (usefilter && (filterdmg || hw.cgb)) {
//		rr = ((r * filter[0][0] + g * filter[0][1] + b * filter[0][2]) >> 8) + filter[0][3];
//		gg = ((r * filter[1][0] + g * filter[1][1] + b * filter[1][2]) >> 8) + filter[1][3];
//		b = ((r * filter[2][0] + g * filter[2][1] + b * filter[2][2]) >> 8) + filter[2][3];
//		r = rr;
//		g = gg;
//	}

	PAL4[i] = qRgb(r,g,b);
}

void GBLcd::writePal(int i, byte b) {
	if (m_pal[i] == b)
		return;
	m_pal[i] = b;
	updatepalette(i>>1);
}

void GBLcd::writePalDmg(int i, int mapnum, quint8 d) {
	int j;
	int *cmap = dmg_pal[mapnum];
	int c, r, g, b;

	if (hw.cgb)
		return;

	/* if (mapnum >= 2) d = 0xe4; */
	for (j = 0; j < 8; j += 2) {
		c = cmap[(d >> j) & 3];
		r = (c & 0xf8) >> 3;
		g = (c & 0xf800) >> 6;
		b = (c & 0xf80000) >> 9;
		c = r|g|b;
		/* FIXME - handle directly without faking cgb */
		writePal(i+j, c & 0xff);
		writePal(i+j+1, c >> 8);
	}
}

void GBLcd::updatepatpix() {
	int i, j, k;
	int a, c;
	quint8 *vram = m_vramBanks[0];

	if (!anydirty)
		return;
	for (i = 0; i < 1024; i++) {
		if (i == 384)
			i = 512;
		if (i == 896)
			break;
		if (!patdirty[i])
			continue;
		patdirty[i] = 0;
		for (j = 0; j < 8; j++) {
			a = ((i<<4) | (j<<1));
			for (k = 0; k < 8; k++) {
				c = vram[a] & (1<<k) ? 1 : 0;
				c |= vram[a+1] & (1<<k) ? 2 : 0;
				patpix[i+1024][j][k] = c;
			}
			for (k = 0; k < 8; k++)
				patpix[i][j][k] = patpix[i+1024][j][7-k];
		}
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 8; k++) {
				patpix[i+2048][j][k] = patpix[i][7-j][k];
				patpix[i+3072][j][k] = patpix[i+1024][7-j][k];
			}
		}
	}
	anydirty = false;
}

void GBLcd::processScanline() {
	Q_ASSERT(isDisplayEnabled());

	updatepatpix();

	m_l = R->LY;
	m_x = R->SCX;
	m_y = (R->SCY + m_l) & 0xff;
	m_s = m_x >> 3;
	m_t = m_y >> 3;
	m_u = m_x & 7;
	m_v = m_y & 7;

	m_wx = R->WX - 7;
	if (m_wy>m_l || m_wy<0 || m_wy>143 || m_wx<-7 || m_wx>159 || !(R->LCDC&0x20))
		m_wx = 160;
	m_wt = (m_l - m_wy) >> 3;
	m_wv = (m_l - m_wy) & 7;

	enumSprites();

	bufferTile();
	if (hw.cgb) {
		bg_scan_color();
		wnd_scan_color();
		if (m_ns) {
			bg_scan_pri();
			wnd_scan_pri();
		}
	} else {
		scanBG();
		scanWND();
	}
	scanSPR();
	int cnt = 160;
	quint8 *src = BUF;
	dest = m_frame.scanLine(m_l);
	while (cnt--) *(dest++) = PAL4[*(src++)];
}

void GBLcd::enumSprites() {
	m_ns = 0;
	if (!(R->LCDC & 0x02))
		return;

	Sprite *sprite = m_oam.sprites;

	int pat;
	for (int i = 40; i; i--, sprite++) {
		if (m_l >= sprite->y || m_l + 16 < sprite->y)
			continue;
		if (m_l + 8 >= sprite->y && !(R->LCDC & 0x04))
			continue;
		VS[m_ns].x = sprite->x - 8;
		int v = m_l - sprite->y + 16;
		if (hw.cgb) {
			pat = sprite->pattern | ((sprite->flags & 0x60) << 5)
				| ((sprite->flags & 0x08) << 6);
			VS[m_ns].pal = 32 + ((sprite->flags & 0x07) << 2);
		} else {
			pat = sprite->pattern | ((sprite->flags & 0x60) << 5);
			VS[m_ns].pal = 32 + ((sprite->flags & 0x10) >> 2);
		}
		VS[m_ns].pri = (sprite->flags & 0x80) >> 7;
		if (R->LCDC & 0x04) {
			pat &= ~1;
			if (v >= 8) {
				v -= 8;
				pat++;
			}
			if (sprite->flags & 0x40)
				pat ^= 1;
		}
		VS[m_ns].buf = patpix[pat][v];
		if (++m_ns == 10)
			break;
	}
	if (hw.cgb)
		return;
	/* not quite optimal but it finally works! */
	VisbleSprite ts[10];
	for (int i = 0; i < m_ns; i++) {
		int l = 0;
		int x = VS[0].x;
		for (int j = 1; j < m_ns; j++) {
			if (VS[j].x < x) {
				l = j;
				x = VS[j].x;
			}
		}
		ts[i] = VS[l];
		VS[l].x = 160;
	}
	memcpy(VS, ts, sizeof(VS));
}

void GBLcd::bufferTile() {
	static const int wraptable[64] = {
		0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,-32
	};

	int base = ((R->LCDC&0x08)?0x1C00:0x1800) + (m_t<<5) + m_s;
	quint8 *tilemap = m_vramBanks[0] + base;
	quint8 *attrmap = m_vramBanks[1] + base;
	int *tilebuf = BG;
	const int *wrap = wraptable + m_s;
	int cnt = ((m_wx + 7) >> 3) + 1;

	if (hw.cgb) {
		if (R->LCDC & 0x10) {
			for (int i = cnt; i > 0; i--) {
				*(tilebuf++) = *tilemap
					| ((*attrmap & 0x08) << 6)
					| ((*attrmap & 0x60) << 5);
				*(tilebuf++) = ((*attrmap & 0x07) << 2);
				attrmap += *wrap + 1;
				tilemap += *(wrap++) + 1;
			}
		} else {
			for (int i = cnt; i > 0; i--) {
				*(tilebuf++) = 256 + qint8(*tilemap)
					| ((*attrmap & 0x08) << 6)
					| ((*attrmap & 0x60) << 5);
				*(tilebuf++) = ((*attrmap & 0x07) << 2);
				attrmap += *wrap + 1;
				tilemap += *(wrap++) + 1;
			}
		}
	} else {
		if (R->LCDC & 0x10) {
			for (int i = cnt; i > 0; i--) {
				*(tilebuf++) = *(tilemap++);
				tilemap += *(wrap++);
			}
		} else {
			for (int i = cnt; i > 0; i--) {
				*(tilebuf++) = 256 + qint8(*(tilemap++));
				tilemap += *(wrap++);
			}
		}
	}

	if (m_wx >= 160)
		return;

	base = ((R->LCDC&0x40)?0x1C00:0x1800) + (m_wt<<5);
	tilemap = m_vramBanks[0] + base;
	attrmap = m_vramBanks[1] + base;
	tilebuf = WND;
	cnt = ((160 - m_wx) >> 3) + 1;

	if (hw.cgb) {
		if (R->LCDC & 0x10) {
			for (int i = cnt; i > 0; i--) {
				*(tilebuf++) = *(tilemap++)
					| ((*attrmap & 0x08) << 6)
					| ((*attrmap & 0x60) << 5);
				*(tilebuf++) = ((*(attrmap++)&7) << 2);
			}
		} else {
			for (int i = cnt; i > 0; i--) {
				*(tilebuf++) = (256 + qint8(*(tilemap++)))
					| ((*attrmap & 0x08) << 6)
					| ((*attrmap & 0x60) << 5);
				*(tilebuf++) = ((*(attrmap++)&7) << 2);
			}
		}
	} else {
		if (R->LCDC & 0x10) {
			for (int i = cnt; i > 0; i--)
				*(tilebuf++) = *(tilemap++);
		} else {
			for (int i = cnt; i > 0; i--)
				*(tilebuf++) = 256 + qint8(*(tilemap++));
		}
	}
}

void GBLcd::scanBG() {
	int cnt;
	quint8 *src, *dest;
	int *tile;

	if (m_wx <= 0)
		return;
	cnt = m_wx;
	tile = BG;
	dest = BUF;

	src = patpix[*(tile++)][m_v] + m_u;
	memcpy(dest, src, 8-m_u);
	dest += 8-m_u;
	cnt -= 8-m_u;
	if (cnt <= 0)
		return;
	while (cnt >= 8) {
		src = patpix[*(tile++)][m_v];
		memcpy(dest, src);
		dest += 8;
		cnt -= 8;
	}
	src = patpix[*tile][m_v];
	while (cnt--)
		*(dest++) = *(src++);
}

void GBLcd::scanWND() {
	int cnt;
	quint8 *src, *dest;
	int *tile;

	if (m_wx >= 160)
		return;
	cnt = 160 - m_wx;
	tile = WND;
	dest = BUF + m_wx;

	while (cnt >= 8) {
		src = patpix[*(tile++)][m_wv];
		memcpy(dest, src);
		dest += 8;
		cnt -= 8;
	}
	src = patpix[*tile][m_wv];
	while (cnt--)
		*(dest++) = *(src++);
}

void GBLcd::scanSPR() {
	register int i, x;
	quint8 pal, b, ns = m_ns;
	quint8 *src, *dest, *bg, *pri;
	VisbleSprite *vs;

	if (!m_ns)
		return;

	memcpy(bgdup, BUF, 256);
	vs = &VS[ns-1];

	for (; ns; ns--, vs--) {
		x = vs->x;
		if (x >= 160)
			continue;
		if (x <= -8)
			continue;
		if (x < 0) {
			src = vs->buf - x;
			dest = BUF;
			i = 8 + x;
		} else {
			src = vs->buf;
			dest = BUF + x;
			if (x > 152)
				i = 160 - x;
			else
				i = 8;
		}
		pal = vs->pal;
		if (vs->pri) {
			bg = bgdup + (dest - BUF);
			while (i--) {
				b = src[i];
				if (b && !(bg[i]&3)) dest[i] = pal|b;
			}
		} else if (hw.cgb) {
			bg = bgdup + (dest - BUF);
			pri = PRI + (dest - BUF);
			while (i--) {
				b = src[i];
				if (b && (!pri[i] || !(bg[i]&3)))
					dest[i] = pal|b;
			}
		} else {
			while (i--) {
				if (src[i])
					dest[i] = pal|src[i];
			}
		}
	}
}

static void blendcpy(byte *dest, byte *src, byte b, int cnt)
{
	while (cnt--) *(dest++) = *(src++) | b;
}

static int priused(void *attr) {
	quint32 *a = attr;
	return (int)((a[0]|a[1]|a[2]|a[3]|a[4]|a[5]|a[6]|a[7])&0x80808080);
}

void GBLcd::scanBGPri() {
	int cnt, i;
	quint8 *src, *dest;

	if (m_wx <= 0)
		return;
	i = m_s;
	cnt = m_wx;
	dest = PRI;
	src = m_vramBanks[1] + ((R_LCDC&0x08)?0x1C00:0x1800) + (m_t<<5);

	if (!priused(src)) {
		memset(dest, 0, cnt);
		return;
	}

	memset(dest, src[i++&31]&128, 8-m_u);
	dest += 8-m_u;
	cnt -= 8-m_u;
	if (cnt <= 0)
		return;
	while (cnt >= 8) {
		memset(dest, src[i++&31]&128, 8);
		dest += 8;
		cnt -= 8;
	}
	memset(dest, src[i&31]&128, cnt);
}

void GBLcd::scanWNDPri() {
	int cnt, i;
	quint8 *src, *dest;

	if (m_wx >= 160)
		return;
	i = 0;
	cnt = 160 - m_wx;
	dest = PRI + m_wx;
	src = m_vramBanks[1] + ((R->LCDC&0x40)?0x1C00:0x1800) + (m_wt<<5);

	if (!priused(src)) {
		memset(dest, 0, cnt);
		return;
	}

	while (cnt >= 8) {
		memset(dest, src[i++]&128, 8);
		dest += 8;
		cnt -= 8;
	}
	memset(dest, src[i]&128, cnt);
}

void GBLcd::scanBGColor() {
	int cnt;
	quint8 *src, *dest;
	int *tile;

	if (m_wx <= 0) return;
	cnt = m_wx;
	tile = BG;
	dest = BUF;

	src = patpix[*(tile++)][m_v] + m_u;
	blendcpy(dest, src, *(tile++), 8-m_u);
	dest += 8-m_u;
	cnt -= 8-m_u;
	if (cnt <= 0)
		return;
	while (cnt >= 8) {
		src = patpix[*(tile++)][m_v];
		blendcpy(dest, src, *(tile++), 8);
		dest += 8;
		cnt -= 8;
	}
	src = patpix[*(tile++)][m_v];
	blendcpy(dest, src, *(tile++), cnt);
}

void GBLcd::scanWNDColor() {
	int cnt;
	quint8 *src, *dest;
	int *tile;

	if (WX >= 160)
		return;
	cnt = 160 - m_wx;
	tile = WND;
	dest = BUF + m_wx;

	while (cnt >= 8) {
		src = patpix[*(tile++)][m_wv];
		blendcpy(dest, src, *(tile++), 8);
		dest += 8;
		cnt -= 8;
	}
	src = patpix[*(tile++)][m_wv];
	blendcpy(dest, src, *(tile++), cnt);
}

quint8 GBLcd::m_invertTab[256] = {
  0x00,0x80,0x40,0xc0,0x20,0xa0,0x60,0xe0,
  0x10,0x90,0x50,0xd0,0x30,0xb0,0x70,0xf0,
  0x08,0x88,0x48,0xc8,0x28,0xa8,0x68,0xe8,
  0x18,0x98,0x58,0xd8,0x38,0xb8,0x78,0xf8,
  0x04,0x84,0x44,0xc4,0x24,0xa4,0x64,0xe4,
  0x14,0x94,0x54,0xd4,0x34,0xb4,0x74,0xf4,
  0x0c,0x8c,0x4c,0xcc,0x2c,0xac,0x6c,0xec,
  0x1c,0x9c,0x5c,0xdc,0x3c,0xbc,0x7c,0xfc,
  0x02,0x82,0x42,0xc2,0x22,0xa2,0x62,0xe2,
  0x12,0x92,0x52,0xd2,0x32,0xb2,0x72,0xf2,
  0x0a,0x8a,0x4a,0xca,0x2a,0xaa,0x6a,0xea,
  0x1a,0x9a,0x5a,0xda,0x3a,0xba,0x7a,0xfa,
  0x06,0x86,0x46,0xc6,0x26,0xa6,0x66,0xe6,
  0x16,0x96,0x56,0xd6,0x36,0xb6,0x76,0xf6,
  0x0e,0x8e,0x4e,0xce,0x2e,0xae,0x6e,0xee,
  0x1e,0x9e,0x5e,0xde,0x3e,0xbe,0x7e,0xfe,
  0x01,0x81,0x41,0xc1,0x21,0xa1,0x61,0xe1,
  0x11,0x91,0x51,0xd1,0x31,0xb1,0x71,0xf1,
  0x09,0x89,0x49,0xc9,0x29,0xa9,0x69,0xe9,
  0x19,0x99,0x59,0xd9,0x39,0xb9,0x79,0xf9,
  0x05,0x85,0x45,0xc5,0x25,0xa5,0x65,0xe5,
  0x15,0x95,0x55,0xd5,0x35,0xb5,0x75,0xf5,
  0x0d,0x8d,0x4d,0xcd,0x2d,0xad,0x6d,0xed,
  0x1d,0x9d,0x5d,0xdd,0x3d,0xbd,0x7d,0xfd,
  0x03,0x83,0x43,0xc3,0x23,0xa3,0x63,0xe3,
  0x13,0x93,0x53,0xd3,0x33,0xb3,0x73,0xf3,
  0x0b,0x8b,0x4b,0xcb,0x2b,0xab,0x6b,0xeb,
  0x1b,0x9b,0x5b,0xdb,0x3b,0xbb,0x7b,0xfb,
  0x07,0x87,0x47,0xc7,0x27,0xa7,0x67,0xe7,
  0x17,0x97,0x57,0xd7,0x37,0xb7,0x77,0xf7,
  0x0f,0x8f,0x4f,0xcf,0x2f,0xaf,0x6f,0xef,
  0x1f,0x9f,0x5f,0xdf,0x3f,0xbf,0x7f,0xff
};
