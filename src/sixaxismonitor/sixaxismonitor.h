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

#ifndef SIXAXISMONITOR_H
#define SIXAXISMONITOR_H

#include <QDeclarativeView>
#include <QStringList>
class SixAxisServer;
class SixAxisDevice;

class SixAxisMonitor : public QDeclarativeView
{
	Q_OBJECT
	Q_PROPERTY(QStringList addresses READ addresses NOTIFY addressesChanged)
public:
	explicit SixAxisMonitor();
	Q_INVOKABLE QString start(const QString &develSuPassword);
	QStringList addresses() const;
	Q_INVOKABLE void identify(int i);
	Q_INVOKABLE void disconnectDev(int i);
private slots:
	void onIdentifyEvent();
	void onIdentifyDevDestroyed();
signals:
	void addressesChanged();
private:
	SixAxisServer *m_server;
	SixAxisDevice *m_identifyDev;
	int m_identifyCounter;
	int m_identifyOldLeds;
};

#endif // SIXAXISMONITOR_H
