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

#ifndef SIXAXISDEVICE_H
#define SIXAXISDEVICE_H

#include <QObject>
#include <bluetooth/bluetooth.h>

class SixAxisDevice : public QObject
{
	Q_OBJECT
public:
	explicit SixAxisDevice(int ctrl, int data, bdaddr_t *addr, QObject *parent = 0);
	~SixAxisDevice();
	void setLeds(int leds);
	void setRumble(int weak, int strong);
	QString addressString() const;
	const bdaddr_t *address() const;
	char *buffer();
	int leds() const;
private slots:
	void receiveData();
	void onDisconnected();
private:
	void enableReporting();

	int m_ctrl;
	int m_data;
	bdaddr_t m_addr;
	char m_buf[1024];
	int m_leds;
};

inline const bdaddr_t *SixAxisDevice::address() const
{ return &m_addr; }
inline char *SixAxisDevice::buffer()
{ return m_buf; }
inline int SixAxisDevice::leds() const
{ return m_leds; }

#endif // SIXAXISDEVICE_H
