/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SIXAXISINPUTDEVICE_H
#define SIXAXISINPUTDEVICE_H

class SixAxis;
#include "hostinputdevice.h"

class BASE_EXPORT SixAxisInputDevice : public HostInputDevice
{
	Q_OBJECT
public:
	explicit SixAxisInputDevice(SixAxis *sixAxis, QObject *parent = 0);
	void sync(EmuInput *emuInput);
signals:
	void pause();
private slots:
	void onSixAxisUpdated();
	void enEmuFunctionChanged();
private:
	void setupEmuFunctionList();
	void convertPad();
	void convertMouse();

	SixAxis *m_sixAxis;
	int m_buttons;
	int m_mouseX;
	int m_mouseY;
	int m_mouseButtons;
	bool m_converted;

	static const int m_buttonsMapping[];
};

#endif // SIXAXISINPUTDEVICE_H
