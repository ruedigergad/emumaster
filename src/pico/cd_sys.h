/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2002 by Stéphane Dallongeville
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#ifndef PICOMCDSYS_H
#define PICOMCDSYS_H

#include "cd_file.h"
#include <QFile>

class PicoMcdMsf
{
public:
	u8 M; // minutes
	u8 S; // seconds
	u8 F; // frames
};

class PicoMcdTrack
{
public:
	enum Type { None, Iso, Bin, Mp3 };

	Type type;
	PicoMcdMsf MSF;
	int Length;
	QFile *file;
};

class PicoMcdToc
{
public:
	bool open(const QString &fileName, QString *error);
	void close();
	int region() const;
	bool playAudio();
	void stopAudio();
	void playTrack(int index, int offset);

	PicoMcdTrack Tracks[100];
	int Last_Track;
private:
	bool detectDataTypeAndRegion(QFile *file);
	void searchForMp3Files();
	void insertMp3File(int index, const QString &fileName);

	int m_region;
};

class PicoMcdScd
{
public:
	uint Status_CDD;
	uint Status_CDC;
	int Cur_LBA;
	uint Cur_Track;
	int File_Add_Delay;
	int CDD_Complete;
};

#ifdef __cplusplus
extern "C" {
#endif


#define INT_TO_BCDB(c)										\
((c) > 99)?(0x99):((((c) / 10) << 4) + ((c) % 10));

#define INT_TO_BCDW(c)										\
((c) > 99)?(0x0909):((((c) / 10) << 8) + ((c) % 10));

#define BCDB_TO_INT(c)										\
(((c) >> 4) * 10) + ((c) & 0xF);

#define BCDW_TO_INT(c)										\
(((c) >> 8) * 10) + ((c) & 0xF);


void LBA_to_MSF(int lba, PicoMcdMsf *MSF);
int  Track_to_LBA(int track);

// moved to Pico.h
// int  Insert_CD(char *iso_name, int is_bin);
// void Stop_CD(void);

void Check_CD_Command(void);

int  Init_CD_Driver(void);
void End_CD_Driver(void);
void Reset_CD(void);

int Get_Status_CDD_c0(void);
int Stop_CDD_c1(void);
int Get_Pos_CDD_c20(void);
int Get_Track_Pos_CDD_c21(void);
int Get_Current_Track_CDD_c22(void);
int Get_Total_Lenght_CDD_c23(void);
int Get_First_Last_Track_CDD_c24(void);
int Get_Track_Adr_CDD_c25(void);
int Play_CDD_c3(void);
int Seek_CDD_c4(void);
int Pause_CDD_c6(void);
int Resume_CDD_c7(void);
int Fast_Foward_CDD_c8(void);
int Fast_Rewind_CDD_c9(void);
int CDD_cA(void);
int Close_Tray_CDD_cC(void);
int Open_Tray_CDD_cD(void);

int CDD_Def(void);


#ifdef __cplusplus
}
#endif

#endif // PICOMCDSYS_H

