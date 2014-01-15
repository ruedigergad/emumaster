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

#include "accelinputdevice.h"
#include "emu.h"
#include <QSettings>

/*! \class AccelInputDevice
	The AccelInputDevice class reads accelerometer and acts as input device
	of the emulated system. The accelerometer is used only when needed,
	because often the AccelInputDevice is not connected in any way to the
	emulated system.

	Available functions in the emulation:
	  - None
	  - Pad A
	  - Pad B
 */

/*! Creates an AccelInputDevice with the given \a parent. */
AccelInputDevice::AccelInputDevice(QObject *parent) :
	HostInputDevice("accel", QObject::tr("Accelerometer"), parent)
{
	// use accelerometer device only when needed
	m_accelerometer = 0;

	setupEmuFunctionList();

	QObject::connect(this, SIGNAL(emuFunctionChanged()), SLOT(onEmuFunctionChanged()));

	loadCalibration();
}

/*! \internal */
void AccelInputDevice::setupEmuFunctionList()
{
	QStringList functionNameList;
	functionNameList << tr("None")
					 << tr("Pad A")
					 << tr("Pad B");
	setEmuFunctionNameList(functionNameList);
}

/*! \internal */
void AccelInputDevice::loadCalibration()
{
	// up and right vectors represents calibration
	QSettings s;
	s.beginGroup("accelerometer");
	// up vector
	m_upVector.setX(s.value("up.x", 0.0f).toReal());
	m_upVector.setY(s.value("up.y", 0.0f).toReal());
	m_upVector.setZ(s.value("up.z", 9.8f).toReal());
	// right vector
	m_rightVector.setX(s.value("right.x", 0.0f).toReal());
	m_rightVector.setY(s.value("right.y", 9.8f).toReal());
	m_rightVector.setZ(s.value("right.z", 0.0f).toReal());
	s.endGroup();
}

/*!
	Calibrates accelerometer with the given vectors:
	  - \a init - init position
	  - \a up - device rotated up
	  - \a right - device rotated right
 */
void AccelInputDevice::calibrate(const QVector3D &init,
								 const QVector3D &up,
								 const QVector3D &right)
{
	// only two vectors are really needed: up and right directions
	m_upVector = up - init;
	m_rightVector = right - init;

	// save vectors
	QSettings s;
	s.beginGroup("accelerometer");
	// up vector
	s.setValue("up.x", m_upVector.x());
	s.setValue("up.y", m_upVector.y());
	s.setValue("up.z", m_upVector.z());
	// right vector
	s.setValue("right.x", m_rightVector.x());
	s.setValue("right.y", m_rightVector.y());
	s.setValue("right.z", m_rightVector.z());
	s.endGroup();
}

/*! \internal */
void AccelInputDevice::onEmuFunctionChanged()
{
	// reset conversions on emu function change
	m_converted = false;
	m_buttons = 0;
	setEnabled(emuFunction() > 0);
}

/*! \internal */
void AccelInputDevice::setEnabled(bool on)
{
	// check if on is equal to the current state
	if (on == (m_accelerometer != 0))
		return;

	// turn off/on accelerometer
	if (!on) {
		delete m_accelerometer;
		m_accelerometer = 0;
	} else {
		m_accelerometer = new QAccelerometer(this);
		QObject::connect(m_accelerometer, SIGNAL(readingChanged()),
						 SLOT(onReadingChanged()));
		m_accelerometer->start();
	}
}

/*! \reimp */
void AccelInputDevice::sync(EmuInput *emuInput)
{
	// exit if turned off
	if (emuFunction() <= 0)
		return;
	// convert if needed
	if (!m_converted) {
		convert();
		m_converted = true;
	}
	// write data
	int padIndex = emuFunction() - 1;
	emuInput->pad[padIndex].setButtons(m_buttons);
	// TODO accelerometer: write analog values
}

/*! \internal */
void AccelInputDevice::onReadingChanged()
{
	// save reading
	QAccelerometerReading *reading = m_accelerometer->reading();
	m_read = QVector3D(reading->x(), reading->y(), reading->z());
	// force convert next time
	m_converted = false;
}

/*! \internal */
void AccelInputDevice::convert()
{
	// convert accelerometer readings to pad data
	// first calculate how much the device is rotated
	qreal up = QVector3D::dotProduct(m_read, m_upVector);
	qreal right = QVector3D::dotProduct(m_read, m_rightVector);
	// up and right are now 9.8^2 max if device is idle (not moving)
	// switch buttons if direction exceeds +-treshold
	qreal treshold = (9.8f*9.8f) * 0.5f;

	m_buttons = 0;
	if (up > treshold)
		m_buttons |= EmuPad::Button_Up;
	else if (up < -treshold)
		m_buttons |= EmuPad::Button_Down;

	if (right > treshold)
		m_buttons |= EmuPad::Button_Right;
	else if (right < -treshold)
		m_buttons |= EmuPad::Button_Left;
}
