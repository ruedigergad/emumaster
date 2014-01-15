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

#include "sixaxis.h"
#include <QLocalSocket>

SixAxisDaemon *SixAxisDaemon::m_instance = 0;

SixAxis::SixAxis(bdaddr_t *addr, QObject *parent) :
	QObject(parent) {
	bacpy(&m_addr, addr);
	m_buttons = 0;
	memset(m_axes, 0, sizeof(m_axes));
}

void SixAxis::processData() {
	m_buttons = m_data[3] | (m_data[4] << 8) | (m_data[5] << 16);

	m_axes[0] = m_data[7] - 128;
	m_axes[1] = m_data[8] - 128;
	m_axes[2] = m_data[9] - 128;
	m_axes[3] = m_data[10] - 128;
	m_axes[4] = m_data[15];
	m_axes[5] = m_data[16];
	m_axes[6] = m_data[17];
	m_axes[7] = m_data[18];
	m_axes[8] = m_data[19];
	m_axes[9] = m_data[20];
	m_axes[10] = m_data[21];
	m_axes[11] = m_data[22];
	m_axes[12] = m_data[23];
	m_axes[13] = m_data[24];
	m_axes[14] = m_data[25];
	m_axes[15] = m_data[26];
	m_axes[16] = -(m_data[42]<<8 | m_data[43]);
	m_axes[17] = m_data[44]<<8 | m_data[45];
	m_axes[18] = m_data[46]<<8 | m_data[47];
	emit updated();
}

void SixAxis::setLeds(int leds) {
	QLocalSocket *io = SixAxisDaemon::m_instance->m_io;
	io->write("l");
	io->write((const char *)&m_addr, sizeof(m_addr));
	io->write((const char *)&leds, sizeof(int));
}

void SixAxis::setRumble(int weak, int strong) {
	QLocalSocket *io = SixAxisDaemon::m_instance->m_io;
	io->write("r");
	io->write((const char *)&m_addr, sizeof(m_addr));
	int param = weak | (strong << 8);
	io->write((const char *)&param, sizeof(int));
}

void SixAxis::emitDisconnected() {
	emit disconnected();
}

SixAxisDaemon *SixAxisDaemon::instance() {
	if (!m_instance)
		m_instance = new SixAxisDaemon();
	return m_instance;
}

SixAxisDaemon::SixAxisDaemon() {
	m_io = new QLocalSocket(this);
	QObject::connect(m_io, SIGNAL(readyRead()), SLOT(processData()));
}

void SixAxisDaemon::start() {
	m_io->connectToServer("emumaster.sixaxis");
}

void SixAxisDaemon::stop() {
	m_io->close();
}

bool SixAxisDaemon::hasNewPad() const {
	return !m_newPads.isEmpty();
}

SixAxis *SixAxisDaemon::nextNewPad() {
	if (m_newPads.isEmpty())
		return 0;
	return m_newPads.takeFirst();
}

void SixAxisDaemon::processData() {
	while (m_io->bytesAvailable()) {
		char c;
		bdaddr_t addr;
		m_io->read(&c, 1);
		m_io->read((char *)&addr, sizeof(addr));

		if (c == 'n') {
			SixAxis *pad = new SixAxis(&addr, this);
			QObject::connect(pad, SIGNAL(destroyed(QObject*)), SLOT(padDestroyed(QObject*)));
			m_pads.append(pad);
			m_newPads.append(pad);
			emit newPad();
		} else if (c == 'd') {
			SixAxis *pad = padByAddr(&addr);
			if (pad) {
				m_pads.removeOne(pad);
				pad->emitDisconnected();
			}
		} else if (c == 'p') {
			SixAxis *pad = padByAddr(&addr);
			int len;
			m_io->read((char *)&len, sizeof(int));
			if (!pad || len > 64) {
				m_io->read(len);
			} else {
				m_io->read(pad->m_data, len);
				pad->processData();
			}
		}
	}
}

SixAxis *SixAxisDaemon::padByAddr(bdaddr_t *addr) {
	for (int i = 0; i < m_pads.size(); i++) {
		SixAxis *pad = m_pads.at(i);
		if (bacmp(&pad->m_addr, addr) == 0)
			return pad;
	}
	return 0;
}

void SixAxisDaemon::padDestroyed(QObject *o) {
	SixAxis *pad = static_cast<SixAxis *>(o);
	m_pads.removeOne(pad);
	m_newPads.removeOne(pad);
}
