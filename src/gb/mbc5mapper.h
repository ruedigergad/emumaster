#ifndef MBC5MAPPER_H
#define MBC5MAPPER_H

#include "gbcpumapper.h"

class MBC5Mapper : public GBCpuMapper {
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
	int m_romHighAddress;
	int m_ramAddress;
	bool m_rumble;
};

#endif // MBC5MAPPER_H
