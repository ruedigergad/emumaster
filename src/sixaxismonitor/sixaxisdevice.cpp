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

#include "sixaxisdevice.h"
#include "sixaxisserver.h"
#include <QSocketNotifier>
#include <sys/socket.h>

/** Converts bdaddr \a a to QString and returns as a result. */
static QString bdaddrToString(const bdaddr_t *a)
{
	return QString().sprintf("%02x:%02x:%02x:%02x:%02x:%02x",
							a->b[5], a->b[4], a->b[3],
							a->b[2], a->b[1], a->b[0]);
}

/**
	\class SixAxisDevice
	The SixAxisDevice is where the actual data transfer with sixaxis occurs.
	It understands the protocol of PS3 controller.
 */

/** Creates SixAxisDevice. */
SixAxisDevice::SixAxisDevice(int ctrl, int data, bdaddr_t *addr, QObject *parent) :
	QObject(parent),
	m_ctrl(ctrl),
	m_data(data),
	m_leds(0)
{
	bacpy(&m_addr, addr);

	QSocketNotifier *dataNotifier = new QSocketNotifier(m_data, QSocketNotifier::Read, this);
	QObject::connect(dataNotifier, SIGNAL(activated(int)), SLOT(receiveData()));
	QSocketNotifier *errNotifier = new QSocketNotifier(m_data, QSocketNotifier::Exception, this);
	QObject::connect(errNotifier, SIGNAL(activated(int)), SLOT(onDisconnected()));

	enableReporting();
}

/** Destroys SixAxisDevice. */
SixAxisDevice::~SixAxisDevice()
{
	shutdown(m_ctrl, SHUT_RDWR);
	shutdown(m_data, SHUT_RDWR);
	close(m_ctrl);
	close(m_data);
	qDebug("SixAxis %s disconnected", qPrintable(bdaddrToString(&m_addr)));
}

/** Enables sixaxis reporting. */
void SixAxisDevice::enableReporting()
{
	qDebug("SixAxis %s connected", qPrintable(bdaddrToString(&m_addr)));

	uchar enablePacket[] = {
		0x53,							  /* HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_FEATURE */
		0xf4, 0x42, 0x03, 0x00, 0x00
	};
	send(m_ctrl, enablePacket, sizeof(enablePacket), 0);
	recv(m_ctrl, m_buf, sizeof(m_buf), 0);
}

/** Sets LEDs at the back of the controller. */
void SixAxisDevice::setLeds(int leds)
{
	uchar setLedsPacket[] = {
		0x52,							  /* HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_OUTPUT */
		0x01,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x1e,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};
	m_leds = leds & 0xf;
	setLedsPacket[11] = m_leds << 1;
	send(m_ctrl, setLedsPacket, sizeof(setLedsPacket), 0);
	recv(m_ctrl, m_buf, sizeof(m_buf), 0);
}

/** Sets rumble of the controller. */
void SixAxisDevice::setRumble(int weak, int strong)
{
	uchar setRumblePacket[] = {
		0x52,								/* HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_OUTPUT */
		0x01,
		0x00, 0x00, 0x00, 0x00, 0x00		// rumble values
	};
	if (weak || strong) {
		setRumblePacket[3] = setRumblePacket[5] = 254; // stay on for a long while
		setRumblePacket[4] = weak ? 255 : 0;
		setRumblePacket[6] = qMin(0x40 + strong, 0xff);;
	} else {
		setRumblePacket[3] = setRumblePacket[5] = 0; // off
		setRumblePacket[4] = setRumblePacket[6] = 0; // minimum rumble
	}
	send(m_ctrl, setRumblePacket, sizeof(setRumblePacket), 0);
	recv(m_ctrl, m_buf, sizeof(m_buf), 0);
}

/** Receives a packet from the sixaxis. */
void SixAxisDevice::receiveData()
{
	int len = recv(m_data, m_buf, 1024, MSG_DONTWAIT);
	if (len > 0) {
		SixAxisServer *srv = static_cast<SixAxisServer *>(parent());
		srv->streamPacket(this, len);
	}
}

/** Handles disconnection of the sixaxis. */
void SixAxisDevice::onDisconnected()
{
	SixAxisServer *srv = static_cast<SixAxisServer *>(parent());
	srv->disconnectDevice(this);
}

/** Returns BT address of the sixaxis. */
QString SixAxisDevice::addressString() const
{
	return bdaddrToString(&m_addr);
}
