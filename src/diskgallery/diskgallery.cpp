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

#include "diskgallery.h"
#include "disklistmodel.h"
#include "diskimageprovider.h"
//#include "touchinputview.h"
//#include <base/accelinputdevice.h>
#include <base/keybinputdevice.h>
#include <base/touchinputdevice.h>
#include <base/pathmanager.h>
#include <base/configuration.h>
#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>

DiskGallery::DiskGallery(QWindow *parent) :
	QQuickView(parent)
{
	m_diskListModel = new DiskListModel(this);
	setupQml();

	m_sock.bind(QHostAddress::LocalHost, 5798);
	QObject::connect(&m_sock, SIGNAL(readyRead()), SLOT(receiveDatagram()));

//	m_usbMode = new MeeGo::QmUSBMode(this);
//	QObject::connect(m_usbMode, SIGNAL(modeChanged(MeeGo::QmUSBMode::Mode)),
//					 SLOT(checkMassStorage()));

//	m_lastMode = m_usbMode->getMode();
}

DiskGallery::~DiskGallery()
{
}

/** Configures QML window. */
void DiskGallery::setupQml()
{
//	qmlRegisterType<TouchInputView>("EmuMaster", 1, 0, "TouchInputView");
	engine()->addImageProvider("disk", new DiskImageProvider());

	QQmlContext *context = rootContext();
	context->setContextProperty("diskListModel", m_diskListModel);
	context->setContextProperty("diskGallery", this);
	context->setContextProperty("appVersion", QCoreApplication::applicationVersion());
//	context->setContextProperty("accelInputDevice", new AccelInputDevice(this));
	context->setContextProperty("keybInputDevice", new KeybInputDevice(this));
	context->setContextProperty("touchInputDevice", new TouchInputDevice(this));

	QObject::connect(engine(), SIGNAL(quit()), SLOT(close()));
	QString qmlPath = QString("%1/qml/gallery/main.qml")
			.arg(pathManager.installationDirPath());
	setSource(QUrl::fromLocalFile(qmlPath));
}

void DiskGallery::launch(int index)
{
	advancedLaunch(index, 0, QString());
}

/*!
	Launches the emulation with the given disk as \a index.
	\a autoSaveLoad can take {-1,0,1}, 0 for default, -1 to force off auto load,
	1 to force on.
 */
void DiskGallery::advancedLaunch(int index, int autoSaveLoad, const QString &confStr)
{
	QString diskFileName = m_diskListModel->getDiskFileName(index);
	QString diskEmuName = m_diskListModel->getDiskEmuName(index);
	if (diskFileName.isEmpty())
		return;
	QProcess process;
	QStringList args;
	args << QString("%1/bin/%2")
			.arg(pathManager.installationDirPath())
			.arg(diskEmuName);
	args << diskFileName;

	if (autoSaveLoad == 1)
		args << "-autoSaveLoadEnable";
	else if (autoSaveLoad == -1)
		args << "-autoSaveLoadDisable";

	if (!confStr.isEmpty()) {
		args << "-conf";
		args << confStr;
	}

    qDebug() << "Starting with args: " << args;
#if defined(MEEGO_EDITION_HARMATTAN)
    qDebug("Using single-instance.");
	process.startDetached("/usr/bin/single-instance", args);
#elif defined(Q_WS_MAEMO_5)
	QString app = args.takeFirst();
	process.startDetached(app, args);
#endif
}

/** Starts web browser with PayPal address for donation. */
void DiskGallery::donate()
{
	QStringList args;
	args << "https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=WUG37X8GMW9PQ&lc=US&item_number=emumaster&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted";
	QProcess::startDetached("grob", args);
}

/** Starts web browser with blog address. */
void DiskGallery::maemoThread()
{
	QStringList args;
	args << "http://talk.maemo.org/showthread.php?t=81136";
	QProcess::startDetached("grob", args);
}

/** Starts web browser with wiki address. */
void DiskGallery::wiki()
{
	QStringList args;
	args << "http://bitbucket.org/elemental/emumaster/wiki";
	QProcess::startDetached("grob", args);
}

/** Received from one of emulated systems. Decodes the packet,
	and updates the screen shot. */
void DiskGallery::receiveDatagram()
{
	QByteArray ba(m_sock.pendingDatagramSize(), Qt::Uninitialized);
	m_sock.readDatagram(ba.data(), ba.size());
	QDataStream s(&ba, QIODevice::ReadOnly);
	QString diskFileName;
	s >> diskFileName;
	m_diskListModel->updateScreenShot(diskFileName);
}

void DiskGallery::checkMassStorage()
{
//	MeeGo::QmUSBMode::Mode mode = m_usbMode->getMode();
//	if (m_lastMode == MeeGo::QmUSBMode::MassStorage ||
//		mode == MeeGo::QmUSBMode::MassStorage) {
//		emit massStorageInUseChanged();
//	}
//	m_lastMode = mode;
}

/** Starts SixAxis Monitor app. */
void DiskGallery::sixAxisMonitor()
{
	QStringList args;
	args << (pathManager.installationDirPath() + "/bin/sixaxismonitor");
	QProcess::startDetached("/usr/bin/single-instance", args);
}

QVariant DiskGallery::globalOption(const QString &name)
{
	return m_settings.value(name, emConf.defaultValue(name));
}

void DiskGallery::setGlobalOption(const QString &name, const QVariant &value)
{
	if (m_settings.value(name) != value) {
		m_settings.setValue(name, value);
		m_settings.sync();
	}
}

bool DiskGallery::massStorageInUse() const
{
//	return m_usbMode->getMode() == MeeGo::QmUSBMode::MassStorage;
    return false;
}
