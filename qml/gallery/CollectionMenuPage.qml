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

Page {

	SilicaFlickable {
		id: flickable
		anchors.fill: parent
		contentWidth: width
		contentHeight: collectionGrid.height

		Grid {
			id: collectionGrid
			rows: 4
			columns: 2
			anchors.horizontalCenter: parent.horizontalCenter

			CollectionTypeButton { name: "fav" }
			CollectionTypeButton { name: "nes" }
			CollectionTypeButton { name: "snes" }
			CollectionTypeButton { name: "gba" }
			CollectionTypeButton { name: "amiga" }
			CollectionTypeButton { name: "pico" }
		}

		PullDownMenu {
			id: mainMenu

			MenuItem {
				text: qsTr("Global Settings")
				onClicked: appWindow.pageStack.push(Qt.resolvedUrl("GlobalSettings.qml"))
			}
//			MenuItem {
//				text: qsTr("SixAxis Monitor")
//				onClicked: diskGallery.sixAxisMonitor()
//			}
			MenuItem {
				text: qsTr("About EmuMaster ...")
				onClicked: appWindow.pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
			}
		}

	}

}
