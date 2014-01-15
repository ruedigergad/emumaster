/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AMIGADISK_H
#define AMIGADISK_H

#include <base/emu.h>
#include <QFile>

typedef enum { TRACK_AMIGADOS, TRACK_RAW, TRACK_RAW1 } image_tracktype;
class AmigaDriveTrackId {
public:
	u32 len;
	u32 offs;
	u32 bitlen;
	u32 sync;
	u32 type;
};

class AmigaDrive {
public:
	static const uint MaxTracks = 328;

	enum FileType { ADF_NORMAL, ADF_EXT1, ADF_EXT2 };
	enum ID {
		DRIVE_ID_NONE	= 0xFFFFFFFF,
		DRIVE_ID_35DD	= 0x00000000,
		DRIVE_ID_35HD	= 0xAAAAAAAA,
		DRIVE_ID_525DD	= 0x55555555 /* 40 track 5.25 drive , kickstart does not recognize this */
	};
	void reset();
	bool isEmpty() const { return !file.isOpen(); }
	bool insertDisk(const QString &fileName);
	void ejectDisk();

	bool isAtTrack0() const { return cyl == 0; }
	bool isWriteProtected() const { return !file.isWritable(); }
	bool isRunning() const { return motorOn; }
	void readData(u8 *dst, int track, int offset, int len);
	void fillBigBuffer();
	void updateFile(uint len, u8 type);
	void step();
	void setMotorEnabled(bool on);
	int writeDataAmigaDos();
	int writeDataExt2();
	void writeData();
	void updateReadyTime();
	void updateHsyncWrite();
	void updateHsyncRead();
	void sl(int n);

	QFile file;
	FileType fileType;
	AmigaDriveTrackId trackData[MaxTracks];
	int cyl;
	int bufferedCyl;
	int bufferedSide;
	bool motorOn;
	u16 bigMfmBuf[0x8000];
	int mfmPos;
	int trackLen;
	int trackSpeed;
	u32 numTracks;
	u32 numSectorsPerTrack;
	bool diskChanged;
	bool diskReady;
	int diskReadyTime;
	int stepLimit;
	int ddhd; /* 1=DD 2=HD */
	int idShifter; /* drive id shift counter */
	u32 id; /* drive id to be reported */
private:
	void setTypeID();
	void insertDiskHandleADF_EXT2();
	void insertDiskHandleADF_EXT1();
	void insertDiskHandleADF_NORMAL();
};

extern AmigaDrive amigaDrives[4];
extern int amigaNumDrives;

extern void amigaDiskReset();
extern void amigaDiskSelect(u8 data);
extern u8 amigaDiskStatus();
extern void amigaDiskHandler();
extern void amigaDiskUpdateHsync();
extern void amigaDiskSl();

extern void DSKLEN(u16 v);
extern u16 DSKDATR(int hpos);
extern u16 DSKBYTR(int hpos);
extern void DSKDAT(u16 v);
extern void DSKSYNC(u16 v);
extern void DSKPTL(u16 v);
extern void DSKPTH(u16 v);

inline void AmigaDrive::updateReadyTime() {
	if (diskReadyTime) {
		diskReadyTime--;
		if (diskReadyTime == 0)
			diskReady = true;
	}
}

inline void amigaDiskUpdateReadyTime() {
	for (int i = 0; i < amigaNumDrives; i++)
		amigaDrives[i].updateReadyTime();
}

#endif // AMIGADISK_H
