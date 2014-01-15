#ifndef MBC2MAPPER_H
#define MBC2MAPPER_H

#include "gbcpumapper.h"

class MBC2Mapper : public GBCpuMapper {
	Q_OBJECT
public:
	void reset();

	void writeLow(quint16 address, quint8 data);

	void writeExRam(quint16 address, quint8 data);
private:
	bool m_ramEnable;
	int m_romBank;
};

#endif // MBC2MAPPER_H
