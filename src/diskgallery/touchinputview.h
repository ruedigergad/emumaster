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

#ifndef TOUCHINPUTVIEW_H
#define TOUCHINPUTVIEW_H

#include <QQuickItem>
class TouchInputDevice;

class TouchInputView : public QQuickItem
{
	Q_OBJECT
	Q_PROPERTY(QObject *touchDevice READ touchDevice WRITE setTouchDevice NOTIFY touchDeviceChanged)
public:
	explicit TouchInputView(QQuickItem *parent = 0);

	void setTouchDevice(QObject *o);
	QObject *touchDevice() const;
signals:
	void touchDeviceChanged();
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);
private:
	TouchInputDevice *m_touchDevice;
};

#endif // TOUCHINPUTVIEW_H
