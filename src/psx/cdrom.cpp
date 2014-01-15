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

/* 
* Handles all CD-ROM registers and functions.
*/

#include "cdrom.h"
#include "ppf.h"
#include "dma.h"
#include "spu.h"
#include "misc.h"
#include <QDataStream>

/* CD-ROM magic numbers */
#define CdlSync        0
#define CdlNop         1
#define CdlSetloc      2
#define CdlPlay        3
#define CdlForward     4
#define CdlBackward    5
#define CdlReadN       6
#define CdlStandby     7
#define CdlStop        8
#define CdlPause       9
#define CdlInit        10
#define CdlMute        11
#define CdlDemute      12
#define CdlSetfilter   13
#define CdlSetmode     14
#define CdlGetmode     15
#define CdlGetlocL     16
#define CdlGetlocP     17
#define CdlReadT       18
#define CdlGetTN       19
#define CdlGetTD       20
#define CdlSeekL       21
#define CdlSeekP       22
#define CdlSetclock    23
#define CdlGetclock    24
#define CdlTest        25
#define CdlID          26
#define CdlReadS       27
#define CdlReset       28
#define CdlReadToc     30

#define AUTOPAUSE      249
#define READ_ACK       250
#define READ           251
#define REPPLAY_ACK    252
#define REPPLAY        253
#define ASYNC          254
/* don't set 255, it's reserved */

CDRinit               CDR_init;
CDRshutdown           CDR_shutdown;
CDRopen               CDR_open;
CDRclose              CDR_close;
CDRgetTN              CDR_getTN;
CDRgetTD              CDR_getTD;
CDRreadTrack          CDR_readTrack;
CDRgetBuffer          CDR_getBuffer;
CDRplay               CDR_play;
CDRstop               CDR_stop;
CDRgetStatus          CDR_getStatus;
CDRgetBufferSub       CDR_getBufferSub;

static const char *CmdName[0x100]= {
    "CdlSync",     "CdlNop",       "CdlSetloc",  "CdlPlay",
    "CdlForward",  "CdlBackward",  "CdlReadN",   "CdlStandby",
    "CdlStop",     "CdlPause",     "CdlInit",    "CdlMute",
    "CdlDemute",   "CdlSetfilter", "CdlSetmode", "CdlGetmode",
    "CdlGetlocL",  "CdlGetlocP",   "CdlReadT",   "CdlGetTN",
    "CdlGetTD",    "CdlSeekL",     "CdlSeekP",   "CdlSetclock",
    "CdlGetclock", "CdlTest",      "CdlID",      "CdlReadS",
    "CdlReset",    NULL,           "CDlReadToc", NULL
};

static const u8 Test04[] = { 0 };
static const u8 Test05[] = { 0 };
static const u8 Test20[] = { 0x98, 0x06, 0x10, 0xC3 };
static const u8 Test22[] = { 0x66, 0x6F, 0x72, 0x20, 0x45, 0x75, 0x72, 0x6F };
static const u8 Test23[] = { 0x43, 0x58, 0x44, 0x32, 0x39 ,0x34, 0x30, 0x51 };

#define NoIntr		0
#define DataReady	1
#define Complete	2
#define Acknowledge	3
#define DataEnd		4
#define DiskError	5

/* Modes flags */
#define MODE_SPEED       (1<<7) // 0x80
#define MODE_STRSND      (1<<6) // 0x40 ADPCM on/off
#define MODE_SIZE_2340   (1<<5) // 0x20
#define MODE_SIZE_2328   (1<<4) // 0x10
#define MODE_SF          (1<<3) // 0x08 channel on/off
#define MODE_REPORT      (1<<2) // 0x04
#define MODE_AUTOPAUSE   (1<<1) // 0x02
#define MODE_CDDA        (1<<0) // 0x01

/* Status flags */
#define STATUS_PLAY      (1<<7) // 0x80
#define STATUS_SEEK      (1<<6) // 0x40
#define STATUS_READ      (1<<5) // 0x20
#define STATUS_SHELLOPEN (1<<4) // 0x10
#define STATUS_UNKNOWN3  (1<<3) // 0x08
#define STATUS_UNKNOWN2  (1<<2) // 0x04
#define STATUS_ROTATING  (1<<1) // 0x02
#define STATUS_ERROR     (1<<0) // 0x01



// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) = cdr read time (linuzappz)
#define cdReadTime (PSXCLK / 75)

static struct CdrStat stat;

static unsigned int msf2sec(char *msf) {
	return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
}

static void sec2msf(unsigned int s, char *msf) {
	msf[0] = s / 75 / 60;
	s = s - msf[0] * 75 * 60;
	msf[1] = s / 75;
	s = s - msf[1] * 75;
	msf[2] = s;
}


#define CDR_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDR); \
	psxRegs.intCycle[PSXINT_CDR].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDR].sCycle = psxRegs.cycle; \
	new_dyna_set_event(PSXINT_CDR, eCycle); \
}

#define CDREAD_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDREAD); \
	psxRegs.intCycle[PSXINT_CDREAD].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDREAD].sCycle = psxRegs.cycle; \
	new_dyna_set_event(PSXINT_CDREAD, eCycle); \
}

#define CDRLID_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRLID); \
	psxRegs.intCycle[PSXINT_CDRLID].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRLID].sCycle = psxRegs.cycle; \
	new_dyna_set_event(PSXINT_CDRLID, eCycle); \
}

#define CDRPLAY_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRPLAY); \
	psxRegs.intCycle[PSXINT_CDRPLAY].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRPLAY].sCycle = psxRegs.cycle; \
	new_dyna_set_event(PSXINT_CDRPLAY, eCycle); \
}

#define StartReading(type, eCycle) { \
	psxCdr.Reading = type; \
	psxCdr.FirstSector = 1; \
	psxCdr.Readed = 0xff; \
	AddIrqQueue(READ_ACK, eCycle); \
}

#define StopReading() { \
	if (psxCdr.Reading) { \
		psxCdr.Reading = 0; \
		psxRegs.interrupt &= ~(1 << PSXINT_CDREAD); \
	} \
	psxCdr.StatP &= ~STATUS_READ;\
}

#define StopCdda() { \
	if (psxCdr.Play) { \
		if (!Config.Cdda) CDR_stop(); \
		psxCdr.StatP &= ~STATUS_PLAY; \
		psxCdr.Play = FALSE; \
		psxCdr.FastForward = 0; \
		psxCdr.FastBackward = 0; \
	} \
}

#define SetResultSize(size) { \
	psxCdr.ResultP = 0; \
	psxCdr.ResultC = size; \
	psxCdr.ResultReady = 1; \
}


void cdrLidSeekInterrupt()
{
	// turn back on checking
	if( psxCdr.LidCheck == 0x10 )
	{
		psxCdr.LidCheck = 0;
	}

	// official lid close
	else if( psxCdr.LidCheck == 0x30 )
	{
		// GS CDX 3.3: $13
		psxCdr.StatP |= STATUS_ROTATING;


		// GS CDX 3.3 - ~50 getlocp tries
		CDRLID_INT( cdReadTime * 3 );
		psxCdr.LidCheck = 0x40;
	}

	// turn off ready
	else if( psxCdr.LidCheck == 0x40 )
	{
		// GS CDX 3.3: $01
		psxCdr.StatP &= ~STATUS_SHELLOPEN;
		psxCdr.StatP &= ~STATUS_ROTATING;


		// GS CDX 3.3 - ~50 getlocp tries
		CDRLID_INT( cdReadTime * 3 );
		psxCdr.LidCheck = 0x50;
	}

	// now seek
	else if( psxCdr.LidCheck == 0x50 )
	{
		// GameShark Lite: Start seeking ($42)
		psxCdr.StatP |= STATUS_SEEK;
		psxCdr.StatP |= STATUS_ROTATING;
		psxCdr.StatP &= ~STATUS_ERROR;


		CDRLID_INT( cdReadTime * 3 );
		psxCdr.LidCheck = 0x60;
	}

	// done = cd ready
	else if( psxCdr.LidCheck == 0x60 )
	{
		// GameShark Lite: Seek detection done ($02)
		psxCdr.StatP &= ~STATUS_SEEK;

		psxCdr.LidCheck = 0;
	}
}


static void Check_Shell( int Irq )
{
	// check case open/close
	if (psxCdr.LidCheck > 0)
	{
#ifdef CDR_LOG
		CDR_LOG( "LidCheck\n" );
#endif

		// $20 = check lid state
		if( psxCdr.LidCheck == 0x20 )
		{
			u32 i;

			i = stat.Status;
			if (CDR_getStatus(&stat) != -1)
			{
				// BIOS hangs + BIOS error messages
				//if (stat.Type == 0xff)
					//psxCdr.Stat = DiskError;

				// case now open
				if (stat.Status & STATUS_SHELLOPEN)
				{
					// Vib Ribbon: pre-CD swap
					StopCdda();


					// GameShark Lite: Death if DiskError happens
					//
					// Vib Ribbon: Needs DiskError for CD swap

					if (Irq != CdlNop)
					{
						psxCdr.Stat = DiskError;

						psxCdr.StatP |= STATUS_ERROR;
						psxCdr.Result[0] |= STATUS_ERROR;
					}

					// GameShark Lite: Wants -exactly- $10
					psxCdr.StatP |= STATUS_SHELLOPEN;
					psxCdr.StatP &= ~STATUS_ROTATING;


					CDRLID_INT( cdReadTime * 3 );
					psxCdr.LidCheck = 0x10;


					// GS CDX 3.3 = $11
				}

				// case just closed
				else if ( i & STATUS_SHELLOPEN )
				{
					psxCdr.StatP |= STATUS_ROTATING;

					CheckCdrom();


					if( psxCdr.Stat == NoIntr )
						psxCdr.Stat = Acknowledge;

					psxHu32ref(0x1070) |= SWAP32((u32)0x4);


					// begin close-seek-ready cycle
					CDRLID_INT( cdReadTime * 3 );
					psxCdr.LidCheck = 0x30;


					// GameShark Lite: Wants -exactly- $42, then $02
					// GS CDX 3.3: Wants $11/$80, $13/$80, $01/$00
				}

				// case still closed - wait for recheck
				else
				{
					CDRLID_INT( cdReadTime * 3 );
					psxCdr.LidCheck = 0x10;
				}
			}
		}


		// GS CDX: clear all values but #1,#2
		if( (psxCdr.LidCheck >= 0x30) || (psxCdr.StatP & STATUS_SHELLOPEN) )
		{
			SetResultSize(16);
			memset( psxCdr.Result, 0, 16 );

			psxCdr.Result[0] = psxCdr.StatP;


			// GS CDX: special return value
			if( psxCdr.StatP & STATUS_SHELLOPEN )
			{
				psxCdr.Result[1] = 0x80;
			}


			if( psxCdr.Stat == NoIntr )
				psxCdr.Stat = Acknowledge;

			psxHu32ref(0x1070) |= SWAP32((u32)0x4);
		}
	}
}


void Find_CurTrack() {
	psxCdr.CurTrack = 0;

	if (CDR_getTN(psxCdr.ResultTN) != -1) {
		int lcv;

		for( lcv = 1; lcv <= psxCdr.ResultTN[1]; lcv++ ) {
			if (CDR_getTD((u8)(lcv), psxCdr.ResultTD) != -1) {
				u32 sect1, sect2;

#ifdef CDR_LOG___0
				CDR_LOG( "curtrack %d %d %d | %d %d %d | %d\n",
					psxCdr.SetSectorPlay[0], psxCdr.SetSectorPlay[1], psxCdr.SetSectorPlay[2],
					psxCdr.ResultTD[2], psxCdr.ResultTD[1], psxCdr.ResultTD[0],
					psxCdr.CurTrack );
#endif

				// find next track boundary - only need m:s accuracy
				sect1 = psxCdr.SetSectorPlay[0] * 60 * 75 + psxCdr.SetSectorPlay[1] * 75;
				sect2 = psxCdr.ResultTD[2] * 60 * 75 + psxCdr.ResultTD[1] * 75;

				// Twisted Metal 4 - psx cdda pregap (2-sec)
				// - fix in-game music
				sect2 -= 75 * 2;

				if( sect1 >= sect2 ) {
					psxCdr.CurTrack++;
					continue;
				}
			}

			break;
		}
	}
}

static void ReadTrack( u8 *time ) {
	psxCdr.Prev[0] = itob( time[0] );
	psxCdr.Prev[1] = itob( time[1] );
	psxCdr.Prev[2] = itob( time[2] );

#ifdef CDR_LOG
	CDR_LOG("ReadTrack() Log: KEY *** %x:%x:%x\n", psxCdr.Prev[0], psxCdr.Prev[1], psxCdr.Prev[2]);
#endif
	psxCdr.RErr = CDR_readTrack(psxCdr.Prev);
}


void AddIrqQueue(unsigned char irq, unsigned long ecycle) {
	psxCdr.Irq = irq;
	psxCdr.eCycle = ecycle;

	// Doom: Force rescheduling
	// - Fixes boot
	CDR_INT(ecycle);
}


void Set_Track()
{
	if (CDR_getTN(psxCdr.ResultTN) != -1) {
		int lcv;

		for( lcv = 1; lcv < psxCdr.ResultTN[1]; lcv++ ) {
			if (CDR_getTD((u8)(lcv), psxCdr.ResultTD) != -1) {
#ifdef CDR_LOG___0
				CDR_LOG( "settrack %d %d %d | %d %d %d | %d\n",
					psxCdr.SetSectorPlay[0], psxCdr.SetSectorPlay[1], psxCdr.SetSectorPlay[2],
					psxCdr.ResultTD[2], psxCdr.ResultTD[1], psxCdr.ResultTD[0],
					psxCdr.CurTrack );
#endif

				// check if time matches track start (only need min, sec accuracy)
				// - m:s:f vs f:s:m
				if( psxCdr.SetSectorPlay[0] == psxCdr.ResultTD[2] &&
						psxCdr.SetSectorPlay[1] == psxCdr.ResultTD[1] ) {
					// skip pregap frames
					if( psxCdr.SetSectorPlay[2] < psxCdr.ResultTD[0] )
						psxCdr.SetSectorPlay[2] = psxCdr.ResultTD[0];

					break;
				}
				else if( psxCdr.SetSectorPlay[0] < psxCdr.ResultTD[2] )
					break;
			}
		}
	}
}


static u8 fake_subq_local[3], fake_subq_real[3], fake_subq_index, fake_subq_change;
static void Create_Fake_Subq()
{
	u8 temp_cur[3], temp_next[3], temp_start[3], pregap;
	int diff;

	if (CDR_getTN(psxCdr.ResultTN) == -1) return;
	if( psxCdr.CurTrack+1 <= psxCdr.ResultTN[1] ) {
		pregap = 150;
		if( CDR_getTD(psxCdr.CurTrack+1, psxCdr.ResultTD) == -1 ) return;
	} else {
		// last track - cd size
		pregap = 0;
		if( CDR_getTD(0, psxCdr.ResultTD) == -1 ) return;
	}

	if( psxCdr.Play == TRUE ) {
		temp_cur[0] = psxCdr.SetSectorPlay[0];
		temp_cur[1] = psxCdr.SetSectorPlay[1];
		temp_cur[2] = psxCdr.SetSectorPlay[2];
	} else {
		temp_cur[0] = btoi( psxCdr.Prev[0] );
		temp_cur[1] = btoi( psxCdr.Prev[1] );
		temp_cur[2] = btoi( psxCdr.Prev[2] );
	}

	fake_subq_real[0] = temp_cur[0];
	fake_subq_real[1] = temp_cur[1];
	fake_subq_real[2] = temp_cur[2];

	temp_next[0] = psxCdr.ResultTD[2];
	temp_next[1] = psxCdr.ResultTD[1];
	temp_next[2] = psxCdr.ResultTD[0];


	// flag- next track
	if( msf2sec((char *)temp_cur) >= msf2sec( (char *)temp_next )-pregap ) {
		fake_subq_change = 1;

		psxCdr.CurTrack++;

		// end cd
		if( pregap == 0 ) StopCdda();
	}

	//////////////////////////////////////////////////
	//////////////////////////////////////////////////

	// repair
	if( psxCdr.CurTrack <= psxCdr.ResultTN[1] ) {
		if( CDR_getTD(psxCdr.CurTrack, psxCdr.ResultTD) == -1 ) return;
	} else {
		// last track - cd size
		if( CDR_getTD(0, psxCdr.ResultTD) == -1 ) return;
	}
	
	temp_start[0] = psxCdr.ResultTD[2];
	temp_start[1] = psxCdr.ResultTD[1];
	temp_start[2] = psxCdr.ResultTD[0];


#ifdef CDR_LOG
	CDR_LOG( "CDDA FAKE SUB - %d:%d:%d / %d:%d:%d / %d:%d:%d\n",
		temp_cur[0], temp_cur[1], temp_cur[2],
		temp_start[0], temp_start[1], temp_start[2],
		temp_next[0], temp_next[1], temp_next[2]);
#endif



	// local time - pregap / real
	diff = msf2sec((char *)temp_cur) - msf2sec( (char *)temp_start );
	if( diff < 0 ) {
		fake_subq_index = 0;

		sec2msf( -diff, (char *)fake_subq_local );
	} else {
		fake_subq_index = 1;

		sec2msf( diff, (char *)fake_subq_local );
	}
}


static void cdrPlayInterrupt_Autopause()
{
	struct SubQ *subq = (struct SubQ *)CDR_getBufferSub();
	if (subq != NULL ) {
#ifdef CDR_LOG
		CDR_LOG( "CDDA SUB - %X:%X:%X\n",
			subq->AbsoluteAddress[0], subq->AbsoluteAddress[1], subq->AbsoluteAddress[2] );
#endif

		/*
		CDDA Autopause

		Silhouette Mirage ($3)
		Tomb Raider 1 ($7)
		*/

		if( psxCdr.CurTrack >= btoi( subq->TrackNumber ) )
			return;
	} else {
		Create_Fake_Subq();
#ifdef CDR_LOG___0
		CDR_LOG( "CDDA FAKE SUB - %d:%d:%d\n",
			fake_subq_real[0], fake_subq_real[1], fake_subq_real[2] );
#endif

		if( !fake_subq_change )
			return;

		fake_subq_change = 0;
	}

	if (psxCdr.Mode & MODE_AUTOPAUSE) {
#ifdef CDR_LOG
		CDR_LOG( "CDDA STOP\n" );
#endif

		// Magic the Gathering
		// - looping territory cdda

		// ...?
		//psxCdr.ResultReady = 1;
		//psxCdr.Stat = DataReady;
		psxCdr.Stat = DataEnd;
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);

		StopCdda();
	}
	if (psxCdr.Mode & MODE_REPORT) {
		// rearmed note: PCSX-Reloaded does this for every sector,
		// but we try to get away with only track change here.
		memset( psxCdr.Result, 0, 8 );
		psxCdr.Result[0] |= 0x10;

		if (subq != NULL) {
#ifdef CDR_LOG
			CDR_LOG( "REPPLAY SUB - %X:%X:%X\n",
				subq->AbsoluteAddress[0], subq->AbsoluteAddress[1], subq->AbsoluteAddress[2] );
#endif
			psxCdr.CurTrack = btoi( subq->TrackNumber );

			// BIOS CD Player: data already BCD format
			psxCdr.Result[1] = subq->TrackNumber;
			psxCdr.Result[2] = subq->IndexNumber;

			psxCdr.Result[3] = subq->AbsoluteAddress[0];
			psxCdr.Result[4] = subq->AbsoluteAddress[1];
			psxCdr.Result[5] = subq->AbsoluteAddress[2];
		} else {
#ifdef CDR_LOG___0
			CDR_LOG( "REPPLAY FAKE - %d:%d:%d\n",
				fake_subq_real[0], fake_subq_real[1], fake_subq_real[2] );
#endif

			// track # / index #
			psxCdr.Result[1] = itob(psxCdr.CurTrack);
			psxCdr.Result[2] = itob(fake_subq_index);
			// absolute
			psxCdr.Result[3] = itob( fake_subq_real[0] );
			psxCdr.Result[4] = itob( fake_subq_real[1] );
			psxCdr.Result[5] = itob( fake_subq_real[2] );
		}

		// Rayman: Logo freeze (resultready + dataready)
		psxCdr.ResultReady = 1;
		psxCdr.Stat = DataReady;

		SetResultSize(8);
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
	}
}

void cdrPlayInterrupt()
{
	if( !psxCdr.Play ) return;

#ifdef CDR_LOG
	CDR_LOG( "CDDA - %d:%d:%d\n",
		psxCdr.SetSectorPlay[0], psxCdr.SetSectorPlay[1], psxCdr.SetSectorPlay[2] );
#endif
	CDRPLAY_INT( cdReadTime );

	if (!psxCdr.Irq && !psxCdr.Stat && (psxCdr.Mode & MODE_CDDA) && (psxCdr.Mode & (MODE_AUTOPAUSE|MODE_REPORT)))
		cdrPlayInterrupt_Autopause();

	psxCdr.SetSectorPlay[2]++;
	if (psxCdr.SetSectorPlay[2] == 75) {
		psxCdr.SetSectorPlay[2] = 0;
		psxCdr.SetSectorPlay[1]++;
		if (psxCdr.SetSectorPlay[1] == 60) {
			psxCdr.SetSectorPlay[1] = 0;
			psxCdr.SetSectorPlay[0]++;
		}
	}

	//Check_Shell(0);
}

void cdrInterrupt() {
	int i;
	unsigned char Irq = psxCdr.Irq;
	struct SubQ *subq;

	// Reschedule IRQ
	if (psxCdr.Stat) {
		CDR_INT( 0x100 );
		return;
	}

	psxCdr.Irq = 0xff;
	psxCdr.Ctrl &= ~0x80;

	switch (Irq) {
		case CdlSync:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlNop:
			SetResultSize(1);
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;

			if (psxCdr.LidCheck == 0) psxCdr.LidCheck = 0x20;
			break;

		case CdlSetloc:
			psxCdr.CmdProcess = 0;
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlPlay:
			fake_subq_change = 0;

			if( psxCdr.Seeked == FALSE ) {
				memcpy( psxCdr.SetSectorPlay, psxCdr.SetSector, 4 );
				psxCdr.Seeked = TRUE;
			}

			/*
			Rayman: detect track changes
			- fixes logo freeze

			Twisted Metal 2: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play

			Wild 9: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play
			*/
			/* unneeded with correct cdriso?
			Set_Track();
			*/
			Find_CurTrack();
			ReadTrack( psxCdr.SetSectorPlay );

			// GameShark CD Player: Calls 2x + Play 2x
			if( psxCdr.FastBackward || psxCdr.FastForward ) {
				if( psxCdr.FastForward ) psxCdr.FastForward--;
				if( psxCdr.FastBackward ) psxCdr.FastBackward--;

				if( psxCdr.FastBackward == 0 && psxCdr.FastForward == 0 ) {
					if( psxCdr.Play && CDR_getStatus(&stat) != -1 ) {
						psxCdr.SetSectorPlay[0] = stat.Time[0];
						psxCdr.SetSectorPlay[1] = stat.Time[1];
						psxCdr.SetSectorPlay[2] = stat.Time[2];
					}
				}
			}


			if (!Config.Cdda) {
				// BIOS CD Player
				// - Pause player, hit Track 01/02/../xx (Setloc issued!!)

				// GameShark CD Player: Resume play
				if( psxCdr.ParamC == 0 ) {
#ifdef CDR_LOG___0
					CDR_LOG( "PLAY Resume @ %d:%d:%d\n",
						psxCdr.SetSectorPlay[0], psxCdr.SetSectorPlay[1], psxCdr.SetSectorPlay[2] );
#endif

					//CDR_play( psxCdr.SetSectorPlay );
				}
				else
				{
					// BIOS CD Player: Resume play
					if( psxCdr.Param[0] == 0 ) {
#ifdef CDR_LOG___0
						CDR_LOG( "PLAY Resume T0 @ %d:%d:%d\n",
							psxCdr.SetSectorPlay[0], psxCdr.SetSectorPlay[1], psxCdr.SetSectorPlay[2] );
#endif

						//CDR_play( psxCdr.SetSectorPlay );
					}
					else {
#ifdef CDR_LOG___0
						CDR_LOG( "PLAY Resume Td @ %d:%d:%d\n",
							psxCdr.SetSectorPlay[0], psxCdr.SetSectorPlay[1], psxCdr.SetSectorPlay[2] );
#endif

						// BIOS CD Player: Allow track replaying
						StopCdda();


						psxCdr.CurTrack = btoi( psxCdr.Param[0] );

						if (CDR_getTN(psxCdr.ResultTN) != -1) {
							// check last track
							if (psxCdr.CurTrack > psxCdr.ResultTN[1])
								psxCdr.CurTrack = psxCdr.ResultTN[1];

							if (CDR_getTD((u8)(psxCdr.CurTrack), psxCdr.ResultTD) != -1) {
								psxCdr.SetSectorPlay[0] = psxCdr.ResultTD[2];
								psxCdr.SetSectorPlay[1] = psxCdr.ResultTD[1];
								psxCdr.SetSectorPlay[2] = psxCdr.ResultTD[0];

								// reset data
								Set_Track();
								Find_CurTrack();
								ReadTrack( psxCdr.SetSectorPlay );

								//CDR_play(psxCdr.SetSectorPlay);
							}
						}
					}
				}
			}


			// Vib Ribbon: gameplay checks flag
			psxCdr.StatP &= ~STATUS_SEEK;


			psxCdr.CmdProcess = 0;
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;

			psxCdr.StatP |= STATUS_PLAY;

			
			// BIOS player - set flag again
			psxCdr.Play = TRUE;

			CDRPLAY_INT( cdReadTime );
			break;

		case CdlForward:
			psxCdr.CmdProcess = 0;
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;


			// GameShark CD Player: Calls 2x + Play 2x
			if( psxCdr.FastForward == 0 ) psxCdr.FastForward = 2;
			else psxCdr.FastForward++;

			psxCdr.FastBackward = 0;
			break;

		case CdlBackward:
			psxCdr.CmdProcess = 0;
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;


			// GameShark CD Player: Calls 2x + Play 2x
			if( psxCdr.FastBackward == 0 ) psxCdr.FastBackward = 2;
			else psxCdr.FastBackward++;

			psxCdr.FastForward = 0;
			break;

		case CdlStandby:
			psxCdr.CmdProcess = 0;
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
			break;

		case CdlStop:
			psxCdr.CmdProcess = 0;
			SetResultSize(1);
			psxCdr.StatP &= ~STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
//			psxCdr.Stat = Acknowledge;

			if (psxCdr.LidCheck == 0) psxCdr.LidCheck = 0x20;
			break;

		case CdlPause:
			SetResultSize(1);
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;

			/*
			Gundam Battle Assault 2: much slower (*)
			- Fixes boot, gameplay

			Hokuto no Ken 2: slower
			- Fixes intro + subtitles

			InuYasha - Feudal Fairy Tale: slower
			- Fixes battles
			*/
			AddIrqQueue(CdlPause + 0x20, cdReadTime * 3);
			psxCdr.Ctrl |= 0x80;
			break;

		case CdlPause + 0x20:
			SetResultSize(1);
			psxCdr.StatP &= ~STATUS_READ;
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
			break;

		case CdlInit:
			SetResultSize(1);
			psxCdr.StatP = STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
//			if (!psxCdr.Init) {
				AddIrqQueue(CdlInit + 0x20, 0x800);
//			}
        	break;

		case CdlInit + 0x20:
			SetResultSize(1);
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
			psxCdr.Init = 1;
			break;

		case CdlMute:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlDemute:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlSetfilter:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlSetmode:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlGetmode:
			SetResultSize(6);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Result[1] = psxCdr.Mode;
			psxCdr.Result[2] = psxCdr.File;
			psxCdr.Result[3] = psxCdr.Channel;
			psxCdr.Result[4] = 0;
			psxCdr.Result[5] = 0;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlGetlocL:
			SetResultSize(8);
			for (i = 0; i < 8; i++)
				psxCdr.Result[i] = psxCdr.Transfer[i];
			psxCdr.Stat = Acknowledge;
			break;

		case CdlGetlocP:
			// GameShark CDX CD Player: uses 17 bytes output (wraps around)
			SetResultSize(17);
			memset( psxCdr.Result, 0, 16 );

			subq = (struct SubQ *)CDR_getBufferSub();

			if (subq != NULL) {
				psxCdr.Result[0] = subq->TrackNumber;
				psxCdr.Result[1] = subq->IndexNumber;
				memcpy(psxCdr.Result + 2, subq->TrackRelativeAddress, 3);
				memcpy(psxCdr.Result + 5, subq->AbsoluteAddress, 3);


				// subQ integrity check - data only (skip audio)
				if( subq->TrackNumber == 1 && stat.Type == 0x01 ) {
				if (qChecksum((const char *)subq + 12, 10) != (((u16)subq->CRC[0] << 8) | subq->CRC[1])) {
					memset(psxCdr.Result + 2, 0, 3 + 3); // CRC wrong, wipe out time data
				}
				}
			} else {
				if( psxCdr.Play == FALSE || !(psxCdr.Mode & MODE_CDDA) || !(psxCdr.Mode & (MODE_AUTOPAUSE|MODE_REPORT)) )
					Create_Fake_Subq();


				// track # / index #
				psxCdr.Result[0] = itob(psxCdr.CurTrack);
				psxCdr.Result[1] = itob(fake_subq_index);

				// local
				psxCdr.Result[2] = itob( fake_subq_local[0] );
				psxCdr.Result[3] = itob( fake_subq_local[1] );
				psxCdr.Result[4] = itob( fake_subq_local[2] );

				// absolute
				psxCdr.Result[5] = itob( fake_subq_real[0] );
				psxCdr.Result[6] = itob( fake_subq_real[1] );
				psxCdr.Result[7] = itob( fake_subq_real[2] );
			}

			// redump.org - wipe time
			if( !psxCdr.Play && CheckSBI(psxCdr.Result+5) ) {
				memset( psxCdr.Result+2, 0, 6 );
			}

			psxCdr.Stat = Acknowledge;
			break;

		case CdlGetTN:
			// 5-Star Racing: don't stop CDDA
			//
			// Vib Ribbon: CD swap
			StopReading();

			psxCdr.CmdProcess = 0;
			SetResultSize(3);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			if (CDR_getTN(psxCdr.ResultTN) == -1) {
				psxCdr.Stat = DiskError;
				psxCdr.Result[0] |= STATUS_ERROR;
			} else {
				psxCdr.Stat = Acknowledge;
				psxCdr.Result[1] = itob(psxCdr.ResultTN[0]);
				psxCdr.Result[2] = itob(psxCdr.ResultTN[1]);
			}
			break;

		case CdlGetTD:
			psxCdr.CmdProcess = 0;
			psxCdr.Track = btoi(psxCdr.Param[0]);
			SetResultSize(4);
			psxCdr.StatP |= STATUS_ROTATING;
			if (CDR_getTD(psxCdr.Track, psxCdr.ResultTD) == -1) {
				psxCdr.Stat = DiskError;
				psxCdr.Result[0] |= STATUS_ERROR;
			} else {
				psxCdr.Stat = Acknowledge;
				psxCdr.Result[0] = psxCdr.StatP;
				psxCdr.Result[1] = itob(psxCdr.ResultTD[2]);
				psxCdr.Result[2] = itob(psxCdr.ResultTD[1]);
				psxCdr.Result[3] = itob(psxCdr.ResultTD[0]);
			}
			break;

		case CdlSeekL:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.StatP |= STATUS_SEEK;
			psxCdr.Stat = Acknowledge;

			/*
			Crusaders of Might and Magic = 0.5x-4x
			- fix cutscene speech start

			Eggs of Steel = 2x-?
			- fix new game

			Medievil = ?-4x
			- fix cutscene speech

			Rockman X5 = 0.5-4x
			- fix capcom logo
			*/
			AddIrqQueue(CdlSeekL + 0x20, cdReadTime * 4);
			break;

		case CdlSeekL + 0x20:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.StatP &= ~STATUS_SEEK;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Seeked = TRUE;
			psxCdr.Stat = Complete;


			// Mega Man Legends 2: must update read cursor for getlocp
			ReadTrack( psxCdr.SetSector );
			break;

		case CdlSeekP:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.StatP |= STATUS_SEEK;
			psxCdr.Stat = Acknowledge;
			AddIrqQueue(CdlSeekP + 0x20, cdReadTime * 1);
			break;

		case CdlSeekP + 0x20:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.StatP &= ~STATUS_SEEK;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
			psxCdr.Seeked = TRUE;

			// GameShark Music Player
			memcpy( psxCdr.SetSectorPlay, psxCdr.SetSector, 4 );

			// Tomb Raider 2: must update read cursor for getlocp
			Find_CurTrack();
			ReadTrack( psxCdr.SetSectorPlay );
			break;

		case CdlTest:
			psxCdr.Stat = Acknowledge;
			switch (psxCdr.Param[0]) {
				case 0x20: // System Controller ROM Version
					SetResultSize(4);
					memcpy(psxCdr.Result, Test20, 4);
					break;
				case 0x22:
					SetResultSize(8);
					memcpy(psxCdr.Result, Test22, 4);
					break;
				case 0x23: case 0x24:
					SetResultSize(8);
					memcpy(psxCdr.Result, Test23, 4);
					break;
			}
			break;

		case CdlID:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			AddIrqQueue(CdlID + 0x20, 0x800);
			break;

		case CdlID + 0x20:
			SetResultSize(8);

			if (CDR_getStatus(&stat) == -1) {
				psxCdr.Result[0] = 0x00; // 0x08 and psxCdr.Result[1]|0x10 : audio cd, enters cd player
				psxCdr.Result[1] = 0x80; // 0x80 leads to the menu in the bios, else loads CD
			}
			else {
				if (stat.Type == 2) {
					// Music CD
					psxCdr.Result[0] = 0x08;
					psxCdr.Result[1] = 0x10;

					psxCdr.Result[1] |= 0x80;
				}
				else {
					// Data CD
					if (CdromId[0] == '\0') {
						psxCdr.Result[0] = 0x00;
						psxCdr.Result[1] = 0x80;
					}
					else {
						psxCdr.Result[0] = 0x08;
						psxCdr.Result[1] = 0x00;
					}
				}
			}

			psxCdr.Result[2] = 0x00;
			psxCdr.Result[3] = 0x00;
			strncpy((char *)&psxCdr.Result[4], "PCSX", 4);
			psxCdr.Stat = Complete;
			break;

		case CdlReset:
			SetResultSize(1);
			psxCdr.StatP = STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case CdlReadT:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			AddIrqQueue(CdlReadT + 0x20, 0x800);
			break;

		case CdlReadT + 0x20:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
			break;

		case CdlReadToc:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			AddIrqQueue(CdlReadToc + 0x20, 0x800);
			break;

		case CdlReadToc + 0x20:
			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Complete;
			break;

		case AUTOPAUSE:
			psxCdr.OCUP = 0;
/*			SetResultSize(1);
			StopCdda();
			StopReading();
			psxCdr.OCUP = 0;
			psxCdr.StatP&=~0x20;
			psxCdr.StatP|= 0x2;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = DataEnd;
*/			AddIrqQueue(CdlPause, 0x800);
			break;

		case READ_ACK:
			if (!psxCdr.Reading) return;


			// Fighting Force 2 - update subq time immediately
			// - fixes new game
			ReadTrack( psxCdr.SetSector );


			// Crusaders of Might and Magic - update getlocl now
			// - fixes cutscene speech
			{
				u8 *buf = CDR_getBuffer();
				if (buf != NULL)
					memcpy(psxCdr.Transfer, buf, 8);
			}
			
			
			/*
			Duke Nukem: Land of the Babes - seek then delay read for one frame
			- fixes cutscenes
			*/

			if (!psxCdr.Seeked) {
				psxCdr.Seeked = TRUE;

				psxCdr.StatP |= STATUS_SEEK;
				psxCdr.StatP &= ~STATUS_READ;

				// Crusaders of Might and Magic - use short time
				// - fix cutscene speech (startup)

				// ??? - use more accurate seek time later
				CDREAD_INT((psxCdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime * 1);
			} else {
				psxCdr.StatP |= STATUS_READ;
				psxCdr.StatP &= ~STATUS_SEEK;

				CDREAD_INT((psxCdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime * 1);
			}

			SetResultSize(1);
			psxCdr.StatP |= STATUS_ROTATING;
			psxCdr.Result[0] = psxCdr.StatP;
			psxCdr.Stat = Acknowledge;
			break;

		case 0xff:
			return;

		default:
			psxCdr.Stat = Complete;
			break;
	}

	Check_Shell( Irq );

	if (psxCdr.Stat != NoIntr && psxCdr.Reg2 != 0x18) {
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
	}

#ifdef CDR_LOG
	CDR_LOG("cdrInterrupt() Log: CDR Interrupt IRQ %x\n", Irq);
#endif
}

void cdrReadInterrupt() {
	u8 *buf;

	if (!psxCdr.Reading)
		return;

	if (psxCdr.Irq || psxCdr.Stat) {
		CDREAD_INT(0x100);
		return;
	}

#ifdef CDR_LOG
	CDR_LOG("cdrReadInterrupt() Log: KEY END");
#endif

	psxCdr.OCUP = 1;
	SetResultSize(1);
	psxCdr.StatP |= STATUS_READ|STATUS_ROTATING;
	psxCdr.StatP &= ~STATUS_SEEK;
	psxCdr.Result[0] = psxCdr.StatP;

	ReadTrack( psxCdr.SetSector );

	buf = CDR_getBuffer();
	if (buf == NULL)
		psxCdr.RErr = -1;

	if (psxCdr.RErr == -1) {
#ifdef CDR_LOG
		fprintf(emuLog, "cdrReadInterrupt() Log: err\n");
#endif
		memset(psxCdr.Transfer, 0, DATA_SIZE);
		psxCdr.Stat = DiskError;
		psxCdr.Result[0] |= STATUS_ERROR;
		CDREAD_INT((psxCdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
		return;
	}

	memcpy(psxCdr.Transfer, buf, DATA_SIZE);
	CheckPPFCache(psxCdr.Transfer, psxCdr.Prev[0], psxCdr.Prev[1], psxCdr.Prev[2]);


#ifdef CDR_LOG
	fprintf(emuLog, "cdrReadInterrupt() Log: psxCdr.Transfer %x:%x:%x\n", psxCdr.Transfer[0], psxCdr.Transfer[1], psxCdr.Transfer[2]);
#endif

	if ((!psxCdr.Muted) && (psxCdr.Mode & MODE_STRSND) && (!Config.Xa) && (psxCdr.FirstSector != -1)) { // CD-XA
		// Firemen 2: Multi-XA files - briefings, cutscenes
		if( psxCdr.FirstSector == 1 && (psxCdr.Mode & MODE_SF)==0 ) {
			psxCdr.File = psxCdr.Transfer[4 + 0];
			psxCdr.Channel = psxCdr.Transfer[4 + 1];
		}

		if ((psxCdr.Transfer[4 + 2] & 0x4) &&
			 (psxCdr.Transfer[4 + 1] == psxCdr.Channel) &&
			(psxCdr.Transfer[4 + 0] == psxCdr.File)) {
			int ret = xa_decode_sector(&psxCdr.Xa, psxCdr.Transfer+4, psxCdr.FirstSector);

			if (!ret) {
				// only handle attenuator basic channel switch for now
				if (psxCdr.Xa.stereo) {
					int i;
					if ((psxCdr.AttenuatorLeft[0] | psxCdr.AttenuatorLeft[1])
						&& !(psxCdr.AttenuatorRight[0] | psxCdr.AttenuatorRight[1]))
					{
						for (i = 0; i < psxCdr.Xa.nsamples; i++)
							psxCdr.Xa.pcm[i*2 + 1] = psxCdr.Xa.pcm[i*2];
					}
					else if (!(psxCdr.AttenuatorLeft[0] | psxCdr.AttenuatorLeft[1])
						&& (psxCdr.AttenuatorRight[0] | psxCdr.AttenuatorRight[1]))
					{
						for (i = 0; i < psxCdr.Xa.nsamples; i++)
							psxCdr.Xa.pcm[i*2] = psxCdr.Xa.pcm[i*2 + 1];
					}
				}

				SPU_playADPCMchannel(&psxCdr.Xa);
				psxCdr.FirstSector = 0;

#if 0
				// Crash Team Racing: music, speech
				// - done using cdda decoded buffer (spu irq)
				// - don't do here

				// signal ADPCM data ready
				psxHu32ref(0x1070) |= SWAP32((u32)0x200);
#endif
			}
			else psxCdr.FirstSector = -1;
		}
	}

	psxCdr.SetSector[2]++;
	if (psxCdr.SetSector[2] == 75) {
		psxCdr.SetSector[2] = 0;
		psxCdr.SetSector[1]++;
		if (psxCdr.SetSector[1] == 60) {
			psxCdr.SetSector[1] = 0;
			psxCdr.SetSector[0]++;
		}
	}

	psxCdr.Readed = 0;

	// G-Police: Don't autopause ADPCM even if mode set (music)
	if ((psxCdr.Transfer[4 + 2] & 0x80) && (psxCdr.Mode & MODE_AUTOPAUSE) &&
			(psxCdr.Transfer[4 + 2] & 0x4) != 0x4 ) { // EOF
#ifdef CDR_LOG
		CDR_LOG("cdrReadInterrupt() Log: Autopausing read\n");
#endif
//		AddIrqQueue(AUTOPAUSE, 0x2000);
		AddIrqQueue(CdlPause, 0x2000);
	}
	else {
		CDREAD_INT((psxCdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime);
	}

	/*
	Croc 2: $40 - only FORM1 (*)
	Judge Dredd: $C8 - only FORM1 (*)
	Sim Theme Park - no adpcm at all (zero)
	*/

	if( (psxCdr.Mode & MODE_STRSND) == 0 || (psxCdr.Transfer[4+2] & 0x4) != 0x4 ) {
		psxCdr.Stat = DataReady;
	} else {
		// Breath of Fire 3 - fix inn sleeping
		// Rockman X5 - no music restart problem
		psxCdr.Stat = NoIntr;
	}
	psxHu32ref(0x1070) |= SWAP32((u32)0x4);

	Check_Shell(0);
}

/*
cdrRead0:
	bit 0 - 0 REG1 command send / 1 REG1 data read
	bit 1 - 0 data transfer finish / 1 data transfer ready/in progress
	bit 2 - unknown
	bit 3 - unknown
	bit 4 - unknown
	bit 5 - 1 result ready
	bit 6 - 1 dma ready
	bit 7 - 1 command being processed
*/

unsigned char cdrRead0(void) {
	if (psxCdr.ResultReady)
		psxCdr.Ctrl |= 0x20;
	else
		psxCdr.Ctrl &= ~0x20;

	if (psxCdr.OCUP)
		psxCdr.Ctrl |= 0x40;
//  else
//		psxCdr.Ctrl &= ~0x40;

	// What means the 0x10 and the 0x08 bits? I only saw it used by the bios
	psxCdr.Ctrl |= 0x18;

#ifdef CDR_LOG
	CDR_LOG("cdrRead0() Log: CD0 Read: %x\n", psxCdr.Ctrl);
#endif

	return psxHu8(0x1800) = psxCdr.Ctrl;
}

/*
cdrWrite0:
	0 - to send a command / 1 - to get the result
*/

void cdrWrite0(unsigned char rt) {
#ifdef CDR_LOG
	CDR_LOG("cdrWrite0() Log: CD0 write: %x\n", rt);
#endif
	psxCdr.Ctrl = rt | (psxCdr.Ctrl & ~0x3);

	if (rt == 0) {
		psxCdr.ParamP = 0;
		psxCdr.ParamC = 0;
		psxCdr.ResultReady = 0;
	}
}

unsigned char cdrRead1(void) {
	if (psxCdr.ResultReady) { // && psxCdr.Ctrl & 0x1) {
		// GameShark CDX CD Player: uses 17 bytes output (wraps around)
		psxHu8(0x1801) = psxCdr.Result[psxCdr.ResultP & 0xf];
		psxCdr.ResultP++;
		if (psxCdr.ResultP == psxCdr.ResultC)
			psxCdr.ResultReady = 0;
	} else {
		psxHu8(0x1801) = 0;
	}
#ifdef CDR_LOG
	CDR_LOG("cdrRead1() Log: CD1 Read: %x\n", psxHu8(0x1801));
#endif
	return psxHu8(0x1801);
}

void cdrWrite1(unsigned char rt) {
	int i;

#ifdef CDR_LOG
	CDR_LOG("cdrWrite1() Log: CD1 write: %x (%s)\n", rt, CmdName[rt]);
#endif


	// Tekken: CDXA fade-out
	if( (psxCdr.Ctrl & 3) == 3 ) {
		psxCdr.AttenuatorRight[0] = rt;
	}


//	psxHu8(0x1801) = rt;
	psxCdr.Cmd = rt;
	psxCdr.OCUP = 0;

#ifdef CDRCMD_DEBUG
	SysPrintf("cdrWrite1() Log: CD1 write: %x (%s)", rt, CmdName[rt]);
	if (psxCdr.ParamC) {
		SysPrintf(" Param[%d] = {", psxCdr.ParamC);
		for (i = 0; i < psxCdr.ParamC; i++)
			SysPrintf(" %x,", psxCdr.Param[i]);
		SysPrintf("}\n");
	} else {
		SysPrintf("\n");
	}
#endif

	if (psxCdr.Ctrl & 0x1) return;

	switch (psxCdr.Cmd) {
    	case CdlSync:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlNop:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;

		// Twisted Metal 3 - fix music
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlSetloc:
		StopReading();
		psxCdr.Seeked = FALSE;
		for (i = 0; i < 3; i++)
			psxCdr.SetSector[i] = btoi(psxCdr.Param[i]);
			psxCdr.SetSector[3] = 0;

		/*
		   if ((psxCdr.SetSector[0] | psxCdr.SetSector[1] | psxCdr.SetSector[2]) == 0) {
		 *(u32 *)psxCdr.SetSector = *(u32 *)psxCdr.SetSectorSeek;
		 }*/

		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlPlay:
		// Vib Ribbon: try same track again
		StopCdda();

		if (!psxCdr.SetSector[0] & !psxCdr.SetSector[1] & !psxCdr.SetSector[2]) {
			if (CDR_getTN(psxCdr.ResultTN) != -1) {
				if (psxCdr.CurTrack > psxCdr.ResultTN[1])
					psxCdr.CurTrack = psxCdr.ResultTN[1];
				if (CDR_getTD((unsigned char)(psxCdr.CurTrack), psxCdr.ResultTD) != -1) {
					int tmp = psxCdr.ResultTD[2];
					psxCdr.ResultTD[2] = psxCdr.ResultTD[0];
					psxCdr.ResultTD[0] = tmp;
					if (!Config.Cdda) CDR_play(psxCdr.ResultTD);
				}
			}
		} else if (!Config.Cdda) {
			CDR_play(psxCdr.SetSector);
		}

		// Vib Ribbon - decoded buffer IRQ for CDDA reading
		// - fixes ribbon timing + music CD mode
		//TODO?
		//CDRDBUF_INT( PSXCLK / 44100 * 0x100 );


		psxCdr.Play = TRUE;

		psxCdr.StatP |= STATUS_SEEK;
		psxCdr.StatP &= ~STATUS_ROTATING;

		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		AddIrqQueue(psxCdr.Cmd, 0x800);
    		break;

    	case CdlForward:
		//if (psxCdr.CurTrack < 0xaa)
		//	psxCdr.CurTrack++;
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlBackward:
		//if (psxCdr.CurTrack > 1)
		//psxCdr.CurTrack--;
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlReadN:
		psxCdr.Irq = 0;
		StopReading();
		psxCdr.Ctrl|= 0x80;
		psxCdr.Stat = NoIntr;
		StartReading(1, 0x800);
		break;

    	case CdlStandby:
		StopCdda();
		StopReading();
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlStop:
		// GameShark CD Player: Reset CDDA to track start
		if( psxCdr.Play && CDR_getStatus(&stat) != -1 ) {
			psxCdr.SetSectorPlay[0] = stat.Time[0];
			psxCdr.SetSectorPlay[1] = stat.Time[1];
			psxCdr.SetSectorPlay[2] = stat.Time[2];

			Find_CurTrack();


			// grab time for current track
			CDR_getTD((u8)(psxCdr.CurTrack), psxCdr.ResultTD);

			psxCdr.SetSectorPlay[0] = psxCdr.ResultTD[2];
			psxCdr.SetSectorPlay[1] = psxCdr.ResultTD[1];
			psxCdr.SetSectorPlay[2] = psxCdr.ResultTD[0];
		}

		StopCdda();
		StopReading();

		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlPause:
		/*
		   GameShark CD Player: save time for resume

		   Twisted Metal - World Tour: don't mix Setloc / CdlPlay cursors
		*/

		StopCdda();
		StopReading();
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;

		AddIrqQueue(psxCdr.Cmd, 0x800);
		break;

	case CdlReset:
    	case CdlInit:
		StopCdda();
		StopReading();
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlMute:
			psxCdr.Muted = TRUE;
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);

			// Duke Nukem - Time to Kill
			// - do not directly set cd-xa volume
			//SPU_writeRegister( H_CDLeft, 0x0000 );
			//SPU_writeRegister( H_CDRight, 0x0000 );
        	break;

    	case CdlDemute:
			psxCdr.Muted = FALSE;
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);

			// Duke Nukem - Time to Kill
			// - do not directly set cd-xa volume
			//SPU_writeRegister( H_CDLeft, 0x7f00 );
			//SPU_writeRegister( H_CDRight, 0x7f00 );
        	break;

    	case CdlSetfilter:
			psxCdr.File = psxCdr.Param[0];
			psxCdr.Channel = psxCdr.Param[1];
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlSetmode:
#ifdef CDR_LOG
		CDR_LOG("cdrWrite1() Log: Setmode %x\n", psxCdr.Param[0]);
#endif 
			psxCdr.Mode = psxCdr.Param[0];
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);

		// Squaresoft on PlayStation 1998 Collector's CD Vol. 1
		// - fixes choppy movie sound
		if( psxCdr.Play && (psxCdr.Mode & MODE_CDDA) == 0 )
			StopCdda();
        	break;

    	case CdlGetmode:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlGetlocL:
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;

		// Crusaders of Might and Magic - cutscene speech
		AddIrqQueue(psxCdr.Cmd, 0x800);
		break;

    	case CdlGetlocP:
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;

		// GameShark CDX / Lite Player: pretty narrow time window
		// - doesn't always work due to time inprecision
		//AddIrqQueue(psxCdr.Cmd, 0x28);

		// Tomb Raider 2 - cdda
		//AddIrqQueue(psxCdr.Cmd, 0x40);

		// rearmed: the above works in pcsxr-svn, but breaks here
		// (TOCA world touring cars), perhaps some other code is not merged yet
		AddIrqQueue(psxCdr.Cmd, 0x1000);
        	break;

    	case CdlGetTN:
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		//AddIrqQueue(psxCdr.Cmd, 0x800);

		// GameShark CDX CD Player: very long time
		AddIrqQueue(psxCdr.Cmd, 0x100000);
		break;

    	case CdlGetTD:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlSeekL:
//			((u32 *)psxCdr.SetSectorSeek)[0] = ((u32 *)psxCdr.SetSector)[0];
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		AddIrqQueue(psxCdr.Cmd, 0x800);

		StopCdda();
		StopReading();

		break;

    	case CdlSeekP:
//        	((u32 *)psxCdr.SetSectorSeek)[0] = ((u32 *)psxCdr.SetSector)[0];
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;

		// Tomb Raider 2 - reset cdda
		StopCdda();
		StopReading();

		AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

	// Destruction Derby: read TOC? GetTD after this
	case CdlReadT:
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		AddIrqQueue(psxCdr.Cmd, 0x800);
		break;

    	case CdlTest:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlID:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	case CdlReadS:
		psxCdr.Irq = 0;
		StopReading();
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		StartReading(2, 0x800);
		break;

    	case CdlReadToc:
		psxCdr.Ctrl |= 0x80;
			psxCdr.Stat = NoIntr;
			AddIrqQueue(psxCdr.Cmd, 0x800);
        	break;

    	default:
#ifdef CDR_LOG
		CDR_LOG("cdrWrite1() Log: Unknown command: %x\n", psxCdr.Cmd);
#endif
		return;
	}
	if (psxCdr.Stat != NoIntr) {
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
	}
}

unsigned char cdrRead2(void) {
	unsigned char ret;

	if (psxCdr.Readed == 0) {
		ret = 0;
	} else {
		ret = *psxCdr.pTransfer++;
	}

#ifdef CDR_LOG
	CDR_LOG("cdrRead2() Log: CD2 Read: %x\n", ret);
#endif
	return ret;
}

void cdrWrite2(unsigned char rt) {
#ifdef CDR_LOG
	CDR_LOG("cdrWrite2() Log: CD2 write: %x\n", rt);
#endif

	// Tekken: CDXA fade-out
	if( (psxCdr.Ctrl & 3) == 2 ) {
		psxCdr.AttenuatorLeft[0] = rt;
	}
	else if( (psxCdr.Ctrl & 3) == 3 ) {
		psxCdr.AttenuatorRight[1] = rt;
	}


	if (psxCdr.Ctrl & 0x1) {
		switch (rt) {
			case 0x07:
				psxCdr.ParamP = 0;
				psxCdr.ParamC = 0;
				psxCdr.ResultReady = 1; //0;
				psxCdr.Ctrl &= ~3; //psxCdr.Ctrl = 0;
				break;

			default:
				psxCdr.Reg2 = rt;
				break;
		}
	} else if (!(psxCdr.Ctrl & 0x1) && psxCdr.ParamP < 8) {
		psxCdr.Param[psxCdr.ParamP++] = rt;
		psxCdr.ParamC++;
	}
}

unsigned char cdrRead3(void) {
	if (psxCdr.Stat) {
		if (psxCdr.Ctrl & 0x1)
			psxHu8(0x1803) = psxCdr.Stat | 0xE0;
		else
			psxHu8(0x1803) = 0xff;
	} else {
		psxHu8(0x1803) = 0;
	}
#ifdef CDR_LOG
	CDR_LOG("cdrRead3() Log: CD3 Read: %x\n", psxHu8(0x1803));
#endif
	return psxHu8(0x1803);
}

void cdrWrite3(unsigned char rt) {
#ifdef CDR_LOG
	CDR_LOG("cdrWrite3() Log: CD3 write: %x\n", rt);
#endif

	// Tekken: CDXA fade-out
	if( (psxCdr.Ctrl & 3) == 2 ) {
		psxCdr.AttenuatorLeft[1] = rt;
	}
	else if( (psxCdr.Ctrl & 3) == 3 && rt == 0x20 ) {
#ifdef CDR_LOG
		CDR_LOG( "CD-XA Volume: %X %X | %X %X\n",
			psxCdr.AttenuatorLeft[0], psxCdr.AttenuatorLeft[1],
			psxCdr.AttenuatorRight[0], psxCdr.AttenuatorRight[1] );
#endif
	}


	// GameShark CDX CD Player: Irq timing mania
	if( rt == 0 &&
			psxCdr.Irq != 0 && psxCdr.Irq != 0xff &&
			psxCdr.ResultReady == 0 ) {

		// GS CDX: ~0x28 cycle timing - way too precise
		if( psxCdr.Irq == CdlGetlocP ) {
			cdrInterrupt();

			psxRegs.interrupt &= ~(1 << PSXINT_CDR);
		}
	}


	if (rt == 0x07 && psxCdr.Ctrl & 0x1) {
		psxCdr.Stat = 0;

		if (psxCdr.Irq == 0xff) {
			psxCdr.Irq = 0;
			return;
		}

		// XA streaming - incorrect timing because of this reschedule
		// - Final Fantasy Tactics
		// - various other games

		if (psxCdr.Reading && !psxCdr.ResultReady) {
			int left = psxRegs.intCycle[PSXINT_CDREAD].sCycle + psxRegs.intCycle[PSXINT_CDREAD].cycle - psxRegs.cycle;
			int time = (psxCdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime;
			if (Config.CdrReschedule != 2)
			if (left < time / 2 || Config.CdrReschedule) { // rearmed guesswork hack
				//printf("-- resched %d -> %d\n", left, time);
				CDREAD_INT(time);
			}
		}

		return;
	}

	if (rt == 0x80 && !(psxCdr.Ctrl & 0x1) && psxCdr.Readed == 0) {
		psxCdr.Readed = 1;
		psxCdr.pTransfer = psxCdr.Transfer;

		switch (psxCdr.Mode & 0x30) {
			case MODE_SIZE_2328:
			case 0x00:
				psxCdr.pTransfer += 12;
				break;

			case MODE_SIZE_2340:
				psxCdr.pTransfer += 0;
				break;

			default:
				break;
		}
	}
}

void psxDma3(u32 madr, u32 bcr, u32 chcr) {
	u32 cdsize;
	int size;
	u8 *ptr;

#ifdef CDR_LOG
	CDR_LOG("psxDma3() Log: *** DMA 3 *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif

	switch (chcr) {
		case 0x11000000:
		case 0x11400100:
			if (psxCdr.Readed == 0) {
#ifdef CDR_LOG
				CDR_LOG("psxDma3() Log: *** DMA 3 *** NOT READY\n");
#endif
				break;
			}

			cdsize = (bcr & 0xffff) * 4;

			// Ape Escape: bcr = 0001 / 0000
			// - fix boot
			if( cdsize == 0 )
			{
				switch (psxCdr.Mode & 0x30) {
					case 0x00: cdsize = 2048; break;
					case MODE_SIZE_2328: cdsize = 2328; break;
					case MODE_SIZE_2340: cdsize = 2340; break;
				}
			}


			ptr = (u8 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CDR_LOG("psxDma3() Log: *** DMA 3 *** NULL Pointer!\n");
#endif
				break;
			}

			/*
			GS CDX: Enhancement CD crash
			- Setloc 0:0:0
			- CdlPlay
			- Spams DMA3 and gets buffer overrun
			*/
			size = CD_FRAMESIZE_RAW - (psxCdr.pTransfer - psxCdr.Transfer);
			if (size > cdsize)
				size = cdsize;
			if (size > 0)
			{
				memcpy(ptr, psxCdr.pTransfer, size);
			}

			psxCpu->clear(madr, cdsize / 4);
			psxCdr.pTransfer += cdsize;


			// burst vs normal
			if( chcr == 0x11400100 ) {
				CDRDMA_INT( (cdsize/4) / 4 );
			}
			else if( chcr == 0x11000000 ) {
				CDRDMA_INT( (cdsize/4) * 1 );
			}
			return;

		default:
#ifdef CDR_LOG
			CDR_LOG("psxDma3() Log: Unknown cddma %x\n", chcr);
#endif
			break;
	}

	HW_DMA3_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(3);
}

void cdrDmaInterrupt()
{
	if (HW_DMA3_CHCR & SWAP32(0x01000000))
	{
		HW_DMA3_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(3);
	}
}

void LidInterrupt() {
	psxCdr.LidCheck = 0x20; // start checker

	CDRLID_INT( cdReadTime * 3 );

	// generate interrupt if none active - open or close
	if (psxCdr.Irq == 0 || psxCdr.Irq == 0xff) {
		psxCdr.Ctrl |= 0x80;
		psxCdr.Stat = NoIntr;
		AddIrqQueue(CdlNop, 0x800);
	}
}

PsxCdr psxCdr;

// TODO Xa - his own reset()
void PsxCdr::reset() {
	OCUP = 0;
	Reg1Mode = 0;
	Reg2 = 0;
	CmdProcess = 0;
	Ctrl = 0;
	Stat = 0;

	StatP = 0;

	memset(Transfer, 0, sizeof(Transfer));
	pTransfer = Transfer;

	memset(Prev, 0, sizeof(Prev));
	memset(Param, 0, sizeof(Param));
	memset(Result, 0, sizeof(Result));

	ParamC = 0;
	ParamP = 0;
	ResultC = 0;
	ResultP = 0;
	ResultReady = 0;
	Cmd = 0;
	Readed = 0;
	Reading = 0;

	memset(ResultTN, 0, sizeof(ResultTN));
	memset(ResultTD, 0, sizeof(ResultTD));
	memset(SetSector, 0, sizeof(SetSector));
	memset(SetSectorSeek, 0, sizeof(SetSectorSeek));
	memset(SetSectorPlay, 0, sizeof(SetSectorPlay));
	Track = 0;
	Play = 0;
	Muted = 0;
	Mode = 0;
	Reset = 0;
	RErr = 0;
	FirstSector = 0;

	memset(&Xa, 0, sizeof(Xa));

	Init = 0;

	Irq = 0;
	eCycle = 0;

	Seeked = 0;

	LidCheck = 0;
	FastForward = 0;
	FastBackward = 0;

	psxCdr.CurTrack = 1;
	psxCdr.File = 1;
	psxCdr.Channel = 1;

	psxCdr.AttenuatorLeft[0] = 0x80;
	psxCdr.AttenuatorLeft[1] = 0x00;
	psxCdr.AttenuatorRight[0] = 0x80;
	psxCdr.AttenuatorRight[1] = 0x00;
}

// TODO Xa - his own save/load

void PsxCdr::sl() {
	emsl.begin("cdr");
	if (!emsl.save) {
		StopCdda();
	}
	emsl.var("ocup", OCUP);
	emsl.var("reg1Mode", Reg1Mode);
	emsl.var("reg2", Reg2);
	emsl.var("cmdProcess", CmdProcess);
	emsl.var("ctrl", Ctrl);
	emsl.var("stat", Stat);
	emsl.var("statP", StatP);
	emsl.array("transfer", Transfer, sizeof(Transfer));
	emsl.array("prev", Prev, sizeof(Prev));
	emsl.array("param", Param, sizeof(Param));
	emsl.array("result", Result, sizeof(Result));
	emsl.var("paramC", ParamC);
	emsl.var("paramP", ParamP);
	emsl.var("resultC", ResultC);
	emsl.var("resultP", ResultP);
	emsl.var("resultReady", ResultReady);
	emsl.var("cmd", Cmd);
	emsl.var("readed", Readed);
	emsl.var("reading", Reading);
	emsl.array("resultTN", ResultTN, sizeof(ResultTN));
	emsl.array("resultTD", ResultTD, sizeof(ResultTD));
	emsl.array("setSector", SetSector, sizeof(SetSector));
	emsl.array("setSectorSeek", SetSectorSeek, sizeof(SetSectorSeek));
	emsl.array("setSectorPlay", SetSectorPlay, sizeof(SetSectorPlay));
	emsl.var("track", Track);
	emsl.var("play", Play);
	emsl.var("muted", Muted);
	emsl.var("curTrack", CurTrack);
	emsl.var("mode", Mode);
	emsl.var("file", File);
	emsl.var("channel", Channel);
	emsl.var("reset", Reset);
	emsl.var("rErr", RErr);
	emsl.var("firstSector", FirstSector);
	emsl.array("xa", &Xa, sizeof(Xa));
	emsl.var("init", Init);
	emsl.var("irq", Irq);
	emsl.var("eCycle", eCycle);
	emsl.var("seeked", Seeked);
	emsl.var("lidCheck", LidCheck);
	emsl.var("fastForward", FastForward);
	emsl.var("fastBackward", FastBackward);
	emsl.array("attenuatorLeft", AttenuatorLeft, sizeof(AttenuatorLeft));
	emsl.array("attenuatorRight", AttenuatorRight, sizeof(AttenuatorRight));
	uint offset = psxCdr.pTransfer - psxCdr.Transfer;
	emsl.var("offset", offset);
	psxCdr.pTransfer = psxCdr.Transfer + offset;
	emsl.end();
}
