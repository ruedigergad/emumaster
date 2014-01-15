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
import Qt.labs.folderlistmodel 1.0
import "../base"

Page {
	id: coverSelector

	property int diskIndex
	property alias folder: folderModel.folder
	property string selectedPath

	tools: ToolBarLayout {
		ToolIcon {
			iconId: "toolbar-back"
			onClicked: appWindow.pageStack.pop()
		}
	}

	Label {
		id: helpLabel
		anchors.centerIn: parent
		width: parent.width
		text: qsTr("Copy your covers (.jpg files) to\n\"emumaster/covers\"")
		horizontalAlignment: Text.AlignHCenter
	}

	FolderListModel {
		id: folderModel
		folder: "file:/home/nemo/emumaster/covers"
		nameFilters: ["*.jpg"]
		showDirs: false
	}

	ListView {
		id: coverViewPortrait
		anchors.fill: parent
		visible: appWindow.inPortrait
		model: folderModel
		spacing: 10

		delegate: ImageListViewDelegate {
			width: 480
			height: 280
			property string filePath: coverSelector.folder + "/" + fileName
			imgSource: filePath
			onClicked: coverSelector.coverSelected(filePath)
		}
	}
	ScrollDecorator {
		flickableItem: coverViewPortrait
		__minIndicatorSize: 80
	}
	GridView {
		id: coverViewLandscape
		anchors.fill: parent
		visible: !appWindow.inPortrait
		cellWidth: 284
		cellHeight: 240

		delegate: ImageListViewDelegate {
			width: 270
			height: 220
			property string filePath: coverSelector.folder + "/" + fileName
			imgSource: filePath
			onClicked: coverSelector.coverSelected(filePath)
		}
	}
	ScrollDecorator {
		flickableItem: coverViewLandscape
		__minIndicatorSize: 80
	}

	Timer {
		interval: 50; repeat: false; running: true
		onTriggered: {
			coverViewLandscape.model = folderModel
			helpLabel.visible = (folderModel.count <= 0)
		}
	}

	function coverSelected(filePath) {
		diskListModel.setDiskCover(coverSelector.diskIndex,
								   filePath)
		appWindow.pageStack.pop()
	}
}
