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

#include "sixaxisserver.h"
#include "sixaxisdevice.h"
#include <bluetooth/l2cap.h>

#define BTCTRL 17
#define BTDATA 19

/**
	\class SixAxisServer
	Establishes connection with PS3 controllers and shares them
	with other processes through local sockets.
 */

/** Creates SixAxisServer. */
SixAxisServer::SixAxisServer(QObject *parent) :
	QObject(parent)
{
	m_ctrl = -1;
	m_data = -1;
	m_localServer = new QLocalServer(this);
	QObject::connect(m_localServer, SIGNAL(newConnection()), SLOT(newClient()));
}

/** Destroys SixAxisServer. */
SixAxisServer::~SixAxisServer()
{
	close();
	m_localServer->close();
}

/**
	Opens bluetooth device and starts listening for new PS3 controllers.
	Returns nonempty string on error.
 */
QString SixAxisServer::open()
{
	m_ctrl = l2Listen(BTCTRL);
	if (m_ctrl < 0)
		return tr("Can't bind to psm %1").arg(BTCTRL);
	m_data = l2Listen(BTDATA);
	if (m_data < 0)
		return tr("Can't bind to psm %1").arg(BTDATA);
	if (!m_localServer->listen("emumaster.sixaxis"))
		return tr("Unable to initialize local server");
	QSocketNotifier *ctrlNotify = new QSocketNotifier(m_ctrl, QSocketNotifier::Read, this);
	QObject::connect(ctrlNotify, SIGNAL(activated(int)), SLOT(acceptNewDevice()));
	return QString();
}

/** Stops listening and closes bluetooth device. */
void SixAxisServer::close()
{
	::close(m_ctrl);
	::close(m_data);
}

int SixAxisServer::l2Listen(int psm)
{
	int fd;
	struct sockaddr_l2 addr;

	if ((fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1)
		return -1;
	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_bdaddr = *BDADDR_ANY;
	addr.l2_psm = htobs(psm);
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		return -1;
	if (listen(fd, 5) == -1)
		return -1;
	return fd;
}

/**
	Tries to establish connection with newly detected PS3 controller.
	If succeeds informs clients about the new controller.
 */
void SixAxisServer::acceptNewDevice()
{
	struct sockaddr_l2 addrCtrl;
	struct sockaddr_l2 addrData;
	socklen_t len = sizeof(addrCtrl);
	int saCtrl = accept(m_ctrl, (struct sockaddr *)&addrCtrl, &len);
	if (saCtrl < 0) {
		qDebug("Unable to accept ctrl stream");
		return;
	}
	int saData = accept(m_data, (struct sockaddr *)&addrData, &len);
	if (saData < 0) {
		qDebug("Unable to accept data stream");
		return;
	}
	if (bacmp(&addrCtrl.l2_bdaddr, &addrData.l2_bdaddr) != 0) {
		qDebug("Ctrl and data streams from different devices");
		::close(saCtrl);
		::close(saData);
		return;
	}
	SixAxisDevice *dev = new SixAxisDevice(saCtrl, saData, &addrCtrl.l2_bdaddr, this);
	m_devices.append(dev);

	int leds = 1 << (m_devices.size()-1);
	if (m_devices.size() >= 3)
		leds = 0xF;
	dev->setLeds(leds);

	for (int i = 0; i < m_localClients.size(); i++) {
		QIODevice *io = m_localClients.at(i);
		io->write("n");
		io->write((const char *)dev->address(), sizeof(bdaddr_t));
	}
	emit countChanged();
}

/** Disconnects the controller. Informs clients about disconnection. */
void SixAxisServer::disconnectDevice(SixAxisDevice *dev)
{
	if (!m_devices.removeOne(dev))
		return;
	for (int i = 0; i < m_localClients.size(); i++) {
		QIODevice *io = m_localClients.at(i);
		io->write("d");
		io->write((const char *)dev->address(), sizeof(bdaddr_t));
	}
	delete dev;
	emit countChanged();
}

/** Streams the data received from a sixaxis to the clients. */
void SixAxisServer::streamPacket(SixAxisDevice *dev, int len)
{
	for (int i = 0; i < m_localClients.size(); i++) {
		QLocalSocket *io = m_localClients.at(i);
		io->write("p");
		io->write((const char *)dev->address(), sizeof(bdaddr_t));
		io->write((const char *)&len, sizeof(int));
		io->write(dev->buffer(), len);
		io->flush();
	}
}

/**
	Perform action issued by one of the clients.
	l - set leds   000L L-leds
	r - set rumble SSWW S-strong W-weak
*/
void SixAxisServer::dataFromSocket()
{
	QLocalSocket *io = static_cast<QLocalSocket *>(sender());
	while (io->bytesAvailable() >= 1+sizeof(bdaddr_t)+sizeof(int)) {
		char c;
		bdaddr_t addr;
		io->read(&c, 1);
		int param;
		io->read((char *)&addr, sizeof(bdaddr_t));
		io->read((char *)&param, sizeof(int));

		SixAxisDevice *dev = deviceByAddr(&addr);
		if (dev) {
			if (c == 'l') {
				dev->setLeds(param);
			} else if (c == 'r') {
				dev->setRumble((param >> 0) & 0xFF,
							   (param >> 8) & 0xFF);
			}
		}
	}
}

/** Finds and returns the controller with specified BT address \a addr. */
SixAxisDevice *SixAxisServer::deviceByAddr(const bdaddr_t *addr) const
{
	for (int i = 0; i < m_devices.size(); i++) {
		SixAxisDevice *dev = m_devices.at(i);
		if (bacmp(dev->address(), addr) == 0)
			return dev;
	}
	return 0;
}

/** Returns number of connected controllers. */
int SixAxisServer::numDevices() const
{
	return m_devices.size();
}

/** Returns a device of \a i index. */
SixAxisDevice *SixAxisServer::device(int i) const
{
	return m_devices.at(i);
}

/** Accepts new client. */
void SixAxisServer::newClient()
{
	while (m_localServer->hasPendingConnections()) {
		QLocalSocket *io = m_localServer->nextPendingConnection();
		QObject::connect(io, SIGNAL(disconnected()), SLOT(clientDisconnected()));
		m_localClients.append(io);

		for (int i = 0; i < m_devices.size(); i++) {
			SixAxisDevice *dev = m_devices.at(i);
			io->write("n");
			io->write((const char *)dev->address(), sizeof(bdaddr_t));
		}
	}
}

/** Called when client disconnected from the server. */
void SixAxisServer::clientDisconnected()
{
	QLocalSocket *io = static_cast<QLocalSocket *>(sender());
	if (m_localClients.removeOne(io))
		io->deleteLater();
}
