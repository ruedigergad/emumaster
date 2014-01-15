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

#ifndef PSXSIO_H
#define PSXSIO_H

#include "common.h"

#ifdef __cplusplus

#include <QFile>
#include <QList>
#include <QImage>

extern void psxSioSl();

class PsxMcdBlockInfo {
public:
	QString title;
	QString id;
	QString name;
	u8 flags;
	QList<QImage> icons;
};

class PsxMcd {
public:
	static const int BlockSize = 8192;
	static const int NumBlocks = 16;
	static const int Size = BlockSize * NumBlocks;

	PsxMcd();
	void init(const QString &path);
	void write(u32 address, int size);
	bool isEnabled() { return m_enabled; }

	PsxMcdBlockInfo blockInfo(int block);

	u8 memory[Size];
private:
	QFile m_file;
	bool m_enabled;
};

extern PsxMcd psxMcd1;
extern PsxMcd psxMcd2;

extern "C" {
#endif

void sioWrite8(u8 data);
void sioWriteStat16(u16 data);
void sioWriteMode16(u16 data);
void sioWriteCtrl16(u16 data);
void sioWriteBaud16(u16 data);

u8 sioRead8();
u16 sioReadStat16();
u16 sioReadMode16();
u16 sioReadCtrl16();
u16 sioReadBaud16();

void sioInterrupt();

#ifdef __cplusplus
}
#endif

#endif // PSXSIO_H
