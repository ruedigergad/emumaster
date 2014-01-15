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

Item {
	id: sectionDelegate
	property int rightPad: 150
	property alias text: headerLabel.text

	width: parent.width
	height: 40
	Text {
		id: headerLabel
		width: sectionDelegate.rightPad
		height: parent.height
		anchors.right: parent.right
		font.pointSize: 18
		color: theme.inverted ? "#4D4D4D" : "#3C3C3C";
		horizontalAlignment: Text.AlignHCenter
		verticalAlignment: Text.AlignVCenter
	}
	Image {
		anchors.left: parent.left
		anchors.leftMargin: 10
		anchors.right: parent.right
		anchors.rightMargin: sectionDelegate.rightPad
		anchors.verticalCenter: headerLabel.verticalCenter
		source: "image://theme/meegotouch-groupheader" + (theme.inverted ? "-inverted" : "") + "-background"
	}
}
