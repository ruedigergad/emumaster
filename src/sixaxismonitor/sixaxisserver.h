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

#ifndef SIXAXISSERVER_H
#define SIXAXISSERVER_H

#include <QObject>
#include <QSocketNotifier>
#include <QLocalServer>
#include <QLocalSocket>
#include <bluetooth/bluetooth.h>
class SixAxisDevice;

class SixAxisServer : public QObject
{
	Q_OBJECT
public:
	explicit SixAxisServer(QObject *parent = 0);
	~SixAxisServer();
	QString open();
	void close();
	void streamPacket(SixAxisDevice *dev, int len);
	int numDevices() const;
	SixAxisDevice *device(int i) const;
	SixAxisDevice *deviceByAddr(const bdaddr_t *addr) const;
	void disconnectDevice(SixAxisDevice *dev);
private slots:
	void acceptNewDevice();
	void dataFromSocket();
	void newClient();
	void clientDisconnected();
signals:
	void countChanged();
private:
	int l2Listen(int psm);

	int m_ctrl;
	int m_data;
	QList<SixAxisDevice *> m_devices;
	QList<int> m_disconnected;

	QLocalServer *m_localServer;
	QList<QLocalSocket *> m_localClients;
};

#endif // SIXAXISSERVER_H
