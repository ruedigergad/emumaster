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

#ifndef PSXEMU_H
#define PSXEMU_H

#include <base/emu.h>
#include <QThread>
#include <QSemaphore>

class PsxEmu : public Emu {
	Q_OBJECT
public:	
	enum SystemType {
		NtscType = 0,
		PalType
	};
	enum CpuType {
		CpuDynarec = 0,
		CpuInterpreter
	};
	enum GpuType {
		GpuUnai = 0
	};
	enum SpuType {
		SpuNull = 0
	};

	explicit PsxEmu(QObject *parent = 0);
	bool init(const QString &diskPath, QString *error);
	void shutdown();
	void reset();
	void updateGpuScale(int w, int h);
	void flipScreen();

	QString setDisk(const QString &path);
	void emulateFrame(bool drawEnabled);
	const QImage &frame() const;
	int fillAudioBuffer(char *stream, int streamSize);
	void setPadKeys(int pad, int keys);

	void sl();

	int systemType;
protected:
	void resume();
private:
	QSemaphore m_consSem;
	QSemaphore m_prodSem;
};

class PsxThread : public QThread {
	Q_OBJECT
public:
protected:
	void run();
};

extern PsxEmu psxEmu;

#endif // PSXEMU_H
