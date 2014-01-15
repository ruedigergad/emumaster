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

ApplicationWindow {
	id: appWindow
	initialPage: mainPage

	CollectionMenuPage { id: mainPage }
	GalleryPage { id: galleryPage }

	function selectCollection(name) {
		diskListModel.collection = name
		if (diskListModel.count <= 0) {
			if (name == "fav")
				showHowToAddToFavDialog()
			else
				showHowToAddDialog()
		} else {
			appWindow.pageStack.push(galleryPage)
		}
		galleryPage.updateModel()
	}

	function showHowToAddToFavDialog() {
		errorDialog.message = qsTr("The favourite category is empty. To add a disk to favourite list " +
								   "just select the disk from one of emulated systems (press and hold), " +
								   "a menu will appear, choose \"Add To Favourites\"")
		errorDialog.open()
	}

	function showHowToAddDialog() {
		errorDialog.message = qsTr("The directory is empty. To install a disk you need to attach " +
								   "the phone to the PC and copy your files to \"emumaster/%1\" " +
								   "directory. Remember to detach the phone from the PC.")
									 .arg(diskListModel.collection)
		errorDialog.open()
	}

/*
	QueryDialog {
		property bool quitOnReject: false

		id: errorDialog
		titleText: qsTr("Oops")
		message: qsTr("Something went wrong!")
		rejectButtonText: qsTr("Close")

		onRejected: {
			if (quitOnReject)
				Qt.quit()
		}
	}
*/

	Component.onCompleted: {
		theme.inverted = true
	}

	property Item keysForwardedTo: mainPage

	Connections {
		target: pageStack
		onCurrentPageChanged: keysForwardedTo = pageStack.currentPage
	}
	Keys.forwardTo: keysForwardedTo
}
