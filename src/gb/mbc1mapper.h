#ifndef MBC1MAPPER_H
#define MBC1MAPPER_H

#include "gbcpumapper.h"

class MBC1Mapper : public GBCpuMapper {
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
	int m_memoryModel;
	int m_romHighAddress;
	int m_ramAddress;
	int m_romBank0Remapping;
};

#endif // MBC1MAPPER_H
