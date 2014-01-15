/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2002 by Stéphane Dallongeville
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include <stdio.h>

#include "pico.h"
#include "cd_sys.h"
#include "cd_file.h"

#define cdprintf(x...)
#define DEBUG_CD

#define TRAY_OPEN	0x0500		// TRAY OPEN CDD status
#define NOCD		0x0000		// CD removed CDD status
#define STOPPED		0x0900		// STOPPED CDD status (happen after stop or close tray command)
#define READY		0x0400		// READY CDD status (also used for seeking)
#define FAST_FOW	0x0300		// FAST FORWARD track CDD status
#define FAST_REV	0x10300		// FAST REVERSE track CDD status
#define PLAYING		0x0100		// PLAYING audio track CDD status


static int CD_Present = 0;


#define CHECK_TRAY_OPEN				\
if (Pico_mcd->scd.Status_CDD == TRAY_OPEN)	\
{									\
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;	\
									\
	Pico_mcd->cdd.Minute = 0;					\
	Pico_mcd->cdd.Seconde = 0;				\
	Pico_mcd->cdd.Frame = 0;					\
	Pico_mcd->cdd.Ext = 0;					\
									\
	Pico_mcd->scd.CDD_Complete = 1;				\
									\
	return 2;						\
}


#define CHECK_CD_PRESENT			\
if (!CD_Present)					\
{									\
	Pico_mcd->scd.Status_CDD = NOCD;			\
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;	\
									\
	Pico_mcd->cdd.Minute = 0;					\
	Pico_mcd->cdd.Seconde = 0;				\
	Pico_mcd->cdd.Frame = 0;					\
	Pico_mcd->cdd.Ext = 0;					\
									\
	Pico_mcd->scd.CDD_Complete = 1;				\
									\
	return 3;						\
}


static int MSF_to_LBA(PicoMcdMsf *MSF)
{
	return (MSF->M * 60 * 75) + (MSF->S * 75) + MSF->F - 150;
}


void LBA_to_MSF(int lba, PicoMcdMsf *MSF)
{
	if (lba < -150) lba = 0;
	else lba += 150;
	MSF->M = lba / (60 * 75);
	MSF->S = (lba / 75) % 60;
	MSF->F = lba % 75;
}


static unsigned int MSF_to_Track(PicoMcdMsf *MSF)
{
	int i, Start, Cur;

	Start = (MSF->M << 16) + (MSF->S << 8) + MSF->F;

	for(i = 1; i <= (Pico_mcd->TOC.Last_Track + 1); i++)
	{
		Cur = Pico_mcd->TOC.Tracks[i - 1].MSF.M << 16;
		Cur += Pico_mcd->TOC.Tracks[i - 1].MSF.S << 8;
		Cur += Pico_mcd->TOC.Tracks[i - 1].MSF.F;

		if (Cur > Start) break;
	}

	--i;

	if (i > Pico_mcd->TOC.Last_Track) return 100;
	else if (i < 1) i = 1;

	return (unsigned) i;
}


static unsigned int LBA_to_Track(int lba)
{
	PicoMcdMsf MSF;

	LBA_to_MSF(lba, &MSF);
	return MSF_to_Track(&MSF);
}


static void Track_to_MSF(int track, PicoMcdMsf *MSF)
{
	if (track < 1) track = 1;
	else if (track > Pico_mcd->TOC.Last_Track) track = Pico_mcd->TOC.Last_Track;

	MSF->M = Pico_mcd->TOC.Tracks[track - 1].MSF.M;
	MSF->S = Pico_mcd->TOC.Tracks[track - 1].MSF.S;
	MSF->F = Pico_mcd->TOC.Tracks[track - 1].MSF.F;
}


int Track_to_LBA(int track)
{
	PicoMcdMsf MSF;

	Track_to_MSF(track, &MSF);
	return MSF_to_LBA(&MSF);
}


void Check_CD_Command(void)
{
	cdprintf("CHECK CD COMMAND");

	// Check CDC
	if (Pico_mcd->scd.Status_CDC & 1)			// CDC is reading data ...
	{
		cdprintf("Got a read command");

		// DATA ?
		if (Pico_mcd->scd.Cur_Track == 1)
		     Pico_mcd->s68k_regs[0x36] |=  0x01;
		else Pico_mcd->s68k_regs[0x36] &= ~0x01;			// AUDIO

		if (Pico_mcd->scd.File_Add_Delay == 0)
		{
			FILE_Read_One_LBA_CDC();
		}
		else Pico_mcd->scd.File_Add_Delay--;
	}

	// Check CDD
	if (Pico_mcd->scd.CDD_Complete)
	{
		Pico_mcd->scd.CDD_Complete = 0;

		Pico_mcd->cdd.exportStatus();
	}

	if (Pico_mcd->scd.Status_CDD == FAST_FOW)
	{
		Pico_mcd->scd.Cur_LBA += 10;
		Pico_mcd->cdc.updateHeader();

	}
	else if (Pico_mcd->scd.Status_CDD == FAST_REV)
	{
		Pico_mcd->scd.Cur_LBA -= 10;
		if (Pico_mcd->scd.Cur_LBA < -150) Pico_mcd->scd.Cur_LBA = -150;
		Pico_mcd->cdc.updateHeader();
	}
}


int Init_CD_Driver(void)
{
	return 0;
}


void End_CD_Driver(void)
{
	Pico_mcd->TOC.close();
}


void Reset_CD(void)
{
	Pico_mcd->scd.Cur_Track = 0;
	Pico_mcd->scd.Cur_LBA = -150;
	Pico_mcd->scd.Status_CDC &= ~1;
	Pico_mcd->scd.Status_CDD = CD_Present ? READY : NOCD;
	Pico_mcd->scd.CDD_Complete = 0;
	Pico_mcd->scd.File_Add_Delay = 0;
}


bool Insert_CD(const QString &fileName, QString *error)
{

	CD_Present = 0;
	Pico_mcd->scd.Status_CDD = NOCD;

	bool ok = Pico_mcd->TOC.open(fileName, error);
	if (ok) {
		CD_Present = 1;
		Pico_mcd->scd.Status_CDD = READY;
	}

	return ok;
}


void Stop_CD(void)
{
	Pico_mcd->TOC.close();
	CD_Present = 0;
}


/*
void Change_CD(void)
{
	if (Pico_mcd->scd.Status_CDD == TRAY_OPEN) Close_Tray_CDD_cC();
	else Open_Tray_CDD_cD();
}
*/

int Get_Status_CDD_c0(void)
{
	cdprintf("Status command : Cur LBA = %d", Pico_mcd->scd.Cur_LBA);

	// Clear immediat status
	if ((Pico_mcd->cdd.Status & 0x0F00) == 0x0200)
		Pico_mcd->cdd.Status = (Pico_mcd->scd.Status_CDD & 0xFF00) | (Pico_mcd->cdd.Status & 0x00FF);
	else if ((Pico_mcd->cdd.Status & 0x0F00) == 0x0700)
		Pico_mcd->cdd.Status = (Pico_mcd->scd.Status_CDD & 0xFF00) | (Pico_mcd->cdd.Status & 0x00FF);
	else if ((Pico_mcd->cdd.Status & 0x0F00) == 0x0E00)
		Pico_mcd->cdd.Status = (Pico_mcd->scd.Status_CDD & 0xFF00) | (Pico_mcd->cdd.Status & 0x00FF);

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Stop_CDD_c1(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	if (CD_Present) Pico_mcd->scd.Status_CDD = STOPPED;
	else Pico_mcd->scd.Status_CDD = NOCD;
	Pico_mcd->cdd.Status = 0x0000;

	Pico_mcd->s68k_regs[0x36] |= 0x01;			// Data bit set because stopped

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Get_Pos_CDD_c20(void)
{
	PicoMcdMsf MSF;

	cdprintf("command 200 : Cur LBA = %d", Pico_mcd->scd.Cur_LBA);

	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	cdprintf("Status CDD = %.4X  Status = %.4X", Pico_mcd->scd.Status_CDD, Pico_mcd->cdd.Status);

	LBA_to_MSF(Pico_mcd->scd.Cur_LBA, &MSF);

	Pico_mcd->cdd.Minute = INT_TO_BCDW(MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(MSF.F);
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Get_Track_Pos_CDD_c21(void)
{
	int elapsed_time;
	PicoMcdMsf MSF;

	cdprintf("command 201 : Cur LBA = %d", Pico_mcd->scd.Cur_LBA);

	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	elapsed_time = Pico_mcd->scd.Cur_LBA - Track_to_LBA(LBA_to_Track(Pico_mcd->scd.Cur_LBA));
	LBA_to_MSF(elapsed_time - 150, &MSF);

	cdprintf("   elapsed = %d", elapsed_time);

	Pico_mcd->cdd.Minute = INT_TO_BCDW(MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(MSF.F);
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Get_Current_Track_CDD_c22(void)
{
	cdprintf("Status CDD = %.4X  Status = %.4X", Pico_mcd->scd.Status_CDD, Pico_mcd->cdd.Status);

	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	Pico_mcd->scd.Cur_Track = LBA_to_Track(Pico_mcd->scd.Cur_LBA);

	if (Pico_mcd->scd.Cur_Track == 100) Pico_mcd->cdd.Minute = 0x0A02;
	else Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Get_Total_Lenght_CDD_c23(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->TOC.Tracks[Pico_mcd->TOC.Last_Track].MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(Pico_mcd->TOC.Tracks[Pico_mcd->TOC.Last_Track].MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(Pico_mcd->TOC.Tracks[Pico_mcd->TOC.Last_Track].MSF.F);
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Get_First_Last_Track_CDD_c24(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(1);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(Pico_mcd->TOC.Last_Track);
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Get_Track_Adr_CDD_c25(void)
{
	int track_number;

	CHECK_TRAY_OPEN

	// track number in TC4 & TC5

	track_number = (Pico_mcd->s68k_regs[0x38+10+4] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+5] & 0xF);

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	if (track_number > Pico_mcd->TOC.Last_Track) track_number = Pico_mcd->TOC.Last_Track;
	else if (track_number < 1) track_number = 1;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->TOC.Tracks[track_number - 1].MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(Pico_mcd->TOC.Tracks[track_number - 1].MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(Pico_mcd->TOC.Tracks[track_number - 1].MSF.F);
	Pico_mcd->cdd.Ext = track_number % 10;

	if (track_number == 1) Pico_mcd->cdd.Frame |= 0x0800; // data track

	Pico_mcd->scd.CDD_Complete = 1;
	return 0;
}


int Play_CDD_c3(void)
{
	PicoMcdMsf MSF;
	int delay, new_lba;

	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	// MSF of the track to play in TC buffer

	MSF.M = (Pico_mcd->s68k_regs[0x38+10+2] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+3] & 0xF);
	MSF.S = (Pico_mcd->s68k_regs[0x38+10+4] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+5] & 0xF);
	MSF.F = (Pico_mcd->s68k_regs[0x38+10+6] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+7] & 0xF);

	int oldTrack = Pico_mcd->scd.Cur_Track;
	Pico_mcd->scd.Cur_Track = MSF_to_Track(&MSF);

	new_lba = MSF_to_LBA(&MSF);
	delay = new_lba - Pico_mcd->scd.Cur_LBA;
	if (delay < 0) delay = -delay;
	delay >>= 12;

	Pico_mcd->scd.Cur_LBA = new_lba;
	Pico_mcd->cdc.updateHeader();

	cdprintf("Read : Cur LBA = %d, M=%d, S=%d, F=%d", Pico_mcd->scd.Cur_LBA, MSF.M, MSF.S, MSF.F);

	if (Pico_mcd->scd.Status_CDD != PLAYING) delay += 20;

	Pico_mcd->scd.Status_CDD = PLAYING;
	Pico_mcd->cdd.Status = 0x0102;
//	Pico_mcd->cdd.Status = COMM_OK;

	if (Pico_mcd->scd.File_Add_Delay == 0) Pico_mcd->scd.File_Add_Delay = delay;

	if (Pico_mcd->scd.Cur_Track == 1)
	{
		Pico_mcd->s68k_regs[0x36] |=  0x01;				// DATA
		if (oldTrack != 1)
			Pico_mcd->TOC.stopAudio();
	}
	else
	{
		Pico_mcd->s68k_regs[0x36] &= ~0x01;				// AUDIO
		//CD_Audio_Starting = 1;
		Pico_mcd->TOC.playAudio();
	}

	if (Pico_mcd->scd.Cur_Track == 100) Pico_mcd->cdd.Minute = 0x0A02;
	else Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.Status_CDC |= 1;			// Read data with CDC

	Pico_mcd->scd.CDD_Complete = 1;
	return 0;
}


int Seek_CDD_c4(void)
{
	PicoMcdMsf MSF;

	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	// MSF to seek in TC buffer

	MSF.M = (Pico_mcd->s68k_regs[0x38+10+2] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+3] & 0xF);
	MSF.S = (Pico_mcd->s68k_regs[0x38+10+4] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+5] & 0xF);
	MSF.F = (Pico_mcd->s68k_regs[0x38+10+6] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+7] & 0xF);

	Pico_mcd->scd.Cur_Track = MSF_to_Track(&MSF);
	Pico_mcd->scd.Cur_LBA = MSF_to_LBA(&MSF);
	Pico_mcd->cdc.updateHeader();

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	Pico_mcd->scd.Status_CDD = READY;
	Pico_mcd->cdd.Status = 0x0200;

	// DATA ?
	if (Pico_mcd->scd.Cur_Track == 1)
	     Pico_mcd->s68k_regs[0x36] |=  0x01;
	else Pico_mcd->s68k_regs[0x36] &= ~0x01;		// AUDIO

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Pause_CDD_c6(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;			// Stop CDC read to start a new one if raw data

	Pico_mcd->scd.Status_CDD = READY;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;

	Pico_mcd->s68k_regs[0x36] |= 0x01;		// Data bit set because stopped

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	if (Pico_mcd->scd.Cur_Track != 1)
		Pico_mcd->TOC.stopAudio();
	return 0;
}


int Resume_CDD_c7(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Cur_Track = LBA_to_Track(Pico_mcd->scd.Cur_LBA);

#ifdef DEBUG_CD
	{
		PicoMcdMsf MSF;
		LBA_to_MSF(Pico_mcd->scd.Cur_LBA, &MSF);
		cdprintf("Resume read : Cur LBA = %d, M=%d, S=%d, F=%d", Pico_mcd->scd.Cur_LBA, MSF.M, MSF.S, MSF.F);
	}
#endif

	Pico_mcd->scd.Status_CDD = PLAYING;
	Pico_mcd->cdd.Status = 0x0102;

	if (Pico_mcd->scd.Cur_Track == 1)
	{
		Pico_mcd->s68k_regs[0x36] |=  0x01;				// DATA
	}
	else
	{
		Pico_mcd->s68k_regs[0x36] &= ~0x01;				// AUDIO
		//CD_Audio_Starting = 1;
		Pico_mcd->TOC.playAudio();
	}

	if (Pico_mcd->scd.Cur_Track == 100) Pico_mcd->cdd.Minute = 0x0A02;
	else Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.Status_CDC |= 1;			// Read data with CDC

	Pico_mcd->scd.CDD_Complete = 1;
	return 0;
}


int Fast_Foward_CDD_c8(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	Pico_mcd->scd.Status_CDD = FAST_FOW;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD | 2;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Fast_Rewind_CDD_c9(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	Pico_mcd->scd.Status_CDD = FAST_REV;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD | 2;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Close_Tray_CDD_cC(void)
{
	CD_Present = 0;
	//Clear_Sound_Buffer();

	Pico_mcd->scd.Status_CDC &= ~1;			// Stop CDC read

	elprintf(EL_STATUS, "tray close\n");

	if (PicoMCDcloseTray != NULL)
		CD_Present = PicoMCDcloseTray();

	Pico_mcd->scd.Status_CDD = CD_Present ? STOPPED : NOCD;
	Pico_mcd->cdd.Status = 0x0000;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int Open_Tray_CDD_cD(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->scd.Status_CDC &= ~1;			// Stop CDC read

	elprintf(EL_STATUS, "tray open\n");

	Pico_mcd->TOC.close();
	CD_Present = 0;

	if (PicoMCDopenTray != NULL)
		PicoMCDopenTray();

	Pico_mcd->scd.Status_CDD = TRAY_OPEN;
	Pico_mcd->cdd.Status = 0x0E00;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int CDD_cA(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;

	Pico_mcd->scd.Status_CDD = READY;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(1);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(1);
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.CDD_Complete = 1;

	return 0;
}


int CDD_Def(void)
{
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	return 0;
}
