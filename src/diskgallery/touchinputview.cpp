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

#include "touchinputview.h"
#include <base/touchinputdevice.h>
#include <QPainter>

TouchInputView::TouchInputView(QDeclarativeItem *parent) :
	QQuickItem(parent)
{
	setFlag(ItemHasNoContents, false);
}

void TouchInputView::setTouchDevice(QObject *o)
{
	m_touchDevice = static_cast<TouchInputDevice *>(o);
	m_touchDevice->setEmuFunction(1);
	emit touchDeviceChanged();
}

QObject *TouchInputView::touchDevice() const
{
	return m_touchDevice;
}

void TouchInputView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	if (m_touchDevice)
		m_touchDevice->paint(painter);
}
