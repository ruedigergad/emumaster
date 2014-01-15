#ifndef GBCPU_H
#define GBCPU_H

#include <QObject>
#include <QtEndian>

class GBCpu : public QObject {
    Q_OBJECT
public:
	/* flags */
	enum Flags {
		FZ = 0x80,
		FN = 0x40,
		FH = 0x20,
		FC = 0x10,
		FL = 0x0F /* low unused portion of flags */
	};

	enum Irq {
		VBlankIrq		= 0x01,
		LcdStatIrq		= 0x02,
		TimerIrq		= 0x04,
		SerialIrq		= 0x08,
		JoyPadIrq		= 0x10
	};

    explicit GBCpu(QObject *parent = 0);
	void reset();
	int emulate(int maxCycles);
	int executeoOne();
	quint8 *highRam();
	bool isHalted() const;
private:
	int idle(int maxCycles);
	void emulateTimers(int cycles);

	typedef union {
		struct {
#		if Q_BYTE_ORDER == Q_BIG_ENDIAN
			quint8 hi,lo;
#		else
			quint8 lo,hi;
#		endif
		} b;
		quint16 w;
		quint32 d;
	} GBCpuRegister;

	GBCpuRegister pc;
	GBCpuRegister sp;
	GBCpuRegister af;
	GBCpuRegister bc;
	GBCpuRegister de;
	GBCpuRegister hl;

	bool IME;
	bool IMA;
	bool m_halt;

	quint8 *IE;		// 0xFFFF
	quint8 *IF;		// 0xFF0F
	quint8 *KEY1;	// 0xFF4D

	int m_instrCycles;
	int m_speed;

	quint8 m_highRam[0x100];

	static const int CyclesTable[256];
	static const int CyclesTableCB[256];
	static const quint8 IncFlagTable[256];
	static const quint8 DecFlagTable[256];
	static const quint8 SwapTable[256];
	static const quint16 DAATable[2048];
};

inline quint8 *GBCpu::highRam()
{ return m_highRam; }

inline bool GBCpu::isHalted() const
{ return m_halt; }

#define ADDCYC(n) m_instrCycles += (n)

#define EI ( IMA = 1 )
#define DI ( m_halt = IMA = IME = 0 )

#endif // GBCPU_H
