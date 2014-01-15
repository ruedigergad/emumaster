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

#ifndef AMIGAEMU_H
#define AMIGAEMU_H

#include <base/emu.h>
#include <QThread>

#define _GCCRES_ __restrict__

class AmigaEmu : public Emu {
    Q_OBJECT
public:
	explicit AmigaEmu(QObject *parent = 0);
	bool init(const QString &diskPath, QString *error);
	void shutdown();
	void reset();
	void emulateFrame(bool drawEnabled);
	const QImage &frame() const;
	int fillAudioBuffer(char *stream, int streamSize);
	void vSync();

	void setJoy(int joy, int buttons);
	void setMouse(int mouse, int buttons, int dx, int dy);
	void updateInput();

	void sl();
private:
	bool m_inputPortToggle[2];
};

extern AmigaEmu amigaEmu;

class AmigaThread : public QThread {
	Q_OBJECT
public:
protected:
	void run();
};

#endif // AMIGAEMU_H
