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
import com.nokia.meego 1.1
import EmuMaster 1.0

Item {
	width: parent.width
	height: childrenRect.height

	Column {
		id: column
		width: parent.width

		ButtonRow {
			anchors.horizontalCenter: parent.horizontalCenter
			exclusive: false
			spacing: 5

			Button {
				text: qsTr("Add")
				onClicked: addCheatSheet.clearAndOpen()
			}
			Button {
				text: qsTr("Remove")
				onClicked: emu.cheats.removeAt(cheatsView.currentIndex)
			}
		}
		Repeater {
			property int currentIndex: 0
			id: cheatsView
			model: emu.cheats
			delegate: SelectionItem {
				titleText: codeDescription
				subtitleText: codeList.join(",")
				onClicked: cheatsView.currentIndex = index
				selected: cheatsView.currentIndex === index
				iconSource: ""
				width: column.width

				Switch {
					id: cheatSwitch
					anchors {
						verticalCenter: parent.verticalCenter
						right: parent.right
						rightMargin: 20
					}
					platformStyle: SwitchStyle { inverted: true }
					onCheckedChanged: emu.cheats.setEnabled(index, checked)
					Component.onCompleted: checked = codeEnabled
				}
			}
		}
	}

	Sheet {
		id: addCheatSheet
		property variant codeList: []

		acceptButton.enabled: addCheatSheet.codeList.length !== 0 && descriptionEdit.text !== ""
		acceptButtonText: qsTr("Add")
		rejectButtonText: qsTr("Cancel")

		content: Column {
			y:10
			spacing: 10
			anchors.fill: parent

			Label { text: qsTr("Group Description:") }
			TextField {
				id: descriptionEdit
				anchors { left: parent.left; right: parent.right }
				onAccepted:codeEdit.focus = true
			}

			ButtonRow {
				id: generationSelection
				anchors.horizontalCenter: parent.horizontalCenter

				Button {
					property bool v3: true
					text: "GameShark V3"
				}
				Button {
					property bool v3: false
					text: "GameShark V1"
				}
			}

			Label { text: qsTr("GameShark Codes:") }
			Repeater {
				model: addCheatSheet.codeList
				delegate: Label {
					text: modelData
				}
			}

			TextField {
				id: codeEdit
				anchors { left: parent.left; right: parent.right }
				inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
				onAccepted: {
					var copy = addCheatSheet.codeList
					copy.push(codeEdit.text)
					addCheatSheet.codeList = copy
					codeEdit.text = ""
				}
				validator: GbaGameSharkValidator { }
			}
		}

		onAccepted: emu.cheats.addNew(addCheatSheet.codeList,
									  descriptionEdit.text,
									  generationSelection.checkedButton.v3)

		function clearAndOpen() {
			codeList = []
			descriptionEdit.text = ""
			codeEdit.text = ""
			open()
		}
	}
}
