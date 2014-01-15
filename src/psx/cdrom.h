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

#ifndef PSXCDROM_H
#define PSXCDROM_H

#include "common.h"
#include "decode_xa.h"

struct SubQ {
	char res0[12];
	u8 ControlAndADR;
	u8 TrackNumber;
	u8 IndexNumber;
	u8 TrackRelativeAddress[3];
	u8 Filler;
	u8 AbsoluteAddress[3];
	u8 CRC[2];
	char res1[72];
};

struct CdrStat {
	u32 Type;
	u32 Status;
	u8 Time[3];
};

typedef long (* CDRinit)(void);
typedef long (* CDRshutdown)(void);
typedef long (* CDRopen)(void);
typedef long (* CDRclose)(void);
typedef long (* CDRgetTN)(unsigned char *);
typedef long (* CDRgetTD)(unsigned char, unsigned char *);
typedef long (* CDRreadTrack)(unsigned char *);
typedef unsigned char* (* CDRgetBuffer)(void);
typedef unsigned char* (* CDRgetBufferSub)(void);
typedef long (* CDRplay)(unsigned char *);
typedef long (* CDRstop)(void);
typedef long (* CDRgetStatus)(struct CdrStat *);

extern CDRinit               CDR_init;
extern CDRshutdown           CDR_shutdown;
extern CDRopen               CDR_open;
extern CDRclose              CDR_close;
extern CDRgetTN              CDR_getTN;
extern CDRgetTD              CDR_getTD;
extern CDRreadTrack          CDR_readTrack;
extern CDRgetBuffer          CDR_getBuffer;
extern CDRgetBufferSub       CDR_getBufferSub;
extern CDRplay               CDR_play;
extern CDRstop               CDR_stop;
extern CDRgetStatus          CDR_getStatus;

void SetIsoFile(const char *filename);
const char *GetIsoFile(void);
boolean UsingIso(void);
void SetCdOpenCaseTime(s64 time);

#define btoi(b)     ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i)     ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */

#define MSF2SECT(m, s, f)		(((m) * 60 + (s) - 2) * 75 + (f))

#define CD_FRAMESIZE_RAW		2352
#define DATA_SIZE				(CD_FRAMESIZE_RAW - 12)

#define SUB_FRAMESIZE			96

#ifdef __cplusplus

class PsxCdr {
public:
	void reset();
	void sl();

	unsigned char OCUP;
	unsigned char Reg1Mode;
	unsigned char Reg2;
	unsigned char CmdProcess;
	unsigned char Ctrl;
	unsigned char Stat;

	unsigned char StatP;

	unsigned char Transfer[CD_FRAMESIZE_RAW];
	unsigned char *pTransfer;

	unsigned char Prev[4];
	unsigned char Param[8];
	unsigned char Result[16];

	unsigned char ParamC;
	unsigned char ParamP;
	unsigned char ResultC;
	unsigned char ResultP;
	unsigned char ResultReady;
	unsigned char Cmd;
	unsigned char Readed;
	u32 Reading;

	unsigned char ResultTN[6];
	unsigned char ResultTD[4];
	unsigned char SetSector[4];
	unsigned char SetSectorSeek[4];
	unsigned char SetSectorPlay[4];
	unsigned char Track;
	boolean Play, Muted;
	int CurTrack;
	int Mode, File, Channel;
	int Reset;
	int RErr;
	int FirstSector;

	xa_decode_t Xa;

	int Init;

	unsigned char Irq;
	u32 eCycle;

	boolean Seeked;

	u8 LidCheck;
	u8 FastForward;
	u8 FastBackward;

	u8 AttenuatorLeft[2], AttenuatorRight[2];
};

extern PsxCdr psxCdr;

extern "C" {
#endif

void cdrDecodedBufferInterrupt();

void cdrInterrupt();
void cdrReadInterrupt();
void cdrRepplayInterrupt();
void cdrLidSeekInterrupt();
void cdrPlayInterrupt();
void cdrDmaInterrupt();
void LidInterrupt();
unsigned char cdrRead0(void);
unsigned char cdrRead1(void);
unsigned char cdrRead2(void);
unsigned char cdrRead3(void);
void cdrWrite0(unsigned char rt);
void cdrWrite1(unsigned char rt);
void cdrWrite2(unsigned char rt);
void cdrWrite3(unsigned char rt);

#ifdef __cplusplus
}
#endif

#endif // PSXCDROM_H
