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

PageStackWindow {
	id: appWindow
	showStatusBar: false

	platformStyle: PageStackWindowStyle {
		id: customStyle
		background: "image://theme/meegotouch-video-background"
		backgroundFillMode: Image.Stretch
	}

	initialPage: Page {
		Timer {
			interval: 1
			repeat: false
			running: true
			triggeredOnStart: false
			onTriggered: errorDialog.open()
		}

		QueryDialog {
			id: errorDialog
			titleText: qsTr("Error")
			message: emuView.error + qsTr("\n Application is going to shutdown")
			icon: "image://theme/icon-m-messaging-smiley-sad"
			rejectButtonText: qsTr("Close")
			onRejected: Qt.quit()
		}
	}

	Component.onCompleted: theme.inverted = true
}
