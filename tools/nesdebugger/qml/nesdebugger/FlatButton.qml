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

import QtQuick 1.1

Rectangle {
	id: button
	width: label.width + 30; height: 20
	color: "transparent"
	border.color: "#c8c8c8"; border.width: 1

	property alias text: label.text
	signal clicked
	property alias iconSource: icon.source

	Item {
		anchors.centerIn: parent
		width: childrenRect.width; height: parent.height
		Image {
			id: icon
			anchors.verticalCenter: parent.verticalCenter
		}
		Text {
			id: label
			anchors.left: icon.right; anchors.leftMargin: 4
			height: button.height
			verticalAlignment: Text.AlignVCenter
		}
	}
	MouseArea {
		id: mouseArea
		anchors.fill: parent
		hoverEnabled: true
		onClicked: button.clicked()
	}
	states: [
		State {
			name: "hover"; when: mouseArea.containsMouse
			PropertyChanges { target: button; color: "#a4dbfb" }
		}
	]
}
