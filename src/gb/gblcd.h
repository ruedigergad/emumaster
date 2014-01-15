#ifndef GBLCD_H
#define GBLCD_H

#include <QObject>
#include <QImage>

class GBLcd : public QObject {
    Q_OBJECT
public:
    explicit GBLcd(QObject *parent = 0);
	void reset();
	void emulate(int cycles);
	bool isDisplayEnabled() const;

	void writeVram(quint8 address, quint8 data);
private:
	void emulate();
	void setLCDC(quint8 data);
	void setSTAT(quint8 data);
	void setStatusIntBits(quint8 newStatus);
	void updateStatusInt();
	void enumSprites();

	void bufferTile();
	void scanBG();
	void scanWND();
	void scanSPR();

	void scanBGPri();
	void scanWNDPri();

	void scanBGColor();
	void scanWNDColor();

	void updatepalette(int i);
	void writePal(int i, byte b);
	void writePalDmg(int i, int mapnum, quint8 d);

	void hw_dma(quint8 b);
	void hw_hdma();
	void hw_hdma_cmd(quint8 c);

	class Sprite {
	public:
		quint8 x;
		quint8 y;
		quint8 pattern;
		quint8 flags;
	};

	class VisbleSprite {
	public:
		quint8 *buf;
		int x;
		quint8 pal, pri, pad[6];
	};

	Sprite *oam();

	struct { // 0xFF40 registers
		quint8 LCDC;
		quint8 STAT;
		quint8 SCY;
		quint8 SCX;
		quint8 LY;
		quint8 LYC;
		quint8 DMA;
		quint8 BGP;
		quint8 OBP0;
		quint8 OBP1;
		quint8 WY;
		quint8 WX;
	} *R;

	struct { // 0xFF51 registers
		quint8 HDMA1;
		quint8 HDMA2;
		quint8 HDMA3;
		quint8 HDMA4;
		quint8 HDMA5;
	} *RH;

	int m_cycles;

	quint8 m_vramBanks[2][0x2000];
	union {
		Sprite sprites[40];
		quint8 ram[0x100];
	} m_oam;
	quint8 m_pal[128];

	quint8 patpix[4096][8][8];
	bool patdirty[1024];
	bool anydirty;
	int m_ns, m_l, m_x, m_y, m_s, m_t, m_u, m_v, m_wx, m_wy, m_wt, m_wv;
	VisbleSprite VS[16];
	int BG[64];
	int WND[64];
	quint8 BUF[256];
	quint8 bgdup[256];
	quint8 PRI[256];
	QRgb PAL4[64];

	QImage m_frame;

	static const quint8 m_invertTab[256];
};

inline GBLcd::isDisplayEnabled() const
{ return R->LCDC & 0x80; }
inline GBLcd::emulate(int cycles) {
	m_cycles -= cycles;
	if (m_cycles <= 0)
		emulate();
}

inline void GBLcd::writeVram(quint8 address, quint8 data) {
	m_vramBanks[R->VBK&1][address] = data;
	if (address >= 0x1800) return;
	patdirty[((R->VBK&1)<<9)+(address>>4)] = 1;
	anydirty = true;
}

#endif // GBLCD_H
