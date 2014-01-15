#include "gbcpu.h"

GBCpu::GBCpu(QObject *parent) :
	QObject(parent) {

}

void GBCpu::reset() {
	m_speed = 0;
	m_halt = false;
	m_timer.reset();

	DI;
	PC = 0x0100;
	SP = 0xFFFE;
	AF = 0x01B0;
	BC = 0x0013;
	DE = 0x00D8;
	HL = 0x014D;

	if (hw.cgb)
		A = 0x11;
	if (hw.gba)
		B = 0x01;
}

inline int GBCpu::idle(int maxCycles) {
	if (!(m_halt && IME))
		return 0;
	if (*IF & *IE) {
		m_halt = false;
		return 0;
	}
	/* Make sure we don't miss lcdc status events! */
	if (*IE & (VBlankIrq | LcdStatIrq))
		maxCycles = qMax(maxCycles, cpu.lcdc);

	/* If timer interrupt cannot happen, this is very simple! */
	if (!((*IE & TimerIrq) && m_timer.isRunning())) {
		emulateTimers(maxCycles);
		return maxCycles;
	}
	/* Figure out when the next timer interrupt will happen */
	int cyclesLeft = qMin(m_timer.cyclesToNextIrq(), maxCycles);
	emulateTimers(cyclesLeft);
	return cyclesLeft;
}

int GBCpu::emulate(int maxCycles) {
	Q_ASSERT(maxCycles > 0);
	int executedCycles = 0;
	do {
		for (int i = idle(maxCycles); i > 0; i = idle(maxCycles)) {
			executedCycles += i;
			if (maxCycles <= executedCycles)
				return maxCycles;
		}
		int cycles = executeoOne();
		emulateTimers(cycles);
		executedCycles += cycles;
	} while (maxCycles > executedCycles);
	return executedCycles;
}

inline void GBCpu::emulateTimers(int cycles) {
	m_timer.emulate(cycles << m_speed);
	m_lcdc.emulate(cycles);
	m_apu.emulate(cycles);
}
