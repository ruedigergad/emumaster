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

#ifndef SIXAXIS_H
#define SIXAXIS_H

#include <QObject>
#include <bluetooth/bluetooth.h>
class QLocalSocket;

class SixAxis : public QObject {
	Q_OBJECT
public:
	enum Button {
		Select,
		L3,
		R3,
		Start,
		DpadUp,
		DpadRight,
		DpadDown,
		DpadLeft,
		L2,
		R2,
		L1,
		R1,
		Triangle,
		Circle,
		Cross,
		Square,
		PS
	};
	enum Axis {
		LX,
		LY,
		RX,
		RY,
		Up,
		Right,
		Down,
		Left,
		L2P,
		R2P,
		L1P,
		R1P,
		TriangleP,
		CircleP,
		CrossP,
		SquareP,
		AccX,
		AccY,
		AccZ,
		AxesCount
	};
	int buttons() const;
	bool button(int i) const;
	int axis(int i) const;
	void setLeds(int leds);
	void setRumble(int weak, int strong);
signals:
	void updated();
	void disconnected();
private:
	explicit SixAxis(bdaddr_t *addr, QObject *parent = 0);
	void processData();
	void emitDisconnected();

	bdaddr_t m_addr;
	int m_buttons;
	int m_axes[AxesCount];
	char m_data[64];

	friend class SixAxisDaemon;
};

inline int SixAxis::buttons() const
{ return m_buttons; }
inline bool SixAxis::button(int i) const
{ return m_buttons & (1 << i); }
inline int SixAxis::axis(int i) const
{ return m_axes[i]; }

class SixAxisDaemon : public QObject {
	Q_OBJECT
public:
	static SixAxisDaemon *instance();

	void start();
	void stop();
	bool hasNewPad() const;
	SixAxis *nextNewPad();
signals:
	void newPad();
private slots:
	void processData();
	void padDestroyed(QObject *o);
private:
	SixAxisDaemon();
	SixAxis *padByAddr(bdaddr_t *addr);

	QLocalSocket *m_io;
	QList<SixAxis *> m_newPads;
	QList<SixAxis *> m_pads;

	static SixAxisDaemon *m_instance;

	friend class SixAxis;
};

#endif // SIXAXIS_H
