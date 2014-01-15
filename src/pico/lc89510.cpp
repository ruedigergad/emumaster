/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2002 by Stéphane Dallongeville
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"

#define cdprintf(x...)

#define CDC_DMA_SPEED 256

void CDD::reset()
{
	memset(Pico_mcd->s68k_regs+0x34, 0, 2*2); // CDD.Fader, CDD.Control
	Status = 0;
	Minute = 0;
	Seconde = 0;
	Frame = 0;
	Ext = 0;

	// clear receive status and transfer command
	memset(Pico_mcd->s68k_regs+0x38, 0, 20);
	Pico_mcd->s68k_regs[0x38+9] = 0xF;		// Default checksum
}

void CDC::reset()
{
	memset(Buffer, 0, sizeof(Buffer));

	COMIN = 0;
	IFSTAT = 0xFF;
	DAC.N = 0;
	DBC.N = 0;
	HEAD.N = 0x01000000;
	PT.N = 0;
	WA.N = 2352 * 2;
	STAT.N = 0x00000080;
	SBOUT = 0;
	IFCTRL = 0;
	CTRL.N = 0;

	Decode_Reg_Read = 0;

	Pico_mcd->scd.Status_CDC &= ~0x08;
}

void lc89510Reset()
{
	Pico_mcd->cdd.reset();
	Pico_mcd->cdc.reset();

	// clear DMA_Adr & Stop_Watch
	memset(Pico_mcd->s68k_regs + 0xA, 0, 4);
}

void Update_CDC_TRansfer(int which)
{
	unsigned int DMA_Adr, dep, length;
	unsigned short *dest;
	unsigned char  *src;

	if (Pico_mcd->cdc.DBC.N <= (CDC_DMA_SPEED * 2))
	{
		length = (Pico_mcd->cdc.DBC.N + 1) >> 1;
		Pico_mcd->scd.Status_CDC &= ~0x08;	// Last transfer
		Pico_mcd->s68k_regs[4] |=  0x80;	// End data transfer
		Pico_mcd->s68k_regs[4] &= ~0x40;	// no more data ready
		Pico_mcd->cdc.IFSTAT |= 0x08;		// No more data transfer in progress

		if (Pico_mcd->cdc.IFCTRL & 0x40)	// DTEIEN = Data Trasnfer End Interrupt Enable ?
		{
			Pico_mcd->cdc.IFSTAT &= ~0x40;

			if (Pico_mcd->s68k_regs[0x33] & (1<<5))
			{
				elprintf(EL_INTS, "cdc DTE irq 5");
				SekInterruptS68k(5);
			}
		}
	}
	else length = CDC_DMA_SPEED;


	// TODO: dst bounds checking?
	src = Pico_mcd->cdc.Buffer + Pico_mcd->cdc.DAC.N;
	DMA_Adr = (Pico_mcd->s68k_regs[0xA]<<8) | Pico_mcd->s68k_regs[0xB];

	if (which == 7) // WORD RAM
	{
		if (Pico_mcd->s68k_regs[3] & 4)
		{
			// test: Final Fight
			int bank = !(Pico_mcd->s68k_regs[3]&1);
			dep = ((DMA_Adr & 0x3FFF) << 3);
			cdprintf("CD DMA # %04x -> word_ram1M # %06x, len=%i",
					Pico_mcd->cdc.DAC.N, dep, length);

			dest = (unsigned short *) (Pico_mcd->word_ram1M[bank] + dep);

			memcpy16bswap(dest, src, length);

			/*{ // debug
				unsigned char *b1 = Pico_mcd->word_ram1M[bank] + dep;
				unsigned char *b2 = (unsigned char *)(dest+length) - 8;
				dprintf("%02x %02x %02x %02x .. %02x %02x %02x %02x",
					b1[0], b1[1], b1[4], b1[5], b2[0], b2[1], b2[4], b2[5]);
			}*/
		}
		else
		{
			dep = ((DMA_Adr & 0x7FFF) << 3);
			cdprintf("CD DMA # %04x -> word_ram2M # %06x, len=%i",
					Pico_mcd->cdc.DAC.N, dep, length);
			dest = (unsigned short *) (Pico_mcd->word_ram2M + dep);

			memcpy16bswap(dest, src, length);

			/*{ // debug
				unsigned char *b1 = Pico_mcd->word_ram2M + dep;
				unsigned char *b2 = (unsigned char *)(dest+length) - 4;
				dprintf("%02x %02x %02x %02x .. %02x %02x %02x %02x",
					b1[0], b1[1], b1[2], b1[3], b2[0], b2[1], b2[2], b2[3]);
			}*/
		}
	}
	else if (which == 4) // PCM RAM (check: popful Mail)
	{
		dep = (DMA_Adr & 0x03FF) << 2;
		cdprintf("CD DMA # %04x -> PCM[%i] # %04x, len=%i",
			Pico_mcd->cdc.DAC.N, Pico_mcd->pcm.bank, dep, length);
		dest = (unsigned short *) (Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank] + dep);

		if (Pico_mcd->cdc.DAC.N & 1) /* unaligned src? */
			memcpy(dest, src, length*2);
		else	memcpy16(dest, (unsigned short *) src, length);
	}
	else if (which == 5) // PRG RAM
	{
		dep = DMA_Adr << 3;
		dest = (unsigned short *) (Pico_mcd->prg_ram + dep);
		cdprintf("CD DMA # %04x -> prg_ram # %06x, len=%i",
				Pico_mcd->cdc.DAC.N, dep, length);

		memcpy16bswap(dest, src, length);

		/*{ // debug
			unsigned char *b1 = Pico_mcd->prg_ram + dep;
			unsigned char *b2 = (unsigned char *)(dest+length) - 4;
			dprintf("%02x %02x %02x %02x .. %02x %02x %02x %02x",
				b1[0], b1[1], b1[2], b1[3], b2[0], b2[1], b2[2], b2[3]);
		}*/
	}

	length <<= 1;
	Pico_mcd->cdc.DAC.N = (Pico_mcd->cdc.DAC.N + length) & 0xFFFF;
	if (Pico_mcd->scd.Status_CDC & 0x08) Pico_mcd->cdc.DBC.N -= length;
	else Pico_mcd->cdc.DBC.N = 0;

	// update DMA_Adr
	length >>= 2;
	if (which != 4) length >>= 1;
	DMA_Adr += length;
	Pico_mcd->s68k_regs[0xA] = DMA_Adr >> 8;
	Pico_mcd->s68k_regs[0xB] = DMA_Adr;
}


u16 Read_CDC_Host(int is_sub)
{
	int addr;

	if (!(Pico_mcd->scd.Status_CDC & 0x08))
	{
		// Transfer data disabled
		cdprintf("Read_CDC_Host FIXME: Transfer data disabled");
		return 0;
	}

	if ((is_sub && (Pico_mcd->s68k_regs[4] & 7) != 3) ||
		(!is_sub && (Pico_mcd->s68k_regs[4] & 7) != 2))
	{
		// Wrong setting
		cdprintf("Read_CDC_Host FIXME: Wrong setting");
		return 0;
	}

	Pico_mcd->cdc.DBC.N -= 2;

	if (Pico_mcd->cdc.DBC.N <= 0)
	{
		Pico_mcd->cdc.DBC.N = 0;
		Pico_mcd->scd.Status_CDC &= ~0x08;		// Last transfer
		Pico_mcd->s68k_regs[4] |=  0x80;		// End data transfer
		Pico_mcd->s68k_regs[4] &= ~0x40;		// no more data ready
		Pico_mcd->cdc.IFSTAT |= 0x08;			// No more data transfer in progress

		if (Pico_mcd->cdc.IFCTRL & 0x40)		// DTEIEN = Data Transfer End Interrupt Enable ?
		{
			Pico_mcd->cdc.IFSTAT &= ~0x40;

			if (Pico_mcd->s68k_regs[0x33]&(1<<5)) {
				elprintf(EL_INTS, "m68k: s68k irq 5");
				SekInterruptS68k(5);
			}

			cdprintf("CDC - DTE interrupt");
		}
	}

	addr = Pico_mcd->cdc.DAC.N;
	Pico_mcd->cdc.DAC.N += 2;

	cdprintf("Read_CDC_Host sub=%i d=%04x dac=%04x dbc=%04x", is_sub,
		(Pico_mcd->cdc.Buffer[addr]<<8) | Pico_mcd->cdc.Buffer[addr+1], Pico_mcd->cdc.DAC.N, Pico_mcd->cdc.DBC.N);

	return (Pico_mcd->cdc.Buffer[addr]<<8) | Pico_mcd->cdc.Buffer[addr+1];
}

void CDC::updateHeader()
{
	if (CTRL.B.B1 & 0x01) { // Sub-Header wanted ?
		HEAD.B.B0 = 0;
		HEAD.B.B1 = 0;
		HEAD.B.B2 = 0;
		HEAD.B.B3 = 0;
	} else {
		PicoMcdMsf MSF;

		LBA_to_MSF(Pico_mcd->scd.Cur_LBA, &MSF);

		HEAD.B.B0 = INT_TO_BCDB(MSF.M);
		HEAD.B.B1 = INT_TO_BCDB(MSF.S);
		HEAD.B.B2 = INT_TO_BCDB(MSF.F);
		HEAD.B.B3 = 0x01;
	}
}


u8 CDC_Read_Reg()
{
	unsigned char ret;

	switch(Pico_mcd->s68k_regs[5] & 0xF)
	{
		case 0x0: // COMIN
			cdprintf("CDC read reg 00 = %.2X", Pico_mcd->cdc.COMIN);

			Pico_mcd->s68k_regs[5] = 0x1;
			return Pico_mcd->cdc.COMIN;

		case 0x1: // IFSTAT
			cdprintf("CDC read reg 01 = %.2X", Pico_mcd->cdc.IFSTAT);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 1);		// Reg 1 (decoding)
			Pico_mcd->s68k_regs[5] = 0x2;
			return Pico_mcd->cdc.IFSTAT;

		case 0x2: // DBCL
			cdprintf("CDC read reg 02 = %.2X", Pico_mcd->cdc.DBC.B.L);

			Pico_mcd->s68k_regs[5] = 0x3;
			return Pico_mcd->cdc.DBC.B.L;

		case 0x3: // DBCH
			cdprintf("CDC read reg 03 = %.2X", Pico_mcd->cdc.DBC.B.H);

			Pico_mcd->s68k_regs[5] = 0x4;
			return Pico_mcd->cdc.DBC.B.H;

		case 0x4: // HEAD0
			cdprintf("CDC read reg 04 = %.2X", Pico_mcd->cdc.HEAD.B.B0);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 4);		// Reg 4 (decoding)
			Pico_mcd->s68k_regs[5] = 0x5;
			return Pico_mcd->cdc.HEAD.B.B0;

		case 0x5: // HEAD1
			cdprintf("CDC read reg 05 = %.2X", Pico_mcd->cdc.HEAD.B.B1);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 5);		// Reg 5 (decoding)
			Pico_mcd->s68k_regs[5] = 0x6;
			return Pico_mcd->cdc.HEAD.B.B1;

		case 0x6: // HEAD2
			cdprintf("CDC read reg 06 = %.2X", Pico_mcd->cdc.HEAD.B.B2);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 6);		// Reg 6 (decoding)
			Pico_mcd->s68k_regs[5] = 0x7;
			return Pico_mcd->cdc.HEAD.B.B2;

		case 0x7: // HEAD3
			cdprintf("CDC read reg 07 = %.2X", Pico_mcd->cdc.HEAD.B.B3);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 7);		// Reg 7 (decoding)
			Pico_mcd->s68k_regs[5] = 0x8;
			return Pico_mcd->cdc.HEAD.B.B3;

		case 0x8: // PTL
			cdprintf("CDC read reg 08 = %.2X", Pico_mcd->cdc.PT.B.L);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 8);		// Reg 8 (decoding)
			Pico_mcd->s68k_regs[5] = 0x9;
			return Pico_mcd->cdc.PT.B.L;

		case 0x9: // PTH
			cdprintf("CDC read reg 09 = %.2X", Pico_mcd->cdc.PT.B.H);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 9);		// Reg 9 (decoding)
			Pico_mcd->s68k_regs[5] = 0xA;
			return Pico_mcd->cdc.PT.B.H;

		case 0xA: // WAL
			cdprintf("CDC read reg 10 = %.2X", Pico_mcd->cdc.WA.B.L);

			Pico_mcd->s68k_regs[5] = 0xB;
			return Pico_mcd->cdc.WA.B.L;

		case 0xB: // WAH
			cdprintf("CDC read reg 11 = %.2X", Pico_mcd->cdc.WA.B.H);

			Pico_mcd->s68k_regs[5] = 0xC;
			return Pico_mcd->cdc.WA.B.H;

		case 0xC: // STAT0
			cdprintf("CDC read reg 12 = %.2X", Pico_mcd->cdc.STAT.B.B0);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 12);		// Reg 12 (decoding)
			Pico_mcd->s68k_regs[5] = 0xD;
			return Pico_mcd->cdc.STAT.B.B0;

		case 0xD: // STAT1
			cdprintf("CDC read reg 13 = %.2X", Pico_mcd->cdc.STAT.B.B1);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 13);		// Reg 13 (decoding)
			Pico_mcd->s68k_regs[5] = 0xE;
			return Pico_mcd->cdc.STAT.B.B1;

		case 0xE: // STAT2
			cdprintf("CDC read reg 14 = %.2X", Pico_mcd->cdc.STAT.B.B2);

			Pico_mcd->cdc.Decode_Reg_Read |= (1 << 14);		// Reg 14 (decoding)
			Pico_mcd->s68k_regs[5] = 0xF;
			return Pico_mcd->cdc.STAT.B.B2;

		case 0xF: // STAT3
			cdprintf("CDC read reg 15 = %.2X", Pico_mcd->cdc.STAT.B.B3);

			ret = Pico_mcd->cdc.STAT.B.B3;
			Pico_mcd->cdc.IFSTAT |= 0x20;			// decoding interrupt flag cleared
			if ((Pico_mcd->cdc.CTRL.B.B0 & 0x80) && (Pico_mcd->cdc.IFCTRL & 0x20))
			{
				if ((Pico_mcd->cdc.Decode_Reg_Read & 0x73F2) == 0x73F2)
					Pico_mcd->cdc.STAT.B.B3 = 0x80;
			}
			return ret;
	}

	return 0;
}


void CDC_Write_Reg(u8 Data)
{
	cdprintf("CDC write reg%02d = %.2X", Pico_mcd->s68k_regs[5] & 0xF, Data);

	switch (Pico_mcd->s68k_regs[5] & 0xF)
	{
		case 0x0: // SBOUT
			Pico_mcd->s68k_regs[5] = 0x1;
			Pico_mcd->cdc.SBOUT = Data;

			break;

		case 0x1: // IFCTRL
			Pico_mcd->s68k_regs[5] = 0x2;
			Pico_mcd->cdc.IFCTRL = Data;

			if ((Pico_mcd->cdc.IFCTRL & 0x02) == 0)		// Stop data transfer
			{
				Pico_mcd->cdc.DBC.N = 0;
				Pico_mcd->scd.Status_CDC &= ~0x08;
				Pico_mcd->cdc.IFSTAT |= 0x08;		// No more data transfer in progress
			}
			break;

		case 0x2: // DBCL
			Pico_mcd->s68k_regs[5] = 0x3;
			Pico_mcd->cdc.DBC.B.L = Data;

			break;

		case 0x3: // DBCH
			Pico_mcd->s68k_regs[5] = 0x4;
			Pico_mcd->cdc.DBC.B.H = Data;

			break;

		case 0x4: // DACL
			Pico_mcd->s68k_regs[5] = 0x5;
			Pico_mcd->cdc.DAC.B.L = Data;

			break;

		case 0x5: // DACH
			Pico_mcd->s68k_regs[5] = 0x6;
			Pico_mcd->cdc.DAC.B.H = Data;

			break;

		case 0x6: // DTTRG
			if (Pico_mcd->cdc.IFCTRL & 0x02)		// Data transfer enable ?
			{
				Pico_mcd->cdc.IFSTAT &= ~0x08;		// Data transfer in progress
				Pico_mcd->scd.Status_CDC |= 0x08;	// Data transfer in progress
				Pico_mcd->s68k_regs[4] &= 0x7F;		// A data transfer start

				cdprintf("************** Starting Data Transfer ***********");
				cdprintf("RS0 = %.4X  DAC = %.4X  DBC = %.4X  DMA adr = %.4X\n\n", Pico_mcd->s68k_regs[4]<<8,
					Pico_mcd->cdc.DAC.N, Pico_mcd->cdc.DBC.N, (Pico_mcd->s68k_regs[0xA]<<8) | Pico_mcd->s68k_regs[0xB]);
			}
			break;

		case 0x7: // DTACK
			Pico_mcd->cdc.IFSTAT |= 0x40;			// end data transfer interrupt flag cleared
			break;

		case 0x8: // WAL
			Pico_mcd->s68k_regs[5] = 0x9;
			Pico_mcd->cdc.WA.B.L = Data;

			break;

		case 0x9: // WAH
			Pico_mcd->s68k_regs[5] = 0xA;
			Pico_mcd->cdc.WA.B.H = Data;

			break;

		case 0xA: // CTRL0
			Pico_mcd->s68k_regs[5] = 0xB;
			Pico_mcd->cdc.CTRL.B.B0 = Data;

			break;

		case 0xB: // CTRL1
			Pico_mcd->s68k_regs[5] = 0xC;
			Pico_mcd->cdc.CTRL.B.B1 = Data;

			break;

		case 0xC: // PTL
			Pico_mcd->s68k_regs[5] = 0xD;
			Pico_mcd->cdc.PT.B.L = Data;

			break;

		case 0xD: // PTH
			Pico_mcd->s68k_regs[5] = 0xE;
			Pico_mcd->cdc.PT.B.H = Data;

			break;

		case 0xE: // CTRL2
			Pico_mcd->cdc.CTRL.B.B2 = Data;
			break;

		case 0xF: // RESET
			Pico_mcd->cdc.reset();
			break;
	}
}


static int bswapwrite(int a, unsigned short d)
{
	*(unsigned short *)(Pico_mcd->s68k_regs + a) = (d>>8)|(d<<8);
	return d + (d >> 8);
}

void CDD::exportStatus()
{
	uint csum;

	csum  = bswapwrite( 0x38+0, Status);
	csum += bswapwrite( 0x38+2, Minute);
	csum += bswapwrite( 0x38+4, Seconde);
	csum += bswapwrite( 0x38+6, Frame);
	Pico_mcd->s68k_regs[0x38+8] = Ext;
	csum += Ext;
	Pico_mcd->s68k_regs[0x38+9] = ~csum & 0xf; // checksum

	Pico_mcd->s68k_regs[0x37] &= 3; // CDD.Control

	if (Pico_mcd->s68k_regs[0x33] & (1<<4))
		SekInterruptS68k(4);
}


void CDD_Import_Command()
{
	switch (Pico_mcd->s68k_regs[0x38+10+0])
	{
		case 0x0:	// STATUS (?)
			Get_Status_CDD_c0();
			break;

		case 0x1:	// STOP ALL (?)
			Stop_CDD_c1();
			break;

		case 0x2:	// GET TOC INFORMATIONS
			switch(Pico_mcd->s68k_regs[0x38+10+3])
			{
				case 0x0:	// get current position (MSF format)
					Pico_mcd->cdd.Status = (Pico_mcd->cdd.Status & 0xFF00);
					Get_Pos_CDD_c20();
					break;

				case 0x1:	// get elapsed time of current track played/scanned (relative MSF format)
					Pico_mcd->cdd.Status = (Pico_mcd->cdd.Status & 0xFF00) | 1;
					Get_Track_Pos_CDD_c21();
					break;

				case 0x2:	// get current track in RS2-RS3
					Pico_mcd->cdd.Status =  (Pico_mcd->cdd.Status & 0xFF00) | 2;
					Get_Current_Track_CDD_c22();
					break;

				case 0x3:	// get total length (MSF format)
					Pico_mcd->cdd.Status = (Pico_mcd->cdd.Status & 0xFF00) | 3;
					Get_Total_Lenght_CDD_c23();
					break;

				case 0x4:	// first & last track number
					Pico_mcd->cdd.Status = (Pico_mcd->cdd.Status & 0xFF00) | 4;
					Get_First_Last_Track_CDD_c24();
					break;

				case 0x5:	// get track addresse (MSF format)
					Pico_mcd->cdd.Status = (Pico_mcd->cdd.Status & 0xFF00) | 5;
					Get_Track_Adr_CDD_c25();
					break;

				default :	// invalid, then we return status
					Pico_mcd->cdd.Status = (Pico_mcd->cdd.Status & 0xFF00) | 0xF;
					Get_Status_CDD_c0();
					break;
			}
			break;

		case 0x3:	// READ
			Play_CDD_c3();
			break;

		case 0x4:	// SEEK
			Seek_CDD_c4();
			break;

		case 0x6:	// PAUSE/STOP
			Pause_CDD_c6();
			break;

		case 0x7:	// RESUME
			Resume_CDD_c7();
			break;

		case 0x8:	// FAST FOWARD
			Fast_Foward_CDD_c8();
			break;

		case 0x9:	// FAST REWIND
			Fast_Rewind_CDD_c9();
			break;

		case 0xA:	// RECOVER INITIAL STATE (?)
			CDD_cA();
			break;

		case 0xC:	// CLOSE TRAY
			Close_Tray_CDD_cC();
			break;

		case 0xD:	// OPEN TRAY
			Open_Tray_CDD_cD();
			break;

		default:	// UNKNOWN
			CDD_Def();
			break;
	}
}
