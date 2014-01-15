#ifndef GS3MAPPER_H
#define GS3MAPPER_H

#include "gbcpumapper.h"

class GS3Mapper : public GBCpuMapper {
	Q_OBJECT
public:
	void reset();

	void writeLow(quint16 address, quint8 data);
private:
	int m_romBank;
};

#endif // GS3MAPPER_H
