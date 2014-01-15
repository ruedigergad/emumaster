#ifndef HUC1MAPPER_H
#define HUC1MAPPER_H

#include "gbcpumapper.h"

class HuC1Mapper : public GBCpuMapper {
	Q_OBJECT
public:
	void reset();

	void writeLow(quint16 address, quint8 data);

	void writeExRam(quint16 address, quint8 data);
private:
	bool m_ramEnable;
	int m_romBank;
	int m_ramBank;
	int m_memoryModel;
	int m_romHighAddress;
	int m_ramAddress;
};

#endif // HUC1MAPPER_H
