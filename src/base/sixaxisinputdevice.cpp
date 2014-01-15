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

#include "sixaxisinputdevice.h"
#include "emu.h"
#include <sixaxisclient/sixaxis.h>

// TODO check mouse

SixAxisInputDevice::SixAxisInputDevice(SixAxis *sixAxis, QObject *parent) :
	HostInputDevice("sixaxis", QObject::tr("SixAxis"), parent),
	m_sixAxis(sixAxis)
{
	setupEmuFunctionList();

	QObject::connect(m_sixAxis, SIGNAL(updated()), SLOT(onSixAxisUpdated()));
	QObject::connect(m_sixAxis, SIGNAL(disconnected()), SLOT(deleteLater()));
	QObject::connect(this, SIGNAL(emuFunctionChanged()), SLOT(enEmuFunctionChanged()));

	m_sixAxis->setParent(this);
}

void SixAxisInputDevice::setupEmuFunctionList()
{
	QStringList functionNameList;
	functionNameList << tr("None")
					 << tr("Pad A")
					 << tr("Pad B")
					 << tr("Mouse A")
					 << tr("Mouse B")
					 << tr("Pad B + Mouse A")
					 << tr("Pad A + Mouse B");
	setEmuFunctionNameList(functionNameList);
}

const int SixAxisInputDevice::m_buttonsMapping[] =
{
	EmuPad::Button_Select,
	0,
	0,
	EmuPad::Button_Start,
	EmuPad::Button_Up,
	EmuPad::Button_Right,
	EmuPad::Button_Down,
	EmuPad::Button_Left,
	EmuPad::Button_L2,
	EmuPad::Button_R2,
	EmuPad::Button_L1,
	EmuPad::Button_R1,
	EmuPad::Button_X,
	EmuPad::Button_A,
	EmuPad::Button_B,
	EmuPad::Button_Y
};

void SixAxisInputDevice::enEmuFunctionChanged()
{
	m_converted = false;
	m_buttons = 0;
	m_mouseX = m_mouseY = 0;
	m_mouseButtons = 0;
}

void SixAxisInputDevice::onSixAxisUpdated()
{
	m_converted = false;
}

void SixAxisInputDevice::convertPad()
{
	int b = m_sixAxis->buttons();
	if (b & (1<<SixAxis::PS)) {
		emit pause();
		return;
	}
	m_buttons = 0;
	for (uint i = 0; i < sizeof(m_buttonsMapping)/sizeof(int); i++) {
		if (b & (1 << i))
			m_buttons |= m_buttonsMapping[i];
	}
}

void SixAxisInputDevice::convertMouse()
{
	m_mouseButtons = 0;

	if (m_sixAxis->buttons() & (1<<SixAxis::L3))
		m_mouseButtons |= 1;
	if (m_sixAxis->buttons() & (1<<SixAxis::R3))
		m_mouseButtons |= 2;

	m_mouseX = m_sixAxis->axis(SixAxis::LX);
	m_mouseY = m_sixAxis->axis(SixAxis::LY);

	// deadzones
	if (qAbs(m_mouseX) < 30)
		m_mouseX = 0;
	if (qAbs(m_mouseY) < 30)
		m_mouseY = 0;
}

void SixAxisInputDevice::sync(EmuInput *emuInput)
{
	if (emuFunction() <= 0)
		return;

	int padIndex = -1;
	if (emuFunction() <= 2)
		padIndex = emuFunction()-1;
	else if (emuFunction() >= 5)
		padIndex = 6-emuFunction();

	int mouseIndex = -1;
	if (emuFunction() >= 3 && emuFunction() <= 6)
		mouseIndex = emuFunction() - (emuFunction() & ~1);

	if (!m_converted) {
		if (padIndex >= 0)
			convertPad();
		if (mouseIndex >= 0)
			convertMouse();
		m_converted = true;
	}

	if (padIndex >= 0)
		emuInput->pad[padIndex].setButtons(m_buttons);
	if (mouseIndex >= 0) {
		emuInput->mouse[mouseIndex].setButtons(m_mouseButtons);
		emuInput->mouse[mouseIndex].addRel(m_mouseX, m_mouseY);
	}
}
