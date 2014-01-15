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

#include "hostinput.h"
#include "hostvideo.h"
#include "emu.h"
#include "configuration.h"
#include "touchinputdevice.h"
//#include "accelinputdevice.h"
#include "keybinputdevice.h"
#include "memutils.h"
#include <QDebug>
#include <QKeyEvent>
#include <QTouchEvent>

/*!
	\class HostInput
	HostInput class manages input devices. It filters input events from main
	window and passes them to appropriate objects. HostInput starts sixaxis
	server.

	The events are converted and passed to emulation only on update() call.
	This is done by HostVideo after each frame.
 */

/*! Creates a HostInput object with the given \a emu. */
HostInput::HostInput(Emu *emu) :
	m_emu(emu)
{
	m_padOpacity = emConf.defaultValue("padOpacity").toReal();
	// first device in the list is always touch device ...
	setupTouchDevice();
	// ... and second is always the keyboard
	m_devices.append(new KeybInputDevice(this));
//	m_devices.append(new AccelInputDevice(this));
}

/*! Destroys HostInput object. */
HostInput::~HostInput()
{
}

/*! \internal */
bool HostInput::eventFilter(QObject *o, QEvent *e)
{
	Q_UNUSED(o)
	if (e->type() == QEvent::KeyPress || e->type() == QKeyEvent::KeyRelease) {
		// filter key events
		bool down = (e->type() == QEvent::KeyPress);
		QKeyEvent *ke = static_cast<QKeyEvent *>(e);
		if (!ke->isAutoRepeat())
			keybInputDevice()->processKey(static_cast<Qt::Key>(ke->key()), down);
		ke->accept();
		return true;
	} else if (e->type() == QEvent::TouchBegin ||
			   e->type() == QEvent::TouchUpdate ||
			   e->type() == QEvent::TouchEnd) {
		// filter touch events
		processTouch(e);
		e->accept();
		return true;
	}
	return false;
}

/*! \internal */
void HostInput::processTouch(QEvent *e)
{
	// early processing to capture clicks on pause and quit buttons
	QTouchEvent *touchEvent = static_cast<QTouchEvent *>(e);
	const QList<QTouchEvent::TouchPoint> &points = touchEvent->touchPoints();
	for (int i = 0; i < points.size(); i++) {
		const QTouchEvent::TouchPoint &point = points.at(i);
		if (point.state() & Qt::TouchPointReleased)
			continue;
		int x = point.pos().x();
		int y = point.pos().y();
		if (y < 64) {
			if (x < 80)
                qDebug("Settings button should be handled in QML.");
//				emit pause();
			else if (x > 900) //FIXME: 900 is a hardcoded value.
                qDebug("Quit button should be handled in QML.");
//				emit quit();
		}
	}
	touchInputDevice()->processTouch(e);
}

void HostInput::setupTouchDevice()
{
	TouchInputDevice *touchDevice = new TouchInputDevice(this);

	if (m_emu->name() == "psx")
		touchDevice->setPsxButtonsEnabled(true);
	else if (m_emu->name() == "pico")
		touchDevice->setPicoButtonsEnabled(true);
	else if (m_emu->name() == "gba")
		touchDevice->setGbaButtonsEnabled(true);

#if defined(MEEGO_EDITION_HARMATTAN)
	touchDevice->setEmuFunction(1);
#endif
	m_devices.append(touchDevice);
}

/*! Returns touch input device. */
TouchInputDevice *HostInput::touchInputDevice() const
{
	return static_cast<TouchInputDevice *>(m_devices.first());
}

/*! Returns keyboard input device. */
KeybInputDevice *HostInput::keybInputDevice() const
{
	return static_cast<KeybInputDevice *>(m_devices.at(1));
}

/*! Changes opacity of images on touch screen to the given \a opacity. */
void HostInput::setPadOpacity(qreal opacity)
{
	m_padOpacity = opacity;
}

/*!
	\fn qreal HostInput::padOpacity() const
	Returns opacity of images on touch screen.
 */

/*! Paints images on touch screen using given \a painter. */
void HostInput::paint(QPainter *painter)
{
	if (m_padOpacity <= 0.0f)
		return;

	painter->setOpacity(m_padOpacity);
	touchInputDevice()->paint(painter);
}

/*!
	\fn QList<HostInputDevice *> HostInput::devices() const
	Returns a list of input devices available on host.
 */

/*! Synchronizes input values from host to the emulation. */
void HostInput::sync()
{
	EmuInput *emuInput = m_emu->input();
	memset32(emuInput, 0, sizeof(EmuInput)/4);
	for (int i = 0; i < m_devices.size(); i++)
		m_devices.at(i)->sync(emuInput);
}

/*! Loads function in the emulation for each input device.  */
void HostInput::loadFromConf()
{
	for (int i = 0; i < m_devices.size(); i++)
		m_devices.at(i)->updateEmuFunction();
}
