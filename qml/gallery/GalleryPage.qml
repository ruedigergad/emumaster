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
	id: galleryPage

	Connections {
		target: diskGallery
		onMassStorageInUseChanged: {
			if (diskGallery.massStorageInUse)
				appWindow.pageStack.pop()
		}
	}

//	GalleryMenu { id: galleryMenu }

	ListModel { id: nullModel }

	function updateModel() {
		diskViewPortrait.model = nullModel
		diskViewPortrait.model = diskListModel
		diskViewLandscape.model = nullModel
		diskViewLandscape.model = diskListModel
	}

	function diskClickHandle(index) {
//		if (searchBar.visible)
//			searchBar.visible = false
//		else
			diskGallery.launch(index)
	}

	SilicaListView {
		id: diskViewPortrait
		anchors.fill: parent
		spacing: 10
		visible: false

		delegate: ImageListViewDelegate {
			width: 480
			height: 280
			text: title
			imgSource: imageSource
			onClicked: galleryPage.diskClickHandle(index)
			onPressAndHold: galleryMenu.prepareAndOpen(index)
		}
		section.property: "alphabet"
		section.criteria: ViewSection.FullString
		section.delegate: SectionSeperator { text: section; rightPad: 80 }

		function customSectionScrollerDataHandler() {
			var sectionData = []
			var _sections = []
			var current = "", item;
			for (var i = 0, count = model.count; i < count; i++) {
				item = model.getAlphabet(i);
				if (item !== current) {
					current = item;
					_sections.push(current);
					sectionData.push({ index: i, header: current });
				}
			}
			return { sectionData: sectionData, _sections: _sections }
		}
	}

	SilicaGridView {
		id: diskViewLandscape
		anchors.fill: parent
		visible: true
		cellWidth: 284
		cellHeight: 240

		delegate: ImageListViewDelegate {
			width: 270
			height: 220
			text: titleElided
			imgSource: imageSource
			onClicked: galleryPage.diskClickHandle(index)
			onPressAndHold: galleryMenu.prepareAndOpen(index)
		}
	}

/*
	ToolBar {
		id: searchBar
		anchors.top: parent.top
		visible: false

		tools: ToolBarLayout {
			TextField {
				id: searchField
				anchors.verticalCenter: parent.verticalCenter
				placeholderText: qsTr("Search")
				inputMethodHints: Qt.ImhNoPredictiveText|Qt.ImhNoAutoUppercase

				onTextChanged: {
					if (visible)
						diskListModel.setNameFilter(searchField.text)
				}
				Image {
					source: "image://theme/icon-s-cancel"
					anchors {
						right: parent.right; rightMargin: 10
						verticalCenter: parent.verticalCenter
					}
				}
				MouseArea {
					width: 80
					height: parent.height
					anchors.right: parent.right
					onClicked: searchBar.visible = false
				}
			}
		}

		function hideAndClear() {
			searchBar.visible = false
			searchField.text = ""
		}
	}

	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: {
				searchBar.hideAndClear()
				appWindow.pageStack.pop()
			}
		}

		ToolIcon {
			iconSource: "image://theme/icon-s-description-inverse"
			onClicked: helpDialog.open()
		}

		ToolIcon {
			iconId: "icon-m-common-search-inverse"
			onClicked: {
				searchBar.visible = !searchBar.visible
				searchField.focus = visible
			}
		}
	}
	QueryDialog {
		id: removeDiskDialog
		acceptButtonText: qsTr("Yes")
		rejectButtonText: qsTr("No")
		titleText: qsTr("Delete")
		onAccepted: diskListModel.trash(diskIndex)

		property int diskIndex

		function prepareAndOpen(index) {
			removeDiskDialog.diskIndex = index
			removeDiskDialog.message = qsTr("Do you really want to delete\n\"%1\" ?")
											.arg(diskListModel.getDiskTitle(index))
			removeDiskDialog.open()
		}
	}

	QueryDialog {
		id: helpDialog
		rejectButtonText: qsTr("Close")
		titleText: qsTr("Tip")
		message: qsTr("Press and hold a disk to show the menu")
	}
*/

	function homeScreenIcon(diskIndexArg) {
		if (diskListModel.getScreenShotUpdate(diskIndexArg) < 0) {
			errorDialog.message = qsTr("You need to make a screenshot first!")
			errorDialog.open()
		} else {
			var path = qsTr("image://disk/%1/%2*%3")
							.arg(diskListModel.getDiskEmuName(diskIndexArg))
							.arg(diskListModel.getDiskTitle(diskIndexArg))
							.arg(diskListModel.getScreenShotUpdate(diskIndexArg))
			appWindow.pageStack.push(Qt.resolvedUrl("HomeScreenIconSheet.qml"),
									 { imgSource: path, diskIndex: diskIndexArg })
		}
	}
}
