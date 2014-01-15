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

Menu {
	id: galleryMenu
	property bool addRemoveIconToggle: false
	property bool addRemoveFavToggle: false
	property int diskIndex

	function prepareAndOpen(index) {
		galleryMenu.diskIndex = index
		galleryMenu.addRemoveIconToggle = diskListModel.iconInHomeScreenExists(galleryMenu.diskIndex)
		galleryMenu.addRemoveFavToggle = diskListModel.diskInFavExists(galleryMenu.diskIndex)
		galleryMenu.open()
	}

	MenuLayout {
		MenuItem {
			text: qsTr("Advanced Launch")
			onClicked: {
				var emuName = diskListModel.getDiskEmuName(diskIndex)
				var qmlPage = emuName.charAt(0).toUpperCase() +
						emuName.substr(1) + "AdvancedLaunchPage.qml"
				appWindow.pageStack.push(Qt.resolvedUrl(qmlPage),
										 { diskIndex: galleryMenu.diskIndex })
			}
		}
		MenuItem {
			text: qsTr("Select Cover")
			onClicked: appWindow.pageStack.push(Qt.resolvedUrl("CoverSelectorPage.qml"),
												{ diskIndex: galleryMenu.diskIndex })
		}
		MenuItem {
			text: qsTr("Add to Favourites")
			visible: !galleryMenu.addRemoveFavToggle
			onClicked: diskListModel.addToFav(galleryMenu.diskIndex)
		}
		MenuItem {
			text: qsTr("Remove from Favourites")
			visible: galleryMenu.addRemoveFavToggle
			onClicked: diskListModel.removeFromFav(galleryMenu.diskIndex)
		}
		MenuItem {
			text: qsTr("Create Icon in Home Screen")
			onClicked: galleryPage.homeScreenIcon(galleryMenu.diskIndex)
			visible: !galleryMenu.addRemoveIconToggle
		}
		MenuItem {
			text: qsTr("Remove Icon from Home Screen")
			onClicked: diskListModel.removeIconFromHomeScreen(galleryMenu.diskIndex)
			visible: galleryMenu.addRemoveIconToggle
		}
		MenuItem {
			text: qsTr("Delete")
			onClicked: removeDiskDialog.prepareAndOpen(galleryMenu.diskIndex)
		}
	}
}
