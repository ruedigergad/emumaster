#ifndef MBC3MAPPER_H
#define MBC3MAPPER_H

#include "gbcpumapper.h"

class MBC3Mapper : public GBCpuMapper {
	Q_OBJECT
public:
	void reset();

	void writeLow(quint16 address, quint8 data);

	void writeExRam(quint16 address, quint8 data);
	quint8 readExRam(quint16 address);
private:
	void updateClock();

	bool m_ramEnable;
	int m_romBank;
	int m_ramBank;
	int m_ramAddress;

	int m_clockLatch;
	int m_clockRegister;

	int m_seconds;
	int m_minutes;
	int m_hours;
	int m_days;
	int m_control;

	int m_latchSeconds;
	int m_latchMinutes;
	int m_latchHours;
	int m_latchDays;
	int m_latchControl;

	time_t m_lastTime;
};

#endif // MBC3MAPPER_H
