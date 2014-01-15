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

import QtQuick 2.0
import com.nokia.meego 1.1
import QtMobility.sensors 1.2

Page {
	id: accelCalibrationPage
	orientationLock: PageOrientation.LockPortrait

	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: appWindow.pageStack.pop()
		}
	}

	property int step: 0

	property variant initVec
	property variant upVec
	property variant rightVec

	Accelerometer {
		id: accelerometer
		active: true

		function getVector3D() {
			var r = accelerometer.reading
			return Qt.vector3d(r.x, r.y, r.z)
		}
	}

	Column {
		width: parent.width
		spacing: 10

		Label {
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("Place your phone in the initial position \n and click OK")
		}
		Button {
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("OK")
			onClicked: {
				initVec = accelerometer.getVector3D()
				step = step+1
			}
		}

		Label {
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("Rotate your phone to the up \n and click OK")
			visible: step >= 1
		}
		Button {
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("OK")
			visible: step >= 1
			onClicked: {
				upVec = accelerometer.getVector3D()
				step = step+1
			}
		}

		Label {
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("Move the phone back to the initial position,\n rotate to the right \n and click OK")
			visible: step >= 2
		}
		Button {
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("OK")
			visible: step >= 2
			onClicked: {
				rightVec = accelerometer.getVector3D()
				accelInputDevice.calibrate(initVec, upVec, rightVec)
				appWindow.pageStack.pop()
			}
		}
	}
	Button {
		anchors {
			horizontalCenter: parent.horizontalCenter
			bottom: parent.bottom
		}
		text: qsTr("Reset to default")
		onClicked: {
			var defaultInitVec = Qt.vector3d(0.0, 0.0, 0.0)
			var defaultUpVec = Qt.vector3d(0.0, 0.0, 9.8)
			var defaultRightVec = Qt.vector3d(0.0, 9.8, 0.0)
			accelInputDevice.calibrate(defaultInitVec, defaultUpVec, defaultRightVec)
			appWindow.pageStack.pop()
		}
	}
}
