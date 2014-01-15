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

Item {
	id: button
	property string name
	property bool alphaVersion: false

	width: 240; height: appWindow.inPortrait ? 240 : 200
	Image {
		anchors.horizontalCenter: parent.horizontalCenter
		source: "../img/collection-" + name + ".png"
	}
	Text {
		text: button.name
		anchors.horizontalCenter: button.horizontalCenter
		anchors.top: button.top
		anchors.topMargin: 10
		font.pointSize: 20
		font.bold: true
		font.capitalization: Font.AllUppercase
		color: "white"
	}
	MouseArea {
		id: mouseArea
		anchors.fill: parent
		onClicked: selectCollection(button.name)
	}
	Behavior on scale {
		NumberAnimation { duration: 100 }
	}
	states: [
		State {
			name: "pressed"
			when: mouseArea.pressed
			PropertyChanges {
				target: button
				scale: 0.8
			}
		}
	]
}
