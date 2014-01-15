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

#include "mem.h"
#include "events.h"
#include "custom.h"
#include "disk.h"
#include "cia.h"
#include "cpu.h"
#include <QDataStream>

/* writable track length with normal 2us bitcell/300RPM motor */
#define FLOPPY_WRITE_LEN (12650 / 2)
/* This works out to 341 */
#define FLOPPY_GAP_LEN (FLOPPY_WRITE_LEN - 11 * 544)
/* (cycles/bitcell) << 8, normal = ((2us/280ns)<<8) = ~1830 */
#define NORMAL_FLOPPY_SPEED 1830
#define MAX_DISK_WORDS_PER_LINE 50	/* depends on floppy_speed */
#define DISK_INDEXSYNC 1
#define DISK_WORDSYNC 2

static u16 dskbytrTab[MAX_DISK_WORDS_PER_LINE * 2 + 1];
static u8 dskbytrCycle[MAX_DISK_WORDS_PER_LINE * 2 + 1];
static short wordsyncCycle[MAX_DISK_WORDS_PER_LINE * 2 + 1];
static u32 dmaTab[MAX_DISK_WORDS_PER_LINE + 1];
static u8 diskSyncTab[MAXHPOS];
static int diskSyncCycle;
static int dskdmaen;
static int dsklength;
static u16 dsksync;
static u32 dskpt;

static int dmaEnabled;
static int bitOffset;
static u32 word;

/* Always carried through to the next line.  */
static int diskHpos;
static int lineCounter;
static int diskSide;
static int diskDirection = 0;
static u8 diskSelected = 0xF;
static u8 diskSelectPrevdata = 0;
static u8 diskDisabledMask = 0;
static u8 writeBuffer[544 * 22];
static int step;
static int uhrDskbytrLast = 0;
static int uhrWordSyncLast = -1;

AmigaDrive amigaDrives[4];
int amigaNumDrives = 4;

/* Simulate exact behaviour of an A3000T 3.5 HD disk drive. 
 * The drive reports to be a 3.5 DD drive whenever there is no
 * disk or a 3.5 DD disk is inserted. Only 3.5 HD drive id is reported
 * when a real 3.5 HD disk is inserted. -Adil
 */
inline void AmigaDrive::setTypeID() {
	if (isEmpty() || numSectorsPerTrack == 11) {
		/* Double density */
		id = DRIVE_ID_35DD;
		ddhd = 1;
	} else {
		/* High density */
		id = DRIVE_ID_35HD;
		ddhd = 2;
	}
	idShifter = 0;
}

void AmigaDrive::ejectDisk() {
	if (file.isOpen()) {
		file.close();
		diskChanged = true;
		setTypeID();
	}
}

/* UAE-1ADF (ADF_EXT2)
 * L	number of tracks (default 2*80=160)
 * L	type, 0=normal AmigaDOS track, 1 = raw MFM
 * L	available space for track in bytes
 * L	track length in bits
 */
inline void AmigaDrive::insertDiskHandleADF_EXT2() {
	fileType = ADF_EXT2;
	numSectorsPerTrack = 11;

	QDataStream s(&file);
	s.setByteOrder(QDataStream::BigEndian);
	s >> numTracks;
	int offs = 8+4 + numTracks*(3*4);
	for (uint i = 0; i < numTracks; i++) {
		u32 type, len, bitlen;
		s >> type >> len >> bitlen;
		AmigaDriveTrackId *tid = &trackData[i];
		tid->type = type;
		tid->len = len;
		tid->bitlen = bitlen;
		tid->offs = offs;
		if (tid->type == TRACK_AMIGADOS) {
			if (tid->len > 20000)
				numSectorsPerTrack = 22;
		}
		offs += tid->len;
	}
}

inline void AmigaDrive::insertDiskHandleADF_EXT1() {
	fileType = ADF_EXT1;
	numTracks = 160;
	numSectorsPerTrack = 11;

	QDataStream s(&file);
	s.setByteOrder(QDataStream::BigEndian);
	int offs = 160 * 4 + 8;
	for (int i = 0; i < 160; i++) {
		AmigaDriveTrackId *tid = &trackData[i];
		u16 sync, len;
		s >> sync >> len;
		tid->sync = sync;
		tid->len = len;
		tid->offs = offs;
		if (tid->sync == 0) {
			tid->type = TRACK_AMIGADOS;
			tid->bitlen = 0;
		} else {
			tid->type = TRACK_RAW1;
			tid->bitlen = tid->len * 8;
		}
		offs += tid->len;
	}
}

inline void AmigaDrive::insertDiskHandleADF_NORMAL() {
	fileType = ADF_NORMAL;
	/* High-density disk? */
	if (file.size() >= 160 * 22 * 512)
		numSectorsPerTrack = 22;
	else
		numSectorsPerTrack = 11;
	numTracks = file.size() / (512 * numSectorsPerTrack);

	if (numTracks > MaxTracks)
		qDebug("Your diskfile is too big!");
	for (uint i = 0; i < numTracks; i++) {
		AmigaDriveTrackId *tid = &trackData[i];
		tid->type = TRACK_AMIGADOS;
		tid->len = 512 * numSectorsPerTrack;
		tid->bitlen = 0;
		tid->offs = i * 512 * numSectorsPerTrack;
	}
}

bool AmigaDrive::insertDisk(const QString &fileName) {
	ejectDisk();
	file.setFileName(fileName);
	bool opened = false;
	if (file.exists()) { // required because rw mode creates new file
		if (file.open(QIODevice::ReadWrite))
			opened = true;
		else if (file.open(QIODevice::ReadOnly))
			opened = true;
	}
	if (!opened) {
		trackLen = FLOPPY_WRITE_LEN * 2 * 8;
		trackSpeed = NORMAL_FLOPPY_SPEED;
		return false;
	}
	QByteArray adfMagic = file.read(8);
	if (qstrncmp(adfMagic, "UAE-1ADF", 8) == 0)
		insertDiskHandleADF_EXT2();
	else if (qstrncmp(adfMagic, "UAE--ADF", 8) == 0)
		insertDiskHandleADF_EXT1();
	else
		insertDiskHandleADF_NORMAL();
	setTypeID();	/* Set DD or HD drive */
	bufferedSide = 2;	/* will force read */
	fillBigBuffer();
	return true;
}

inline void AmigaDrive::step() {
	if (stepLimit)
		return;
	/* A1200's floppy drive needs at least 30 raster lines between steps
	 * but we'll use very small value for better compatibility with faster CPU emulation
	 * (stupid trackloaders with CPU delay loops)
	 */
	stepLimit = 2;
	if (!isEmpty())
		diskChanged = false;
	if (diskDirection) {
		if (cyl)
			cyl--;
	} else {
		if (cyl < 83)
			cyl++;
		else
			qDebug("program tried to step over track 83");
	}
}

void AmigaDrive::setMotorEnabled(bool on) {
	/* A value of 5 works out to a guaranteed delay of 1/2 of a second
	   Higher values are dangerous, e.g. a value of 8 breaks the RSI demo.  */
	if (!isRunning() && on) {
		diskReadyTime = 5;
		idShifter = 0; /* Reset id shift reg counter */
	}
	motorOn = on;
	if (!on) {
		diskReady = false;
		diskReadyTime = 0;
	}
}

inline void AmigaDrive::readData(u8 *dst, int track, int offset, int len) {
	AmigaDriveTrackId *ti = &trackData[track];
	file.seek(ti->offs + offset);
	file.read((char *)dst, len);
}

static const u32 MfmMask = 0x55555555;

/* Megalomania does not like zero MFM words... */
static void mfmcode(u16 *mfm, int words) {
	u32 lastword = 0;

	while (words--) {
		u32 v = *mfm;
		u32 lv = (lastword << 16) | v;
		u32 nlv = 0x55555555 & ~lv;
		u32 mfmbits = (nlv << 1) & (nlv >> 1);

		*mfm++ = v | mfmbits;
		lastword = v;
	}
}

inline void AmigaDrive::fillBigBuffer() {
	uint trackIndex = cyl * 2 + diskSide;
	AmigaDriveTrackId *ti = &trackData[trackIndex];

	if (isEmpty() || trackIndex >= numTracks) {
		trackLen = FLOPPY_WRITE_LEN * ddhd * 2 * 8;
		trackSpeed = NORMAL_FLOPPY_SPEED;
		memset(bigMfmBuf, 0xAA, FLOPPY_WRITE_LEN * 2 * ddhd);
		return;
	}
	if (bufferedCyl == cyl && bufferedSide == diskSide)
		return;

	if (ti->type == TRACK_AMIGADOS) {
		/* Normal AmigaDOS format track */
		trackLen = (numSectorsPerTrack * 544 + FLOPPY_GAP_LEN) * 2 * 8;
		memset(bigMfmBuf, 0xAA, FLOPPY_GAP_LEN * 2);

		for (uint sec = 0; sec < numSectorsPerTrack; sec++) {
			u8 secbuf[544];
			u16 *mfmbuf = bigMfmBuf + 544 * sec + FLOPPY_GAP_LEN;
			u32 deven, dodd;
			u32 hck = 0, dck = 0;

			secbuf[0] = secbuf[1] = 0x00;
			secbuf[2] = secbuf[3] = 0xa1;
			secbuf[4] = 0xff;
			secbuf[5] = trackIndex;
			secbuf[6] = sec;
			secbuf[7] = numSectorsPerTrack - sec;

			for (int i = 8; i < 24; i++)
				secbuf[i] = 0;

			readData(&secbuf[32], trackIndex, sec * 512, 512);

			mfmbuf[0] = mfmbuf[1] = 0xaaaa;
			mfmbuf[2] = mfmbuf[3] = 0x4489;

			deven = ((secbuf[4] << 24) | (secbuf[5] << 16)
				 | (secbuf[6] << 8) | (secbuf[7]));
			dodd = deven >> 1;
			deven &= 0x55555555;
			dodd &= 0x55555555;

			mfmbuf[4] = dodd >> 16;
			mfmbuf[5] = dodd;
			mfmbuf[6] = deven >> 16;
			mfmbuf[7] = deven;

			for (int i = 8; i < 48; i++)
				mfmbuf[i] = 0xaaaa;
			for (int i = 0; i < 512; i += 4) {
				deven = ((secbuf[i + 32] << 24) | (secbuf[i + 33] << 16)
					 | (secbuf[i + 34] << 8) | (secbuf[i + 35]));
				dodd = deven >> 1;
				deven &= 0x55555555;
				dodd &= 0x55555555;
				mfmbuf[(i >> 1) + 32] = dodd >> 16;
				mfmbuf[(i >> 1) + 33] = dodd;
				mfmbuf[(i >> 1) + 256 + 32] = deven >> 16;
				mfmbuf[(i >> 1) + 256 + 33] = deven;
			}

			for (int i = 4; i < 24; i += 2)
				hck ^= (mfmbuf[i] << 16) | mfmbuf[i + 1];

			deven = dodd = hck;
			dodd >>= 1;
			mfmbuf[24] = dodd >> 16;
			mfmbuf[25] = dodd;
			mfmbuf[26] = deven >> 16;
			mfmbuf[27] = deven;

			for (int i = 32; i < 544; i += 2)
				dck ^= (mfmbuf[i] << 16) | mfmbuf[i + 1];

			deven = dodd = dck;
			dodd >>= 1;
			mfmbuf[28] = dodd >> 16;
			mfmbuf[29] = dodd;
			mfmbuf[30] = deven >> 16;
			mfmbuf[31] = deven;
			mfmcode (mfmbuf + 4, 544 - 4);
		}
	} else {
		int base_offset = ti->type == TRACK_RAW ? 0 : 1;
		trackLen = ti->bitlen + 16 * base_offset;
		bigMfmBuf[0] = ti->sync;
		readData((u8 *)(bigMfmBuf + base_offset), trackIndex, 0, (ti->bitlen+7)/8);
		for (int i = base_offset; i < (trackLen + 15) / 16; i++) {
			u16 *mfm = bigMfmBuf + i;
			u8 *data = (u8 *) mfm;
			*mfm = (data[0] << 8) | data[1];
		}
	}
	bufferedSide = diskSide;
	bufferedCyl = cyl;
	trackSpeed = NORMAL_FLOPPY_SPEED * trackLen / (2 * 8 * FLOPPY_WRITE_LEN);
}

/* Update ADF_EXT2 track header */
inline void AmigaDrive::updateFile(uint len, u8 type) {
	if (fileType != ADF_EXT2)
		return;

	int trackIndex = cyl * 2 + diskSide;
	AmigaDriveTrackId *ti = &trackData[trackIndex];
	ti->bitlen = len;
	ti->type = type;
	file.seek(12 + (3*4)*trackIndex);

	QByteArray trackHeader(3*4, '\0');
	trackHeader[3] = ti->type;
	amigaMemDoPutLong((u32 *)(trackHeader.data() + 4), ti->len);
	amigaMemDoPutLong((u32 *)(trackHeader.data() + 8), ti->bitlen);
	file.write(trackHeader);
	if (ti->len > (len + 7) / 8) {
		file.seek(ti->offs);
		file.write(QByteArray(ti->len, '\0'));
	}
}

static inline u32 getmfmlong(u16 *mbuf) {
	return ((*mbuf << 16) | *(mbuf + 1)) & MfmMask;
}

inline int AmigaDrive::writeDataAmigaDos() {
	int secwritten = 0;
	int fwlen = FLOPPY_WRITE_LEN * ddhd;
	int length = 2 * fwlen;
	int drvsec = numSectorsPerTrack;
	u32 odd, even, chksum, id, dlong;
	u8 *secdata;
	u8 secbuf[544];
	u16 *mbuf = bigMfmBuf;
	u16 *mend = mbuf + length;
	char sectable[22];
	memset (sectable, 0, sizeof (sectable));
	memcpy (mbuf + fwlen, mbuf, fwlen * sizeof (u16));
	mend -= (4 + 16 + 8 + 512);
	while (secwritten < drvsec) {
	int trackoffs;

	do {
		while (*mbuf++ != 0x4489) {
		if (mbuf >= mend)
			return 1;
		}
	} while (*mbuf++ != 0x4489);

	odd = getmfmlong (mbuf);
	even = getmfmlong (mbuf + 2);
	mbuf += 4;
	id = (odd << 1) | even;

	trackoffs = (id & 0xff00) >> 8;
	if (trackoffs + 1 > drvsec) {
		if (fileType == ADF_EXT2)
			return 2;
		qDebug("Disk write: weird sector number %d", trackoffs);
		continue;
	}
	chksum = odd ^ even;
	for (int i = 0; i < 4; i++) {
		odd = getmfmlong (mbuf);
		even = getmfmlong (mbuf + 8);
		mbuf += 2;

		dlong = (odd << 1) | even;
		if (dlong) {
		if (fileType == ADF_EXT2)
			return 6;
		secwritten = -200;
		}
		chksum ^= odd ^ even;
	}			/* could check here if the label is nonstandard */
	mbuf += 8;
	odd = getmfmlong (mbuf);
	even = getmfmlong (mbuf + 2);
	mbuf += 4;
	if (((odd << 1) | even) != chksum || ((id & 0x00ff0000) >> 16) != uint(cyl * 2 + diskSide)) {
		if (fileType == ADF_EXT2)
			return 3;
		qDebug("Disk write: checksum error on sector %d header", trackoffs);
		continue;
	}
	odd = getmfmlong (mbuf);
	even = getmfmlong (mbuf + 2);
	mbuf += 4;
	chksum = (odd << 1) | even;
	secdata = secbuf + 32;
	for (int i = 0; i < 128; i++) {
		odd = getmfmlong (mbuf);
		even = getmfmlong (mbuf + 256);
		mbuf += 2;
		dlong = (odd << 1) | even;
		*secdata++ = dlong >> 24;
		*secdata++ = dlong >> 16;
		*secdata++ = dlong >> 8;
		*secdata++ = dlong;
		chksum ^= odd ^ even;
	}
	mbuf += 256;
	if (chksum) {
		if (fileType == ADF_EXT2)
			return 4;
		qDebug("Disk write: sector %d, data checksum error", trackoffs);
		continue;
	}
	sectable[trackoffs] = 1;
	secwritten++;
	memcpy(writeBuffer + trackoffs * 512, secbuf + 32, 512);
	}
	if (fileType == ADF_EXT2 && (secwritten == 0 || secwritten < 0))
	return 5;
	if (secwritten == 0)
		qDebug("Disk write in unsupported format");
	if (secwritten < 0)
		qDebug("Disk write: sector labels ignored");

	updateFile(drvsec * 512 * 8, TRACK_AMIGADOS);
	file.seek(trackData[cyl * 2 + diskSide].offs);
	file.write((char *)writeBuffer, drvsec*512);
	return 0;
}

/* write raw track to disk file */
inline int AmigaDrive::writeDataExt2() {
	int track = cyl * 2 + diskSide;
	AmigaDriveTrackId *ti = &trackData[track];
	uint len = (trackLen + 7) / 8;
	if (len > ti->len) {
		qDebug("disk raw write: image file's track %d is too small (%d < %d)!", track, ti->len, len);
		return 0;
	}
	updateFile(trackLen, TRACK_RAW);
	for (uint i = 0; i < trackData[track].len / 2; i++) {
		u16 *mfm = bigMfmBuf + i;
		u8 *data = (u8 *) mfm;
		*mfm = 256 * *data + *(data + 1);
	}
	file.seek(ti->offs);
	file.write((char *)bigMfmBuf, len);
	return 1;
}

inline void AmigaDrive::writeData() {
	if (isWriteProtected())
		return;

	int ret;
	switch (fileType) {
	case ADF_NORMAL:
		writeDataAmigaDos();
		break;
	case ADF_EXT1:
		qDebug("writing to ADF_EXT1 not supported\n");
		return;
	case ADF_EXT2:
		ret = writeDataAmigaDos();
		if (ret)
			writeDataExt2();
		break;
	}
	bufferedSide = 2;	/* will force read */
}

void amigaDiskSelect(u8 data) {
	int lastselected = diskSelected;
	diskSelected = (data >> 3) & 0xF;
	diskSelected |= diskDisabledMask;
	diskSide = 1 - ((data >> 2) & 1);

	diskDirection = (data >> 1) & 1;
	int step_pulse = data & 1;
	if (step != step_pulse) {
		step = step_pulse;
		if (step) {
			for (int dr = 0; dr < amigaNumDrives; dr++) {
				if (!(diskSelected & (1 << dr)))
					amigaDrives[dr].step();
			}
		}
	}
	for (int dr = 0; dr < amigaNumDrives; dr++) {
	/* motor on/off workings tested with small assembler code on real Amiga 1200. */
	/* motor flipflop is set only when drive select goes from high to low */ 
		if (!(diskSelected & (1 << dr)) && (lastselected & (1 << dr)) ) {
			if (!amigaDrives[dr].isRunning()) {
				/* motor off: if motor bit = 0 in prevdata or data -> turn motor on */
				if ((diskSelectPrevdata & 0x80) == 0 || (data & 0x80) == 0)
					amigaDrives[dr].setMotorEnabled(true);
			} else {
				/* motor on: if motor bit = 1 in prevdata only (motor flag state in data has no effect)
				   -> turn motor off */
				if (diskSelectPrevdata & 0x80)
					amigaDrives[dr].setMotorEnabled(false);
			}
		}
	}
	diskSelectPrevdata = data;
}

u8 amigaDiskStatus() {
	u8 st = 0x3C;
	for (int dr = 0; dr < amigaNumDrives; dr++) {
		AmigaDrive *drv = &amigaDrives[dr];
		if (!(diskSelected & (1 << dr))) {
			if (drv->isRunning()) {
				if (drv->diskReady)
					st &= ~0x20;
			} else {
				/* report drive ID */
				if (drv->id & (1L << (31 - drv->idShifter)))
					st |= 0x20;
				else
					st &= ~0x20;
				/* Left shift id reg bit should be done with each LH transition of drv_sel */
				drv->idShifter = (drv->idShifter+1) & 31;
			}
			if (drv->isAtTrack0())
				st &= ~0x10;
			if (drv->isWriteProtected())
				st &= ~0x08;
			if (drv->diskChanged)
				st &= ~0x04;
		}
	}
	return st;
}

static void diskDmaFinished() {
	INTREQ (0x8002);
	dskdmaen = 0; /* ??? */
}

static void diskEvents(int last) {
	amigaEvents[AmigaEventDisk].active = 0;
	for (diskSyncCycle = last; diskSyncCycle < maxhpos; diskSyncCycle++) {
		if (diskSyncTab[diskSyncCycle]) {
			amigaEvents[AmigaEventDisk].time = amigaCycles + (diskSyncCycle - last) * CYCLE_UNIT;
			amigaEvents[AmigaEventDisk].active = 1;
			amigaEventsSchedule ();
			return;
		}
	}
}

void amigaDiskHandler() {
	amigaEvents[AmigaEventDisk].active = 0;
	if (diskSyncTab[diskSyncCycle] & DISK_WORDSYNC)
		INTREQ (0x9000);
	if (diskSyncTab[diskSyncCycle] & DISK_INDEXSYNC)
		amigaCiaDiskIndex();
	diskSyncTab[diskSyncCycle] = 0;
	diskEvents(diskSyncCycle);
}

/* get one bit from MFM bit stream */
static u32 getonebit (u16 * mfmbuf, int mfmpos, u32 word) {
	u16 *buf = &mfmbuf[mfmpos >> 4];
	word <<= 1;
	word |= (buf[0] & (1 << (15 - (mfmpos & 15)))) ? 1 : 0;
	return word;
}

#define WORDSYNC_CYCLES 7 /* (~7 * 280ns = 2us) */

/* disk DMA fetch happens on real Amiga at the beginning of next horizontal line
   (cycles 9, 11 and 13 according to hardware manual) We transfer all DMA'd
   data at cycle 0. I don't think any program cares about this small difference.

   We must handle dsklength = 0 because some copy protections use it to detect
   wordsync without transferring any data.
*/
static void doDmaFetch() {
	int i = 0;
	while (dmaTab[i] != 0xFFFFFFFF && dskdmaen == 2 && (dmacon & 0x210) == 0x210) {
		if (dsklength > 0) {
			amigaMemPutWord(dskpt, dmaTab[i++]);
			dskpt += 2;
			dsklength--;
		}
		if (dsklength == 0) {
			diskDmaFinished();
			break;
		}
	}
	dmaTab[0] = 0xFFFFFFFF;
}

static void amigaDiskStart() {
	for (int dr = 0; dr < amigaNumDrives; dr++) {
		AmigaDrive *drv = &amigaDrives[dr];
		if (!(diskSelected & (1 << dr))) {
			int tr = drv->cyl * 2 + diskSide;
			AmigaDriveTrackId *ti = &drv->trackData[tr];

			if (dskdmaen == 3) {
				drv->trackLen = FLOPPY_WRITE_LEN * drv->ddhd * 8 * 2;
				drv->trackSpeed = NORMAL_FLOPPY_SPEED;
			}
			/* Ugh.  A nasty hack.  Assume ADF_EXT1 tracks are always read
			   from the start.  */
			if (ti->type == TRACK_RAW1)
				drv->mfmPos = 0;
		}
	}
	dmaEnabled = !(adkcon & 0x400);
	word = 0;
	bitOffset = 0;
	dmaTab[0] = 0xFFFFFFFF;
}

void amigaDiskUpdateHsync() {
	for (int dr = 0; dr < amigaNumDrives; dr++) {
		AmigaDrive *drv = &amigaDrives[dr];
		if (drv->stepLimit)
			drv->stepLimit--;
	}
	if (lineCounter) {
		lineCounter--;
		if (!lineCounter)
			diskDmaFinished();
		return;
	}
	doDmaFetch();

	for (int dr = 0; dr < amigaNumDrives; dr++) {
		AmigaDrive *drv = &amigaDrives[dr];
		if (drv->isRunning()) {
			if (diskSelected & (1 << dr)) {
				drv->mfmPos += (maxhpos << 8) / drv->trackSpeed;
				drv->mfmPos %= drv->trackLen;
			} else {
				drv->fillBigBuffer();
				drv->mfmPos %= drv->trackLen;
				if (dskdmaen == 3)
					drv->updateHsyncWrite();
				else
					drv->updateHsyncRead();
				break;
			}
		}
	}
}

/* emulate disk write dma for full horizontal line */
inline void AmigaDrive::updateHsyncWrite() {
	int hpos = diskHpos;
	while (hpos < (maxhpos << 8)) {
		mfmPos = (mfmPos+1) % trackLen;
		if (!mfmPos) {
			diskSyncTab[hpos >> 8] |= DISK_INDEXSYNC;
			diskEvents(0);
		}
		if ((dmacon & 0x210) == 0x210 && dskdmaen == 3) {
			bitOffset = (bitOffset+1) % 15;
			if (!bitOffset) {
				bigMfmBuf[mfmPos >> 4] = amigaMemGetWord (dskpt);
				dskpt += 2;
				dsklength--;
				if (dsklength == 0) {
					diskDmaFinished();
					writeData();
				}
			}
		}
		hpos += trackSpeed;
	}
	diskHpos = hpos - (maxhpos << 8);
}

/* emulate disk read dma for full horizontal line */
inline void AmigaDrive::updateHsyncRead() {
	int hpos = diskHpos;
	bool isSync = false;
	int j = 0, k = 1, l = 0;

	dskbytrTab[0] = dskbytrTab[uhrDskbytrLast];

	if (uhrWordSyncLast >= 0 && maxhpos - wordsyncCycle[uhrWordSyncLast] < WORDSYNC_CYCLES)
		wordsyncCycle[l++] = (maxhpos - wordsyncCycle[uhrWordSyncLast]) - WORDSYNC_CYCLES;
	uhrWordSyncLast = -1;

	while (hpos < (maxhpos << 8)) {
		if (diskReady)
			word = getonebit(bigMfmBuf, mfmPos, word);
		else
			word <<= 1;
		mfmPos = (mfmPos+1) % trackLen;
		if (!mfmPos) {
			diskSyncTab[hpos >> 8] |= DISK_INDEXSYNC;
			isSync = true;
		}
		if (bitOffset == 31 && dmaEnabled) {
			dmaTab[j++] = (word >> 16) & 0xFFFF;
			if (j == MAX_DISK_WORDS_PER_LINE - 1) {
				qDebug("Bug: Disk DMA buffer overflow!");
				j--;
			}
		}
		if (bitOffset == 15 || bitOffset == 23 || bitOffset == 31) {
			dskbytrTab[k] = (word >> 8) & 0xff;
			dskbytrTab[k] |= 0x8000;
			uhrDskbytrLast = k;
			dskbytrCycle[k++] = hpos >> 8;
		}
		u16 synccheck = (word >> 8) & 0xFFFF;
		if (synccheck == dsksync) {
			if (adkcon & 0x400) {
				if (bitOffset != 23 || !dmaEnabled)
					bitOffset = 7;
				dmaEnabled = true;
			}
			uhrWordSyncLast = l;
			wordsyncCycle[l++] = hpos >> 8;
			diskSyncTab[hpos >> 8] |= DISK_WORDSYNC;
			isSync = true;
		}
		bitOffset++;
		if (bitOffset == 32) bitOffset = 16;
			hpos += trackSpeed;
	}
	dmaTab[j] = 0xFFFFFFFF;
	dskbytrCycle[k] = 255;
	wordsyncCycle[l] = 255;
	if (isSync)
		diskEvents(0);
	diskHpos = hpos - (maxhpos << 8);
}

void DSKLEN(u16 v) {
	if (v & 0x8000) {
		dskdmaen = ((dskdmaen == 1) ? 2 : 1);
	} else {
		/* Megalomania does this */
		if (dskdmaen == 3)
			qDebug("warning: Disk write DMA aborted, %d words left", dsklength);
		dskdmaen = 0;
	}
	dsklength = v & 0x3ffe;
	if (dskdmaen <= 1)
		return;
	if (v & 0x4000)
		dskdmaen = 3;
	/* Megalomania does not work if DSKLEN write is delayed to next horizontal line,
	   also it seems some copy protections require this fix */
	amigaDiskStart();
	/* Try to make floppy access from Kickstart faster.  */
	if (dskdmaen != 2)
		return;
	u32 pc = amigaCpuGetPC();
	if ((pc & 0xF80000) != 0xF80000)
		return;
	for (int dr = 0; dr < amigaNumDrives; dr++) {
		AmigaDrive *drv = &amigaDrives[dr];
		if (!drv->isRunning())
			continue;
		if ((diskSelected & (1 << dr)) == 0) {
			int pos = drv->mfmPos & ~15;
			drv->fillBigBuffer();
			if (adkcon & 0x400) {
				for (int i = 0; i < drv->trackLen; i += 16) {
					pos = (pos+16) % drv->trackLen;
					if (drv->bigMfmBuf[pos >> 4] == dsksync) {
						/* must skip first disk sync marker */
						pos = (pos+16) % drv->trackLen;
						break;
					}
				}
			}
			while (dsklength-- > 0) {
				amigaMemPutWord(dskpt, drv->bigMfmBuf[pos >> 4]);
				dskpt += 2;
				pos = (pos+16) % drv->trackLen;
			}
			INTREQ(0x9000);
			lineCounter = 2;
			dskdmaen = 0;
			break;
		}
	}
}

/* this is very unoptimized. DSKBYTR is used very rarely, so it should not matter. */

u16 DSKBYTR(int hpos) {
	int i = 0;
	while (hpos > dskbytrCycle[i + 1])
		i++;
	u16 v = dskbytrTab[i];
	dskbytrTab[i] &= ~0x8000;
	if (wordsyncCycle[0] != 255) {
		for (i = 0; hpos < wordsyncCycle[i]; i++)
			;
		if (hpos - wordsyncCycle[i] <= WORDSYNC_CYCLES)
			v |= 0x1000;
	}
	if (dskdmaen && (dmacon & 0x210) == 0x210)
		v |= 0x4000;
	if (dskdmaen == 3)
		v |= 0x2000;
	return v;
}

/* not a real hardware register */
u16 DSKDATR(int hpos) {
	Q_UNUSED(hpos)
	return 0;
}

void DSKSYNC(u16 v) {
	dsksync = v;
}

void DSKDAT(u16 v) {
	Q_UNUSED(v)
}

void DSKPTH(u16 v) {
	dskpt = (dskpt & 0xFFFF) | ((u32)v << 16);
}

void DSKPTL(u16 v) {
	dskpt = (dskpt & ~0xFFFF) | (v);
}

void amigaDiskReset() {
	diskHpos = 0;
	diskDisabledMask = 0;
	for (int i = amigaNumDrives; i < 4; i++)
		diskDisabledMask |= 1 << i;
	dmaTab[0] = 0xFFFFFFFF;
	dskbytrCycle[0] = 0;
	dskbytrCycle[1] = 255;
	wordsyncCycle[0] = 255;
	for (int i = 0 ; i < amigaNumDrives; i++)
		amigaDrives[i].reset();
}

void AmigaDrive::reset() {
	cyl = 0;
	mfmPos = 0;
	diskChanged = true;
	setTypeID();
}

void AmigaDrive::sl(int n) {
	emsl.begin(QString("drive%1").arg(n));
	QString fn;
	bool opened = true;
	if (emsl.save) {
		if (file.isOpen())
			fn = file.fileName();
		emsl.var("fileName", fn);
	} else {
		emsl.var("fileName", fn);
		opened = insertDisk(fn);
	}
	if (opened) {
		emsl.var("cyl", cyl);
		emsl.var("motorOn", motorOn);
		emsl.var("mfmPos", mfmPos);
		emsl.var("diskChanged", diskChanged);
		emsl.var("diskReady", diskReady);
		emsl.var("diskReadyTime", diskReadyTime);
		emsl.var("stepLimit", stepLimit);
		emsl.var("idShifter", idShifter);
	}
	emsl.end();
}

void amigaDiskSl() {
	emsl.begin("disk");
	emsl.array("bytrTab", dskbytrTab, sizeof(dskbytrTab));
	emsl.array("bytrCycle", dskbytrCycle, sizeof(dskbytrCycle));
	emsl.array("wordsyncCycle", wordsyncCycle, sizeof(wordsyncCycle));
	emsl.array("dmaTab", dmaTab, sizeof(dmaTab));
	emsl.array("syncTab", diskSyncTab, sizeof(diskSyncTab));
	emsl.var("syncCycle", diskSyncCycle);
	emsl.var("dmaen", dskdmaen);
	emsl.var("length", dsklength);
	emsl.var("dmaen", dskdmaen);
	emsl.var("sync", dsksync);
	emsl.var("pt", dskpt);
	emsl.var("dmaEnabled", dmaEnabled);
	emsl.var("bitOffset", bitOffset);
	emsl.var("word", word);
	emsl.var("hpos", diskHpos);
	emsl.var("lineCounter", lineCounter);
	emsl.var("side", diskSide);
	emsl.var("direction", diskDirection);
	emsl.var("selected", diskSelected);
	emsl.var("selectPrevData", diskSelectPrevdata);
	emsl.array("writeBuffer", writeBuffer, sizeof(writeBuffer));
	emsl.var("step", step);
	emsl.var("uhrDskbytrLast", uhrDskbytrLast);
	emsl.var("uhrWordSyncLast", uhrWordSyncLast);

	// TODO hard conf
	int numDrives = amigaNumDrives;
	emsl.var("numDrives", numDrives);
	if (numDrives != amigaNumDrives) {
		emsl.error = "Configuration dismatch - different num of drives.";
		return;
	}
	emsl.end();

	for (int i = 0; i < amigaNumDrives; i++)
		amigaDrives[i].sl(i);
}
