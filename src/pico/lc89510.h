/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2002 by Stéphane Dallongeville
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#ifndef PICOLC89510_H
#define PICOLC89510_H

#include <base/emu.h>

class CDC
{
public:
	void reset();
	void updateHeader();

	u8 Buffer[(32 * 1024 * 2) + 2352];
//	uint Host_Data;		// unused
//	uint DMA_Adr;		// 0A
//	uint Stop_Watch;	// 0C
	uint COMIN;
	uint IFSTAT;
	union
	{
		struct
		{
			u8 L;
			u8 H;
			u16 unused;
		} B;
		int N;
	} DBC;
	union
	{
		struct
		{
			u8 L;
			u8 H;
			u16 unused;
		} B;
		int N;
	} DAC;
	union
	{
		struct
		{
			u8 B0;
			u8 B1;
			u8 B2;
			u8 B3;
		} B;
		uint N;
	} HEAD;
	union
	{
		struct
		{
			u8 L;
			u8 H;
			u16 unused;
		} B;
		int N;
	} PT;
	union
	{
		struct
		{
			u8 L;
			u8 H;
			u16 unused;
		} B;
		int N;
	} WA;
	union
	{
		struct
		{
			u8 B0;
			u8 B1;
			u8 B2;
			u8 B3;
		} B;
		uint N;
	} STAT;
	uint SBOUT;
	uint IFCTRL;
	union
	{
		struct
		{
			u8 B0;
			u8 B1;
			u8 B2;
			u8 B3;
		} B;
		uint N;
	} CTRL;
	uint Decode_Reg_Read;
};

class CDD
{
public:
	void reset();
	void exportStatus();
//	u16 Fader;	// 34
//	u16 Control;	// 36
//	u16 Cur_Comm;// unused

	// "Receive status"
	u16 Status;
	u16 Minute;
	u16 Seconde;
	u16 Frame;
	u8 Ext;
	u8 pad[3];
};


extern "C" u16 Read_CDC_Host(int is_sub);

void lc89510Reset();
void Update_CDC_TRansfer(int which);

u8 CDC_Read_Reg();
void CDC_Write_Reg(u8 Data);

void CDD_Import_Command();

#endif // PICOLC89510_H
