/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#ifndef PSXCOMMON_H
#define PSXCOMMON_H

// System includes
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <assert.h>

// Define types
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef intptr_t sptr;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;

typedef uint8_t boolean;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// Local includes
static inline void SysPrintf(const char *fmt, ...)
{ (void)fmt; }	// Printf used by bios syscalls

#include "debug.h"

#define strnicmp strncasecmp
#define __inline inline

#ifdef __cplusplus
#include <base/emu.h>
extern "C" {
#endif

typedef struct {
	boolean Xa;
	boolean Sio;
	boolean Mdec;
	boolean PsxAuto;
	boolean Cdda;
	boolean HLE;
	boolean Debug;
	boolean PsxOut;
	boolean SpuIrq;
	boolean RCntFix;
	boolean VSyncWA;
	boolean CdrReschedule;
} PcsxConfig;

extern PcsxConfig Config;

// Make the timing events trigger faster as we are currently assuming everything
// takes one cycle, which is not the case on real hardware.
// FIXME: Count the proper cycle and get rid of this
#define BIAS	2
#define PSXCLK	33868800	/* 33.8688 MHz */

#ifdef __cplusplus
}
#endif

#endif // PSXCOMMON_H
