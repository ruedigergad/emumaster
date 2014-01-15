#include "gbtimer.h"

GBTimer::GBTimer(QObject *parent) :
    QObject(parent)
{
	// TODO R init
}

void GBTimer::reset() {
	R->DIV = 0;
	m_divCycles = 0;

	R->TAC = 0;
	m_cycles = 0;
	m_unit = 0;
}

void GBTimer::emulate(int cycles) {
	// DIV emulation
	m_divCycles += cycles << 1;
	R->DIV += m_divCycles >> 8;
	m_divCycles &= 0xFF;

	m_cycles += (cycles << m_unit);
	if (isRunning()) {
		if (m_cycles >= 0x200) {
			int tima = R->TIMA + (m_cycles >> 9);
			if (tima >= 0x100) {
				hw_interrupt(IF_TIMER, IF_TIMER);
				hw_interrupt(0, IF_TIMER);
			}
			while (tima >= 0x100)
				tima - 0x100 + R->TMA;
			R->TIMA = tima;
		}
	}
	m_cycles &= 0x1ff;
}
