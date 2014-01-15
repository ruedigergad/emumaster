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

import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
	id: imageListViewDelegate

	signal clicked
	signal pressed
	signal pressAndHold

	property alias imgSource: screenShot.source
	property alias text: itemLabel.text

	Image {
		id: screenShot
		width: parent.width
		height: parent.height-30
	}

	Label {
		id: itemLabel
		anchors.top: screenShot.bottom
		anchors.topMargin: 2
	}

	MouseArea {
		id: mouseArea
		anchors.fill: parent
		onClicked: imageListViewDelegate.clicked()
		onPressed: imageListViewDelegate.pressed()
		onPressAndHold: imageListViewDelegate.pressAndHold()
	}

	Behavior on scale {
		NumberAnimation { duration: 100 }
	}

	states: [
		State {
			name: "pressed"
			when: mouseArea.pressed
			PropertyChanges {
				target: imageListViewDelegate
				scale: 0.85
			}
		}
	]
}
