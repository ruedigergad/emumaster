/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2002 by Stéphane Dallongeville
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"
#include "cd_file.h"
#include "mp3player.h"
#include <QDir>
#include <QFileInfo>

bool PicoMcdToc::open(const QString &fileName, QString *error)
{
	close();

	QFile *dataFile = new QFile(fileName);
	if (!dataFile->open(QIODevice::ReadOnly)) {
		delete dataFile;
		*error = QObject::tr("Could not open CD file (%1)").arg(fileName);
		return false;
	}

	if (!detectDataTypeAndRegion(dataFile)) {
		delete dataFile;
		*error = QObject::tr("Invalid header of CD file").arg(fileName);
		return false;
	}

	Last_Track = 1;
	Tracks[0].file = dataFile;
	// size in sectors
	if (Tracks[0].type == PicoMcdTrack::Iso)
		Tracks[0].Length = dataFile->size() >> 11;
	else
		Tracks[0].Length = dataFile->size() / 2352;

	searchForMp3Files();

	// fill msf structures
	Tracks[0].MSF.M = 0;
	Tracks[0].MSF.S = 2;
	Tracks[0].MSF.F = 0;

	int curLba = Tracks[0].Length;
	for (int i = 1; i < Last_Track; i++) {
		LBA_to_MSF(curLba, &Tracks[i].MSF);
		curLba += Tracks[i].Length;
	}
	LBA_to_MSF(curLba, &Tracks[Last_Track].MSF);
	return true;
}

void PicoMcdToc::close()
{
	for (int i = 0; i < 100; i++)
		delete Tracks[i].file;
	memset(Tracks, 0, sizeof(Tracks));
}

bool PicoMcdToc::detectDataTypeAndRegion(QFile *file)
{
	char buf[0x20];

	file->seek(0);
	if (file->read(buf, 32) != 32)
		return false;

	PicoMcdTrack::Type type = PicoMcdTrack::None;
	if (!strncasecmp("SEGADISCSYSTEM", buf+0x00, 14))
		type = PicoMcdTrack::Iso;
	if (!strncasecmp("SEGADISCSYSTEM", buf+0x10, 14))
		type = PicoMcdTrack::Bin;

	if (type == PicoMcdTrack::None)
		return false;

	Tracks[0].type = type;

	m_region = PicoRegionUsa;

	int pos = 0x100 + 0x10B;
	if (type == PicoMcdTrack::Bin)
		pos += 0x10;
	file->seek(pos);
	file->getChar(buf);

	if (buf[0] == 0x64)
		m_region = PicoRegionEurope;
	if (buf[0] == 0xA1)
		m_region = PicoRegionJapanNtsc;

	return true;
}

void PicoMcdToc::searchForMp3Files()
{
	// list mp3 files
	QString dataFileName = Tracks[0].file->fileName();
	QFileInfo diskInfo(dataFileName);
	QDir dir = diskInfo.dir();
	QStringList list = dir.entryList(QStringList() << "*.mp3");

	QString start = diskInfo.completeBaseName();
	foreach (QString name, list) {
		if (name.startsWith(start)) {
			// extract track index
			QString dd = name.mid(name.size()-6, 2);
			if (!dd.at(0).isDigit())
				dd = dd.right(1);

			bool ok;
			// convert to int, filenames starts with 02 - decrement then
			int index = dd.toInt(&ok) - 1;
			if (ok)
				insertMp3File(index, dir.filePath(name));
		}
	}
}

void PicoMcdToc::insertMp3File(int index, const QString &fileName)
{
	if (Tracks[index].file == 0) {
		Tracks[index].type = PicoMcdTrack::Mp3;
		QFile *file = new QFile(fileName);
		if (file->open(QIODevice::ReadOnly)) {
			Tracks[index].file = file;

			int fs = file->size();
			file->close();
			fs *= 75;
			fs /= 30000;
			Tracks[index].Length = fs;
			Last_Track = qMax(index+1, Last_Track);
		} else {
			delete file;
		}
	} else {
		qDebug("MP3 file already inserted, track:%d, file: %s", index, qPrintable(fileName));
	}
}

// TODO move to cdc and toc
void FILE_Read_One_LBA_CDC() {
	// Update CDC stuff
	Pico_mcd->cdc.updateHeader();

	if (Pico_mcd->s68k_regs[0x36] & 1) { // DATA track
		if (Pico_mcd->cdc.CTRL.B.B0 & 0x80) { // DECEN = decoding enable
			if (Pico_mcd->cdc.CTRL.B.B0 & 0x04)	{ // WRRQ : this bit enable write to buffer
				// CAUTION : lookahead bit not implemented
				int lba = qBound(0, Pico_mcd->scd.Cur_LBA, Pico_mcd->TOC.Tracks[0].Length-1);
				Pico_mcd->scd.Cur_LBA++;

				Pico_mcd->cdc.WA.N = (Pico_mcd->cdc.WA.N + 2352) & 0x7FFF;		// add one sector to WA
				Pico_mcd->cdc.PT.N = (Pico_mcd->cdc.PT.N + 2352) & 0x7FFF;

				*(u32 *)(Pico_mcd->cdc.Buffer + Pico_mcd->cdc.PT.N) = Pico_mcd->cdc.HEAD.N;

				u8 *dst = Pico_mcd->cdc.Buffer + Pico_mcd->cdc.PT.N + 4;
				QFile *file = Pico_mcd->TOC.Tracks[0].file;
				if (Pico_mcd->TOC.Tracks[0].type == PicoMcdTrack::Bin)
					file->seek(lba * 2352 + 16);
				else
					file->seek(lba << 11);
				file->read((char *)dst, 2048);
			}
		}
	} else { // music track
		Pico_mcd->scd.Cur_LBA++;

		Pico_mcd->cdc.WA.N = (Pico_mcd->cdc.WA.N + 2352) & 0x7FFF;		// add one sector to WA
		Pico_mcd->cdc.PT.N = (Pico_mcd->cdc.PT.N + 2352) & 0x7FFF;

		if (Pico_mcd->cdc.CTRL.B.B0 & 0x80)		// DECEN = decoding enable
		{
			if (Pico_mcd->cdc.CTRL.B.B0 & 0x04)	// WRRQ : this bit enable write to buffer
			{
				// CAUTION : lookahead bit not implemented
			}
		}
	}

	if (Pico_mcd->cdc.CTRL.B.B0 & 0x80) { // DECEN = decoding enable
		Pico_mcd->cdc.STAT.B.B0 = 0x80;

		if (Pico_mcd->cdc.CTRL.B.B0 & 0x10)	// determine form bit form sub header ?
			Pico_mcd->cdc.STAT.B.B2 = Pico_mcd->cdc.CTRL.B.B1 & 0x08;
		else
			Pico_mcd->cdc.STAT.B.B2 = Pico_mcd->cdc.CTRL.B.B1 & 0x0C;

		if (Pico_mcd->cdc.CTRL.B.B0 & 0x02)
			Pico_mcd->cdc.STAT.B.B3 = 0x20;	// ECC done
		else
			Pico_mcd->cdc.STAT.B.B3 = 0x00;	// ECC not done

		if (Pico_mcd->cdc.IFCTRL & 0x20) {
			if (Pico_mcd->s68k_regs[0x33] & (1<<5))
				SekInterruptS68k(5);
			Pico_mcd->cdc.IFSTAT &= ~0x20;		// DEC interrupt happen
			Pico_mcd->cdc.Decode_Reg_Read = 0;	// Reset read after DEC int
		}
	}
}

bool PicoMcdToc::playAudio()
{
	if (!picoEmu.mp3Player())
		return false;
	picoEmu.mp3Player()->stop();

	Q_ASSERT(Pico_mcd->scd.Cur_Track >= 1);
	int index = Pico_mcd->scd.Cur_Track - 1;
	Pico_mcd->m.audio_track = index;

	picoDebugMcdToc("play track #%i", Pico_mcd->scd.Cur_Track);

	if (index >= 100 || !Tracks[index].file)
		return false;
	if (Tracks[index].type != PicoMcdTrack::Mp3)
		return false;

	int pos1024 = 0;
	int trackLbaPos = Pico_mcd->scd.Cur_LBA - Track_to_LBA(Pico_mcd->scd.Cur_Track);
	if (trackLbaPos < 0)
		trackLbaPos = 0;
	pos1024 = trackLbaPos * 1024 / Tracks[index].Length;
	playTrack(index, pos1024);
	return true;
}

void PicoMcdToc::playTrack(int index, int offset)
{
	Mp3Player *player = picoEmu.mp3Player();
	if (!player)
		return;
	if (!Tracks[index].file)
		return;

	QString fileName = Tracks[index].file->fileName();
	QMetaObject::invokeMethod(player,
							  "start",
							  Qt::QueuedConnection,
							  Q_ARG(QString, fileName),
							  Q_ARG(bool, picoEmu.isRunning()),
							  Q_ARG(int, offset));
}

void PicoMcdToc::stopAudio()
{
	Pico_mcd->m.audio_track = 0;
	Mp3Player *player = picoEmu.mp3Player();
	if (player) {
		QMetaObject::invokeMethod(player,
								  "stop",
								  Qt::QueuedConnection);
	}
}

int PicoMcdToc::region() const
{
	if (PicoRegionOverride)
		return PicoRegionOverride;
	else
		return m_region;
}
