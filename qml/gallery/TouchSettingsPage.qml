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
import EmuMaster 1.0

Page {
	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: appWindow.pageStack.pop()
		}
	}

	Label {
		id: titleLabel
		text: qsTr("Touch Screen Configuration")
		anchors.horizontalCenter: parent.horizontalCenter
	}
	Label {
		id: titleLabel2
		anchors.top: titleLabel.bottom
		text: qsTr("Tip: Show grid to view diagonal area")
		anchors.horizontalCenter: parent.horizontalCenter
	}

	Flickable {
		id: flickable
		anchors {
			top: titleLabel2.bottom
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
			spacing: 20

			Item {
				width: parent.width
				height: 480

				TouchInputView {
					id: touchInputView
					anchors.fill: parent
					opacity: opacitySlider.value
					anchors.horizontalCenter: parent.horizontalCenter
					touchDevice: touchInputDevice
				}
				Label {
					width: parent.width
					text: qsTr("Pad is not visible")
					font.pixelSize: 100
					anchors.centerIn: parent
					visible: touchInputView.opacity == 0 || !(touchInputDevice.buttonsVisible || touchInputDevice.gridVisible)
				}
			}

			Label { text: qsTr("Pad Opacity") }
			Slider {
				id: opacitySlider
				width: parent.width
				minimumValue: 0.0
				maximumValue: 1.0
				stepSize: 0.05
				valueIndicatorVisible: true
				onValueChanged: diskGallery.setGlobalOption("padOpacity", value)
				Component.onCompleted: value = diskGallery.globalOption("padOpacity")
			}

			Label { text: qsTr("D-Pad Size") }
			Slider {
				id: dpadSizeSlider
				width: parent.width
				stepSize: 2
				valueIndicatorVisible: true
				onValueChanged: {
					diskGallery.setGlobalOption("touchAreaSize", value)
					touchInputDevice.dpadAreaSize = value
				}
				Component.onCompleted: {
					var initial = diskGallery.globalOption("touchAreaSize")
					touchInputDevice.dpadAreaSize = initial
					minimumValue = 160
					maximumValue = 320
					value = initial
				}
			}

			Label { text: qsTr("D-Pad Diagonal Size") }
			Slider {
				width: parent.width
				maximumValue: dpadSizeSlider.value/2
				stepSize: 1
				valueIndicatorVisible: true
				onValueChanged: {
					touchInputDevice.dpadAreaDiagonalSize = value
					diskGallery.setGlobalOption("touchAreaDiagonalSize", value)
				}
				Component.onCompleted: {
					value = diskGallery.globalOption("touchAreaDiagonalSize")
					touchInputDevice.dpadAreaDiagonalSize = value
				}
			}

			GlobalSettingsSwitchItem { text: qsTr("Haptic Feedback Enabled"); optionName: "hapticFeedbackEnable" }

			GlobalSettingsSwitchItem {
				text: qsTr("Show Grid")
				optionName: "gridVisible"
				onCheckedChanged: touchInputDevice.gridVisible = checked
				Component.onCompleted: touchInputDevice[optionName] = diskGallery.globalOption(optionName)
			}
			Item {
				width: parent.width
				Label {
					id: gridColorLabel
					text: qsTr("Grid Color")
					anchors.verticalCenter: parent.verticalCenter
				}
				Button {
					id: gridColorButton
					width: 80
					anchors.right: parent.right
					onClicked: colorDialog.open()
					anchors.verticalCenter: parent.verticalCenter

					Rectangle {
						id: gridColorView
						width: 40
						height: 40
						anchors.centerIn: parent

						function refresh() {
							var color = diskGallery.globalOption("gridColor")
							touchInputDevice.gridColor = color
							gridColorView.color = color
						}
						Component.onCompleted: refresh()
					}
				}
				Component.onCompleted: height = Math.max(gridColorLabel.height, gridColorButton.height)
			}

			GlobalSettingsSwitchItem {
				text: qsTr("Show Buttons")
				optionName: "buttonsVisible"
				onCheckedChanged: touchInputDevice.buttonsVisible = checked
				Component.onCompleted: touchInputDevice[optionName] = diskGallery.globalOption(optionName)
			}
			GlobalSettingsSwitchItem {
				text: qsTr("Show L/R Buttons")
				optionName: "lrButtonsVisible"
				onCheckedChanged: touchInputDevice.setLRVisible(checked)
				Component.onCompleted: touchInputDevice.setLRVisible(diskGallery.globalOption(optionName))
			}
		}
	}

	ScrollDecorator { flickableItem: flickable }

	ColorDialog {
		id: colorDialog
		onAccepted: {
			diskGallery.setGlobalOption("gridColor", colorValue)
			gridColorView.refresh()
		}
	}
}
