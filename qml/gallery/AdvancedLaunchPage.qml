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
	// subclasses must implement confString() function which returns -conf parameter to the launch

	property int diskIndex
	default property alias columnContent: column.children

	id: advancedRunPage

	Connections {
		target: diskGallery
		onMassStorageInUseChanged: {
			if (diskGallery.massStorageInUse) {
				appWindow.pageStack.pop()
				appWindow.pageStack.pop()
			}
		}
	}

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

				Button {
					text: qsTr("Run")
					anchors.horizontalCenter: parent.horizontalCenter
					onClicked: {
						var confStr = advancedRunPage.confString()
						var autoSaveLoad = autoSaveLoadEnabled.checked ? 1 : -1
						diskGallery.advancedLaunch(diskIndex, autoSaveLoad, confStr)
					}
				}
				EMSwitchOption {
					id: autoSaveLoadEnabled
					checked: diskGallery.globalOption("autoSaveLoadEnable")
					text: qsTr("Auto Save/Load")
				}
			}
		}
}

