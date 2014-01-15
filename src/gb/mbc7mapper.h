#ifndef MBC7MAPPER_H
#define MBC7MAPPER_H

#include "gbcpumapper.h"

class MBC7Mapper : public GBCpuMapper {
	Q_OBJECT
public:
	void reset();

	void writeLow(quint16 address, quint8 data);

	void writeExRam(quint16 address, quint8 data);
	quint8 readExRam(quint16 address);
private:
	bool m_ramEnable;
	int m_romBank;
	int m_ramBank;
	int m_ramAddress;
	int m_chipSelect;
	int m_sk;
	int m_state;
	int m_buffer;
	bool m_idle;
	int m_count;
	int m_code;
	int m_address;
	bool m_writeEnable;
	int m_retValue;
};

#endif // MBC7MAPPER_H
