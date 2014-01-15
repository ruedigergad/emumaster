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

Page {
	id: saveIconPage

	property int diskIndex

	property alias imgSource: img.source
	property alias imgScale: imageScaler.value

	property int iconX: 0
	property int iconY: 0

	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: appWindow.pageStack.pop()
		}
		ToolIcon {
			iconId: "toolbar-done"
			onClicked: {
				if (rect.visible) {
					if (!diskListModel.addIconToHomeScreen(
								saveIconPage.diskIndex,
								imageScaler.value,
								iconX, iconY)) {
						errorDialog.message = qsTr("Could not save the icon!")
						errorDialog.open()
					} else {
						appWindow.pageStack.pop()
					}
				}
			}
		}
	}

	Label {
		y: 10
		anchors.horizontalCenter: parent.horizontalCenter
		text: qsTr("Select Icon Area")
	}

	Image {
		id: img
		anchors.centerIn: parent
		scale: imageScaler.value
	}
	Rectangle {
		id: rect
		x: img.x - (img.scale-1)*img.width/2 + saveIconPage.iconX
		y: img.y - (img.scale-1)*img.height/2 + saveIconPage.iconY
		width: 80
		height: 80
		color: Qt.rgba(1, 1, 1, 0.3)
		border.color: "white"
		border.width: 2
		visible: false
	}

	MouseArea {
		x: img.x - (img.scale-1)*img.width/2
		y: img.y - (img.scale-1)*img.height/2
		width: img.width*img.scale
		height: img.height*img.scale
		onClicked: {
			if (mouse.x >= 40 && mouse.y >= 40 && mouse.x+40 < width && mouse.y+40 < height) {
				saveIconPage.iconX = mouse.x - 40
				saveIconPage.iconY = mouse.y - 40
				rect.visible = true
			}
		}
	}
	Slider {
		id: imageScaler;
		anchors.bottom: parent.bottom
		width: parent.width
		orientation: Qt.Horizontal
		minimumValue: 0.1; maximumValue: 2.0
		value: 1.0
		onValueChanged: rect.visible = false
	}
}
