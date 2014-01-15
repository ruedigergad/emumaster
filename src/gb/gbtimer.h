#ifndef GBTIMER_H
#define GBTIMER_H

#include <QObject>

class GBTimer : public QObject {
	Q_OBJECT
public:
	explicit GBTimer(QObject *parent = 0);
	void reset();

	void emulate(int cycles);

	bool isRunning() const;
	int cyclesToNextIrq() const;

	void write(quint8 address, quint8 data);
	quint8 read(quint8 address);
signals:
	void irq_o(bool on);
private:
	struct {			// 0xFF04
		quint8 DIV;		// Divider Register
		quint8 TIMA;	// Timer Counter
		quint8 TMA;		// Timer Modulo
		quint8 TAC;		// Timer Control
	} *R;
	int m_divCycles;
	int m_cycles;
	int m_unit;
};

inline bool GBTimer::isRunning() const
{ return R->TAC & 0x04; }
inline int GBTimer::cyclesToNextIrq() const {
	Q_ASSERT(isRunning());
	int result = (511 - m_cycles + (1<<m_unit)) >> m_unit;
	result += (255 - R->TIMA) << (9 - m_unit);
	return result;
}

inline void GBTimer::write(quint8 address, quint8 data) {
	switch (address) {
	case 0x04: R->DIV = 0; break;
	case 0x05: R->TIMA = data; break;
	case 0x06: R->TMA = data; break;
	case 0x07: R->TAC = data; m_unit = ((-data) & 0x03) << 1; break;
	default: Q_ASSERT(false); break;
	}
}

inline quint8 GBTimer::read(quint8 address) {
	switch (address) {
	case 0x04: return R->DIV; break;
	case 0x05: R->TIMA = data; break;
	case 0x06: R->TMA = data; break;
	case 0x07: R->TAC = data; m_unit = ((-data) & 0x03) << 1; break;
	default: Q_ASSERT(false); break;
	}
}

#endif // GBTIMER_H
