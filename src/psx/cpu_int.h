#ifndef PSXCPUINT_H
#define PSXCPUINT_H

#include "cpu.h"

class PsxCpuInt : public PsxCpu {
public:
	bool init();
};

extern PsxCpuInt psxInt;

#endif // PSXCPUINT_H
