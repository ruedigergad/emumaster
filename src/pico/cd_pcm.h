/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#ifndef PICOMCDPCM_H
#define PICOMCDPCM_H

#include <base/emu.h>

#define PCM_STEP_SHIFT 11

extern "C" {
void picoMcdPcmWrite(uint a, u8 d);
void picoMcdPcmSetRate(int rate);
void picoMcdPcmUpdate(int *buffer, int length);
}

#endif // PICOMCDPCM_H
