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
	id: keybMappingPage

	property int currentChanging: -1
	signal refreshText

	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: appWindow.pageStack.pop()
		}
	}
	Label {
		id: titleLabel
		text: qsTr("Keyboard to Pad mapping")
		anchors.horizontalCenter: parent.horizontalCenter
	}

	Flickable {
		id: flickable
		anchors {
			top: titleLabel.bottom
			topMargin: 10
			bottom: parent.bottom
		}
		width: parent.width
		flickableDirection: Flickable.VerticalFlick
		clip: true
		contentHeight: items.height

		Column {
			id: items
			width: parent.width
			height: childrenRect.height
			spacing: 10

			Repeater {
				model: 14

				KeybMappingItem {
					id: item
					buttonName: keybInputDevice.padButtonName(index)
					hostKeyText: keybInputDevice.padButtonText(index)
					onClicked: {
						if (keybMappingPage.currentChanging !== index)
							keybMappingPage.currentChanging = index
						else
							keybMappingPage.currentChanging = -1
					}
					Connections {
						target: keybMappingPage
						onRefreshText: item.hostKeyText = keybInputDevice.padButtonText(index)
					}
					states: [
						State {
							name: "changing"; when: keybMappingPage.currentChanging === index
							PropertyChanges {
								target: item
								hostKeyText: ".."
							}
						}
					]
				}
			}
		}
	}
	ScrollDecorator { flickableItem: flickable }

	Keys.onPressed: {
		if (keybMappingPage.currentChanging !== -1) {
			var text = ""
			switch (event.key) {
			case Qt.Key_Backspace: text = "Backspace"; break
			case Qt.Key_Return: text = "Enter"; break
			case Qt.Key_Enter: text = "Enter"; break
			case Qt.Key_Control: text = "Control"; break
			case Qt.Key_Shift: text = "Shift"; break
			case Qt.Key_Left: text = "Left"; break
			case Qt.Key_Right: text = "Right"; break
			case Qt.Key_Up: text = "Up"; break
			case Qt.Key_Down: text = "Down"; break
			case Qt.Key_Space: text = "Space"; break
			default:
				if (event.key < 0x100)
					text = event.text
				break
			}

			if (text !== "")
				keybInputDevice.setPadButton(keybMappingPage.currentChanging, event.key, text)
			keybMappingPage.currentChanging = -1
			refreshText()
		}
	}
}
