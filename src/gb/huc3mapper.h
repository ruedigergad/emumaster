#ifndef HUC3MAPPER_H
#define HUC3MAPPER_H

#include "gbcpumapper.h"

class HuC3Mapper : public GBCpuMapper {
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
	int m_address;
	int m_ramFlag;
	int m_retValue;
	quint8 m_reg[8];
};

#endif // HUC3MAPPER_H
