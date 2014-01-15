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

#ifndef HOSTINPUT_H
#define HOSTINPUT_H

class Emu;
class HostInputDevice;
class TouchInputDevice;
class KeybInputDevice;
#include "base_global.h"
#include <QObject>
class QPainter;

class BASE_EXPORT HostInput : public QObject
{
	Q_OBJECT
public:
	explicit HostInput(Emu *emu);
	~HostInput();

	void setPadOpacity(qreal opacity);
	qreal padOpacity() const;

	TouchInputDevice *touchInputDevice() const;
	KeybInputDevice *keybInputDevice() const;

	QList<HostInputDevice *> devices() const;

	void sync();

	void paint(QPainter *painter);
	void processTouch(QEvent *e);
public slots:
	void loadFromConf();
signals:
	void pause();
	void quit();
	void devicesChanged();
protected:
	bool eventFilter(QObject *o, QEvent *e);
private slots:
private:
	void setupTouchDevice();

	Emu *m_emu;
	QList<HostInputDevice *> m_devices;
	int m_numSixAxes;
	qreal m_padOpacity;
};

inline qreal HostInput::padOpacity() const
{ return m_padOpacity; }
inline QList<HostInputDevice *> HostInput::devices() const
{ return m_devices; }

#endif // HOSTINPUT_H
