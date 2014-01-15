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
import "../base"

Page {
	id: mainPage

	Column {
		id: column
		width: parent.width
		spacing: 8

		Label {
			id: helpLabel
			anchors.horizontalCenter: parent.horizontalCenter
			y: 10
			text: qsTr("Read wiki before using it !!!")
		}

		Label {
			id: enterPasswordLabel
			text: qsTr("Enter password for devel-su:")
		}

		TextField {
			id: develsuPasswordEdit
			echoMode: TextInput.Password
		}

		Button {
			id: startButton
			anchors.horizontalCenter: parent.horizontalCenter
			text: qsTr("Start Monitor")
			visible: develsuPasswordEdit.text.length !== 0
			onClicked: {
				var err = sixAxisMonitor.start(develsuPasswordEdit.text)
				if (err === "") {
					helpLabel.text = qsTr("Monitor Running")
					startButton.visible = false
					enterPasswordLabel.visible = false
					develsuPasswordEdit.visible = false
				} else {
					errDialog.message = err
					errDialog.open()
				}
			}
		}
	}

	ListView {
		id: sixAxisListView
		width: parent.width
		anchors {
			top: column.bottom
			topMargin: 10
			bottom: parent.bottom
		}
		spacing: 10
		clip: true
		model: sixAxisMonitor.addresses
		delegate: Column {
			anchors.horizontalCenter: parent.horizontalCenter
			width: childrenRect.width
			spacing: 4
			Label {
				text: qsTr("%1. Bluetooth address: %2")
							.arg(index+1)
							.arg(modelData)
			}
			ButtonRow {
				exclusive: false
				Button {
					text: qsTr("Identify")
					onClicked: sixAxisMonitor.identify(index)
				}
				Button {
					text: qsTr("Disconnect")
					onClicked: sixAxisMonitor.disconnectDev(index)
				}
			}
		}
	}

	QueryDialog {
		id: errDialog
		rejectButtonText: qsTr("Close")
		titleText: qsTr("Error")
	}
}
