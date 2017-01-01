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

#include "emuview.h"
#include "emu.h"
#include "configuration.h"
#include "emuthread.h"
#include "frameitem.h"
#include "hostvideo.h"
#include "hostaudio.h"
#include "hostinput.h"
#include "stateimageprovider.h"
#include "statelistmodel.h"
#include "pathmanager.h"
#include "hostinputdevice.h"
#include "touchinputdevice.h"
#include "stringlistproxy.h"
#include <QQuickView>
#include <QQmlContext>
#include <QtQml>
//#include <qdeclarative.h>
#include <QCloseEvent>
#include <QApplication>
//#include <QDesktopWidget>
#include <QQmlEngine>
#include <QTimer>
#include <QDir>
#include <QSettings>
#include <QUdpSocket>
#include <QColor>
#include <QUrl>

EmuView::EmuView(Emu *emu, const QString &diskFileName, QQuickView *view, QObject *parent) :
    QObject(parent),
    m_view(view),
	m_emu(emu),
	m_diskFileName(diskFileName),
	m_running(false),
	m_backgroundCounter(qAbs(qrand())/2),
	m_quit(false),
	m_pauseRequested(false),
	m_slotToBeLoadedOnStart(StateListModel::InvalidSlot),
	m_audioEnable(true),
	m_autoSaveLoadEnable(true),
	m_safetyTimerDisabled(false),
	m_swipeEnabled(false),
	m_runInBackground(false),
	m_lrButtonsVisible(false)
{
	Q_ASSERT(m_emu != 0);

	Configuration::setupAppInfo();
	pathManager.setCurrentEmu(m_emu->name());

	m_thread = new EmuThread(m_emu);
	m_hostInput = new HostInput(m_emu);
	m_hostAudio = new HostAudio(m_emu);
	QObject::connect(m_hostInput, SIGNAL(quit()), SLOT(close()));
	QObject::connect(m_hostInput, SIGNAL(pause()), SLOT(pause()));
	QObject::connect(m_hostInput, SIGNAL(devicesChanged()),
					 SIGNAL(inputDevicesChanged()));

	m_stateListModel = new StateListModel(m_emu, m_diskFileName);

	if (!loadConfiguration())
		m_error = constructSlErrorString();

	// TODO detect if USB mass storage is used

	if (m_error.isEmpty()) {
		QString diskPath = QString("%1/%2")
				.arg(pathManager.diskDirPath())
				.arg(m_diskFileName);
		bool ok = m_emu->init(diskPath, &m_error);
		if (!ok && m_error.isEmpty())
			m_error = tr("Unknown error!");
	}

	m_safetyTimer = new QTimer(this);
	m_safetyTimer->setInterval(10000);
	m_safetyTimer->setSingleShot(false);
	QObject::connect(m_safetyTimer, SIGNAL(timeout()), SLOT(onSafetyEvent()));

    m_audioEnable = true;
    showEmulationView();

	registerClassesInQml();
    m_view->setSource(QUrl("/opt/emumaster/qml/base/EmuView.qml"));
    m_view->show();
}

EmuView::~EmuView()
{
	if (m_error.isEmpty()) {
		if (m_autoSaveLoadEnable)
			m_stateListModel->saveState(StateListModel::AutoSaveLoadSlot);
		// auto save screenshot
		saveScreenShotIfNotExists();
		m_emu->shutdown();
	}
	delete m_thread;
	delete m_stateListModel;
	delete m_hostAudio;
	delete m_hostInput;
}

// two-stage pause preventing deadlocks
void EmuView::pause()
{
	if (!m_running || m_pauseRequested)
		return;
	m_closeTries = 0;
	m_pauseRequested = true;

	m_safetyTimer->stop();
	QObject::disconnect(m_thread, SIGNAL(frameGenerated(bool)),
						this, SLOT(onFrameGenerated(bool)));
	m_thread->pause();
	QMetaObject::invokeMethod(this, "pauseStage2", Qt::QueuedConnection);
}

void EmuView::pauseStage2()
{
	// the code below may be seen as bloat but it is needed
	// we are waiting for the thread to exit, but at the same
	// we allow blocking queued repaints from the thread
	if (m_thread->isRunning()) {
		m_closeTries++;
		if (m_closeTries > 40) {
			m_thread->terminate();
			m_error = tr("Emulated system is not responding.");
		} else {
			QTimer::singleShot(10, this, SLOT(pauseStage2()));
			return;
		}
	}
	m_pauseRequested = false;
	m_running = false;

	if (m_quit) {
		close();
		return;
	}
	if (m_slotToBeLoadedOnStart != StateListModel::InvalidSlot && m_error.isEmpty()) {
		if (m_stateListModel->loadState(m_slotToBeLoadedOnStart)) {
			m_slotToBeLoadedOnStart = StateListModel::InvalidSlot;
			resume();
		}
		return;
	}
    emit pauseStage2Finished();
}

void EmuView::resume()
{
	Q_ASSERT(m_error.isEmpty());

	if (m_running)
		return;

	QObject::connect(m_thread, SIGNAL(frameGenerated(bool)),
					 this, SLOT(onFrameGenerated(bool)),
					 Qt::BlockingQueuedConnection);

	if (!m_safetyTimerDisabled) {
		m_safetyCheck = false;
		m_safetyTimer->start();
	}

	m_thread->resume();

	if (m_slotToBeLoadedOnStart != StateListModel::InvalidSlot)
		QTimer::singleShot(1000, this, SLOT(pause()));

	m_running = true;
}

bool EmuView::close()
{
	m_quit = true;
	if (m_running) {
		pause();
		return false;
	} else {
		qApp->quit();
		return true;
	}
}

void EmuView::showEmulationView()
{
	if (!m_running) {
		resume();
//		setSwipeEnabled(m_swipeEnabled);
		if (m_audioEnable)
			m_hostAudio->open();
	}
}

void EmuView::saveScreenShotIfNotExists()
{
	QString diskTitle = QFileInfo(m_diskFileName).completeBaseName();
	QString path = pathManager.screenShotPath(diskTitle);

    if (!QFile::exists(path)) {
		saveScreenShot();
    }
}

void EmuView::saveScreenShot()
{
	QImage img = m_emu->frame().copy(m_emu->videoSrcRect().toRect());
	img = img.convertToFormat(QImage::Format_ARGB32);
	QString diskTitle = QFileInfo(m_diskFileName).completeBaseName();
	img.save(pathManager.screenShotPath(diskTitle));
	QByteArray ba;
	QDataStream s(&ba, QIODevice::WriteOnly);
	s << m_diskFileName;
	QUdpSocket sock;
	sock.writeDatagram(ba, QHostAddress::LocalHost, 5798);
}

QString EmuView::extractArg(const QStringList &args,
								const QString &argName)
{
	int optionArgIndex = args.indexOf(argName);
	if (optionArgIndex >= 0) {
		if (args.size() > optionArgIndex+1)
			return args.at(optionArgIndex+1);
	}
	return QString();
}

void EmuView::parseConfArg(const QString &arg)
{
	QStringList lines = arg.split(',', QString::SkipEmptyParts);
	foreach (QString line, lines) {
		QStringList lineSplitted = line.split('=', QString::SkipEmptyParts);
		if (lineSplitted.size() != 2) {
			qDebug("Unknown conf option: %s", qPrintable(line));
		} else {
			QString key = lineSplitted.at(0);
			QString value = lineSplitted.at(1);
			emConf.setValue(key, value);
		}
	}
}

void EmuView::onFrameGenerated(bool videoOn)
{
	m_safetyCheck = true;

	if (m_audioEnable) {
		m_hostAudio->sendFrame();
    }

    if (videoOn) {
		emit videoFrameChanged(m_emu->frame().copy(m_emu->videoSrcRect().toRect()));
    }
	// sync input with the emulation
	m_hostInput->sync();
}

int EmuView::determineLoadSlot(const QStringList &args)
{
	int slot = StateListModel::InvalidSlot;

	// check if auto load if forced to the state specified by an argument
	if (args.contains("-autoSaveLoadEnable"))
		m_autoSaveLoadEnable = true;
	else if (args.contains("-autoSaveLoadDisable"))
		m_autoSaveLoadEnable = false;

	if (m_autoSaveLoadEnable)
		slot = StateListModel::AutoSaveLoadSlot;

	QString stateArg = extractArg(args, "-state");
	if (!stateArg.isEmpty()) {
		bool ok;
		slot = stateArg.toInt(&ok);
		if (!ok) {
			qDebug("invalid -state arg passed: %s", qPrintable(stateArg));
			slot = StateListModel::InvalidSlot;
		}
	}
	if (slot != StateListModel::InvalidSlot) {
		if (!m_stateListModel->exists(slot)) {
			if (slot != StateListModel::AutoSaveLoadSlot)
				qDebug("slot passed by -state arg not found: %s", qPrintable(stateArg));
			slot = StateListModel::InvalidSlot;
		}
	}
	return slot;
}

bool EmuView::loadConfiguration()
{
	QSettings s;
	m_autoSaveLoadEnable = s.value("autoSaveLoadEnable").toBool();

	emConf.setValue("version", QCoreApplication::applicationVersion());

	QStringList args = QCoreApplication::arguments();

	m_slotToBeLoadedOnStart = determineLoadSlot(args);

	// load conf from state
	if (m_slotToBeLoadedOnStart != StateListModel::InvalidSlot) {
		emsl.loadConfOnly = true;
		if (!m_stateListModel->loadState(m_slotToBeLoadedOnStart))
			return false;
		emsl.loadConfOnly = false;
	}

	// load conf from arg
	QString confArg = extractArg(args, "-conf");
	parseConfArg(confArg);

	// load conf from global settings
	finishSetupConfiguration();

	return true;
}

void EmuView::onSlFailed()
{
	if (!emsl.save && emsl.abortIfLoadFails) {
		fatalError(constructSlErrorString());
	} else {
		emit faultOccured(constructSlErrorString());
	}
}

QString EmuView::constructSlErrorString() const
{
	QString result;
	if (emsl.save)
		result = tr("Saving state failed: ");
	else
		result = tr("Loading state failed: ");
	result += emsl.error;
	return result;
}

void EmuView::fatalError(const QString &errorStr)
{
	m_error = errorStr;
    qDebug() << "Fatal Error: " << m_error;
}

void EmuView::registerClassesInQml()
{
	qmlRegisterType<StringListProxy>("EmuMaster", 1, 0, "StringListProxy");
	qmlRegisterType<FrameItem>("EmuMaster", 1, 0, "FrameItem");
    m_view->rootContext()->setContextProperty("emuView", this);
	m_view->rootContext()->setContextProperty("emu", static_cast<QObject *>(m_emu));
	m_view->rootContext()->setContextProperty("stateListModel", static_cast<QObject *>(m_stateListModel));
	m_view->engine()->addImageProvider("state", new StateImageProvider(m_stateListModel));
}

QList<QObject *> EmuView::inputDevices() const
{
	QList<QObject *> ret;
	ret.reserve(m_hostInput->devices().size());
	foreach (HostInputDevice *dev, m_hostInput->devices())
		ret.append(dev);
	return ret;
}

void EmuView::onSafetyEvent()
{
	if (!m_safetyCheck) {
		m_thread->terminate();
		fatalError(tr("Emulated system is not responding"));
	}
	m_safetyCheck = false;
}

void EmuView::onStateLoaded()
{
	m_hostInput->loadFromConf();
}

void EmuView::paintEvent(QPaintEvent *)
{
	// make sure no paint will occur for this widget
}

void EmuView::closeEvent(QCloseEvent *e)
{
	e->ignore();
	close();
}

void EmuView::changeEvent(QEvent *e)
{
//	QWidget::changeEvent(e);
//	if (e->type() == QEvent::WindowStateChange && windowState().testFlag(Qt::WindowMinimized)) {
//		if (!m_runInBackground)
//			pause();
//	}
}

void EmuView::focusOutEvent(QFocusEvent *)
{
//	if (!windowState().testFlag(Qt::WindowMinimized))
//		pause();
}

/*! Enables/disables swipe. */
void EmuView::setSwipeEnabled(bool on)
{
	Q_UNUSED(on)
}

//-------------------------------SETTINGS SECTION-------------------------------

void EmuView::finishSetupConfiguration()
{
	QSettings s;
	m_swipeEnabled = loadOptionFromSettings(s, "swipeEnable").toBool();
	m_thread->setFrameSkip(loadOptionFromSettings(s, "frameSkip").toInt());
	setAudioEnabled(loadOptionFromSettings(s, "audioEnable").toBool());
	m_runInBackground = loadOptionFromSettings(s, "runInBackground").toBool();

	m_hostInput->setPadOpacity(loadOptionFromSettings(s, "padOpacity").toReal());
	TouchInputDevice *touch = m_hostInput->touchInputDevice();
	touch->setHapticFeedbackEnabled(loadOptionFromSettings(s, "hapticFeedbackEnable").toBool());
	touch->setButtonsVisible(loadOptionFromSettings(s, "buttonsVisible").toBool());
	touch->setGridVisible(loadOptionFromSettings(s, "gridVisible").toBool());
	touch->setGridColor(loadOptionFromSettings(s, "gridColor").value<QColor>());
	setLRButtonsVisible(loadOptionFromSettings(s, "lrButtonsVisible").toBool());
	touch->setDpadAreaSize(loadOptionFromSettings(s, "touchAreaSize").toInt());
	touch->setDpadAreaDiagonalSize(loadOptionFromSettings(s, "touchAreaDiagonalSize").toInt());
}

QVariant EmuView::loadOptionFromSettings(QSettings &s, const QString &name) const
{
	QVariant option = emConf.value(name);
	if (option.isNull())
		option = s.value(name, emConf.defaultValue(name));
	return option;
}

void EmuView::setFpsVisible(bool on)
{
}

bool EmuView::isFpsVisible() const
{
//	return m_hostVideo->isFpsVisible();
    return false;
}

void EmuView::setFrameSkip(int n)
{
	if (m_thread->frameSkip() != n) {
		m_thread->setFrameSkip(n);
		emConf.setValue("frameSkip", n);
		emit frameSkipChanged();
	}
}

int EmuView::frameSkip() const
{
	return m_thread->frameSkip();
}

void EmuView::setAudioEnabled(bool on)
{
	if (m_audioEnable != on) {
		m_audioEnable = on;
		m_emu->setAudioEnabled(on);
		emConf.setValue("audioEnable", on);
		emit audioEnableChanged();
	}
}

bool EmuView::isAudioEnabled() const
{
	return m_audioEnable;
}

void EmuView::setPadOpacity(qreal opacity)
{
	if (m_hostInput->padOpacity() != opacity) {
		m_hostInput->setPadOpacity(opacity);
		emConf.setValue("padOpacity", opacity);
		emit padOpacityChanged();
	}
}

qreal EmuView::padOpacity() const
{
	return m_hostInput->padOpacity();
}

void EmuView::setKeepAspectRatio(bool on)
{
//	if (m_hostVideo->keepApsectRatio() != on) {
//		m_hostVideo->setKeepAspectRatio(on);
//		emConf.setValue("keepAspectRatio", on);
//		emit keepAspectRatioChanged();
//	}
}

bool EmuView::keepAspectRatio() const
{
//	return m_hostVideo->keepApsectRatio();
    return false;
}

void EmuView::setLRButtonsVisible(bool on)
{
	if (m_lrButtonsVisible != on) {
		if (m_emu->name() != "nes" && m_emu->name() != "amiga") {
			m_lrButtonsVisible = on;
			m_hostInput->touchInputDevice()->setLRVisible(m_lrButtonsVisible);
			emConf.setValue("lrButtonsVisible", on);
			emit lrButtonsVisibleChanged();
		}
	}
}

bool EmuView::areLRButtonsVisible() const
{
	return m_lrButtonsVisible;
}

void EmuView::disableSafetyTimer()
{
	m_safetyTimerDisabled = true;
	m_safetyTimer->stop();
}

