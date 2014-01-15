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
import QtDesktop 0.1

Item {
	width: 640
	height: 480

	Row {
		id: toolbar
		height: childrenRect.height

		ButtonRow {
			height: childrenRect.height
			Button {
				text: "Continue"
				onClicked: dbg.continueRun()
			}

			Button {
				text: "Step In"
				onClicked: dbg.stepInto()
			}

			Button {
				text: "Step Over"
				onClicked: dbg.stepOver()
			}
		}

		CheckBox {
			text: "Log Enabled"
			onCheckedChanged: dbg.logEnable = checked
			Component.onCompleted: checked = dbg.logEnable
		}
		CheckBox {
			text: "Break on NMI"
			onCheckedChanged: dbg.breakOnNmiEnable = checked
			Component.onCompleted: checked = dbg.breakOnNmiEnable
		}

		Button {
			text: "GoTo"
			onClicked:  {
				var pos = dbg.indexOf(addrEdit.text)
				if (pos >= 0)
					mainView.positionViewAtIndex(pos,ListView.Center)
			}
		}
		TextField {
			id: addrEdit
			width: 80
			text: "0x"
		}
	}

	ListView {
		id: mainView
		anchors {
			left: parent.left
			right: parent.right
			top: toolbar.bottom
			bottom: parent.bottom
		}
		clip: true
		model: dbg

		delegate: Rectangle {
			width: parent.width
			height: 20
			color: (index & 1) ? "#f0f0f0" : "#f8f8f8"

			Image {
				x: 2
				anchors.verticalCenter: parent.verticalCenter
				visible: bkpt
				source: "brightness.png"
			}

			Image {
				x: 22
				anchors.verticalCenter: parent.verticalCenter
				visible: index === dbg.currentIndexOfPc
				source: "arrow-skip.png"
			}

			Label {
				x: 40
				id: addrLabel
				width: 50
				text: pc
				font.family: "Courier New"
				font.pixelSize: 16
			}
			Label {
				x: 150
				id: instrLabel
				width: 300
				text: disasm
				font.family: "Courier New"
				font.pixelSize: 16
				font.bold: true
			}
			Label {
				anchors.left: instrLabel.right
				text: cmnt ? "// " + cmnt : ""
				font.family: "Courier New"
				font.pixelSize: 16
				color: "#008000"
			}

			MouseArea {
				width: 20
				height: parent.height
				onClicked: dbg.toggleBreakpoint(index)
			}
		}

		Rectangle {
			x: 20
			width: 1
			height: parent.height
			color: "black"
		}
	}
	Rectangle {
		anchors.right: parent.right
		width: 300
		height: parent.height
		border.color: "black"
		border.width: 2

		Column {
			id: regProfColumn
			width: parent.width
			height: childrenRect.height

			IconTextHeader {
				text: "Registers"
			}

			Column {
				height: childrenRect.height
				Label { font.pixelSize: 16; font.family: "Courier New"; text: " PC " + dbg.regPCx }
				Label { font.pixelSize: 16; font.family: "Courier New"; text: " A  " + dbg.regAx }
				Label { font.pixelSize: 16; font.family: "Courier New"; text: " X  " + dbg.regXx }
				Label { font.pixelSize: 16; font.family: "Courier New"; text: " Y  " + dbg.regYx }
				Label { font.pixelSize: 16; font.family: "Courier New"; text: " S  " + dbg.regSx }
				Label { font.pixelSize: 16; font.family: "Courier New"; text: " P  " + dbg.regPx }
			}

			IconTextHeader {
				text: "Banks"
			}

			Label {
				id: bankLabel
				font.family: "Courier New"
				font.pixelSize: 16
				text: " " + dbg.bankString
			}

			IconTextHeader {
				text: "Profiler"
			}

			Button {
				anchors.horizontalCenter: parent.horizontalCenter
				text: "Fetch"
				onClicked: dbg.fetchProfiler()
			}
			Button {
				anchors.horizontalCenter: parent.horizontalCenter
				text: "Reset"
				onClicked: dbg.resetProfiler()
			}
		}
		ListView {
			id: profilerView
			anchors {
				top: regProfColumn.bottom
				bottom: parent.bottom
			}
			x: 3
			width: parent.width-6
			clip: true
			model: dbg.prof

			delegate: FlatButton {
				width: profilerView.width
				text: pc + ": " + cnt
				onClicked: mainView.positionViewAtIndex(dbg.indexOf("0x"+pc),
														ListView.Center)
			}
		}
	}

	Connections {
		target: dbg
		onRegPCChanged: mainView.positionViewAtIndex(dbg.currentIndexOfPc,
													 ListView.Center)
	}

	Timer {
		id: startTimer
		interval: 100
		repeat: false
		onTriggered: dbg.connectToServer()
	}
	Component.onCompleted: startTimer.start()
}
