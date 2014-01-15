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

#ifndef DISKGALLERY_H
#define DISKGALLERY_H

class DiskListModel;
#include <QQuickView>
#include <QUdpSocket>
#include <QSettings>
//#include <qmusbmode.h>

class DiskGallery : public QQuickView
{
	Q_OBJECT
	Q_PROPERTY(bool massStorageInUse READ massStorageInUse NOTIFY massStorageInUseChanged)
public:
	explicit DiskGallery(QWindow *parent = 0);
	~DiskGallery();

	Q_INVOKABLE void launch(int index);
	Q_INVOKABLE void advancedLaunch(int index, int autoSaveLoad, const QString &confStr);

	Q_INVOKABLE void donate();
	Q_INVOKABLE void maemoThread();
	Q_INVOKABLE void wiki();
	Q_INVOKABLE void sixAxisMonitor();

	Q_INVOKABLE QVariant globalOption(const QString &name);
	Q_INVOKABLE void setGlobalOption(const QString &name, const QVariant &value);

	bool massStorageInUse() const;
signals:
	void massStorageInUseChanged();
private slots:
	void receiveDatagram();
	void checkMassStorage();
private:
	void setupQml();

	DiskListModel *m_diskListModel;
	QUdpSocket m_sock;
	QSettings m_settings;
//	MeeGo::QmUSBMode *m_usbMode;
//	MeeGo::QmUSBMode::Mode m_lastMode;
};

#endif // DISKGALLERY_H
