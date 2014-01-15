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

#ifndef ACCELINPUTDEVICE_H
#define ACCELINPUTDEVICE_H

#include "hostinputdevice.h"
#include <QAccelerometer>
#include <QVector3D>

QTM_USE_NAMESPACE

class BASE_EXPORT AccelInputDevice : public HostInputDevice
{
	Q_OBJECT
public:
	explicit AccelInputDevice(QObject *parent = 0);
	void sync(EmuInput *emuInput);

	Q_INVOKABLE void calibrate(const QVector3D &init,
							   const QVector3D &up,
							   const QVector3D &right);
private slots:
	void onEmuFunctionChanged();
	void onReadingChanged();
private:
	void setupEmuFunctionList();
	void loadCalibration();
	void setEnabled(bool on);
	void convert();

	QAccelerometer *m_accelerometer;
	QVector3D m_upVector;
	QVector3D m_rightVector;

	int m_buttons;
	QVector3D m_read;
	bool m_converted;
};

#endif // ACCELINPUTDEVICE_H
