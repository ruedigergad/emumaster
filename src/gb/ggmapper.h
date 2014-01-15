#ifndef GGMAPPER_H
#define GGMAPPER_H

#include "gbcpumapper.h"

class GGMapper : public GBCpuMapper {
	Q_OBJECT
public:
	void writeLow(quint16 address, quint8 data);
};

#endif // GGMAPPER_H
