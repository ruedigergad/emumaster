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
import Sailfish.Silica 1.0
import "../base"

Page {
/*
	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: appWindow.pageStack.pop()
		}
	}
*/

	SilicaFlickable {
		id: flickable
		anchors.fill: parent
		flickableDirection: Flickable.VerticalFlick
		contentHeight: column.height

	Column {
		id: column
		width: parent.width
		height: childrenRect.height
		spacing: 20

		SectionSeperator { text: qsTr("STATE") }

		GlobalSettingsSwitchItem { text: qsTr("Auto Load/Save on Start/Exit"); optionName: "autoSaveLoadEnable" }

		SectionSeperator { text: qsTr("INPUT") }

		GlobalSettingsSwitchItem { text: qsTr("Swipe Enabled"); optionName: "swipeEnable" }

		Button {
			text: qsTr("Touch Configuration")
			anchors.horizontalCenter: parent.horizontalCenter
			onClicked: appWindow.pageStack.push(Qt.resolvedUrl("TouchSettingsPage.qml"))
		}
		Button {
			text: qsTr("Keyboard Configuration")
			anchors.horizontalCenter: parent.horizontalCenter
			onClicked: appWindow.pageStack.push(Qt.resolvedUrl("KeybMappingPage.qml"))
		}
		Button {
			text: qsTr("Calibrate Accelerometer")
			anchors.horizontalCenter: parent.horizontalCenter
			onClicked: appWindow.pageStack.push(Qt.resolvedUrl("AccelCalibrationPage.qml"))
		}

		SectionSeperator { text: qsTr("VIDEO") }

		Label { text: qsTr("Frameskip") }
		Slider {
			width: parent.width
			minimumValue: 0
			maximumValue: 5
			stepSize: 1
			onValueChanged: diskGallery.setGlobalOption("frameSkip", value)
			Component.onCompleted: value = diskGallery.globalOption("frameSkip")
		}
		GlobalSettingsSwitchItem { text: qsTr("Show FPS"); optionName: "fpsVisible" }
		GlobalSettingsSwitchItem { text: qsTr("Keep Aspect Ratio"); optionName: "keepAspectRatio" }

		SectionSeperator { text: qsTr("MISC") }

		GlobalSettingsSwitchItem { text: qsTr("Audio Enabled"); optionName: "audioEnable" }

		EMSwitchOption {
			id: runInBackgroundSwitch
			text: qsTr("Run in Background")
			checked: diskGallery.globalOption("runInBackground")
			onCheckedChanged: {
				if (checked) {
					// special handling to prevent the dialog from showing at the start
					if (diskGallery.globalOption("runInBackground") !== "true")
						runInBackgroundDialog.open()
				} else {
					diskGallery.setGlobalOption("runInBackground", false)
				}
			}
		}
	}

	}
/*
	ScrollDecorator { flickableItem: flickable }

	QueryDialog {
		id: runInBackgroundDialog
		titleText: qsTr("Enabling the option will run the application in the background. " +
						"This can consume a lot of battery!")
		acceptButtonText: qsTr("Enable")
		rejectButtonText: qsTr("Disable")
		onRejected: runInBackgroundSwitch.checked = false
		onAccepted: diskGallery.setGlobalOption("runInBackground", true)
	}
*/
}
