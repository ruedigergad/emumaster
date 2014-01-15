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
	property alias iconSource: img.source
	property alias text: label.text

	width: parent.width; height: 20
	border.width: 1; border.color: "#606060"
	color: "white"

	Item {
		width: childrenRect.width
		height: parent.height
		anchors.horizontalCenter: parent.horizontalCenter

		Image {
			id: img
			x: 4
			anchors.verticalCenter: parent.verticalCenter
		}
		Text {
			id: label
			anchors.left: img.right; anchors.leftMargin: 4
			anchors.verticalCenter: parent.verticalCenter
			font.pointSize: 8
			font.bold: true
		}
	}
}
