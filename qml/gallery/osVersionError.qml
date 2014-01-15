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
import com.nokia.meego 1.0

PageStackWindow {
	showStatusBar: false
	showToolBar: false

	platformStyle: PageStackWindowStyle {
		id: customStyle;
		background: "image://theme/meegotouch-video-background"
		backgroundFillMode: Image.Stretch
		cornersVisible: false
	}
	initialPage: Page {

		Label {
			anchors.fill: parent
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
			text: qsTr("N9 v1.2 firmware is required to use this software")
			font.pixelSize: 64
		}

		Button {
			anchors.horizontalCenter: parent
			anchors.bottom: parent.bottom
			anchors.bottomMargin: 16
			text: qsTr("Close")
			onClicked: Qt.quit()
		}
	}
}
