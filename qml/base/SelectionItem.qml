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
import Sailfish.Silica 1.0
import "constants.js" as UI

Item {
	id: selectionItem

	signal clicked
	property alias pressed: mouseArea.pressed

	property int titleSize: UI.LIST_TILE_SIZE
	property int titleWeight: Font.Bold
	property string titleFont: UI.FONT_FAMILY
	property color titleColor: theme.inverted ? UI.LIST_TITLE_COLOR_INVERTED : UI.LIST_TITLE_COLOR
	property color titleColorPressed: theme.inverted ? UI.LIST_TITLE_COLOR_PRESSED_INVERTED : UI.LIST_TITLE_COLOR_PRESSED

	property int subtitleSize: UI.LIST_SUBTILE_SIZE
	property int subtitleWeight: Font.Normal
	property string subtitleFont: UI.FONT_FAMILY_LIGHT
	property color subtitleColor: theme.inverted ? UI.LIST_SUBTITLE_COLOR_INVERTED : UI.LIST_SUBTITLE_COLOR
	property color subtitleColorPressed: theme.inverted ? UI.LIST_SUBTITLE_COLOR_PRESSED_INVERTED : UI.LIST_SUBTITLE_COLOR_PRESSED

	property alias titleText: mainText.text
	property alias subtitleText: subText.text

	property alias iconSource: icon.source
	property alias imageSource: image.source

	property bool selected: false

	height: UI.LIST_ITEM_HEIGHT
	width: parent.width
/*
	BorderImage {
		id: background
		anchors.fill: parent
		// Fill page porders
		anchors.leftMargin: -UI.MARGIN_XLARGE
		anchors.rightMargin: -UI.MARGIN_XLARGE
		visible: mouseArea.pressed || selectionItem.selected
		source: theme.inverted ? "image://theme/meegotouch-panel-inverted-background-pressed" : "image://theme/meegotouch-panel-background-pressed"
	}
*/

	Row {
		anchors.fill: parent
		spacing: UI.LIST_ITEM_SPACING

		Image {
			id: image
			anchors.verticalCenter: parent.verticalCenter
			visible: selectionItem.imageSource ? true : false
		}

		Column {
			anchors.verticalCenter: parent.verticalCenter

			Label {
				id: mainText
				font.family: selectionItem.titleFont
				font.weight: selectionItem.titleWeight
				font.pixelSize: selectionItem.titleSize
				color: mouseArea.pressed ? selectionItem.titleColorPressed : selectionItem.titleColor
			}

			Label {
				id: subText
				font.family: selectionItem.subtitleFont
				font.weight: selectionItem.subtitleWeight
				font.pixelSize: selectionItem.subtitleSize
				color: mouseArea.pressed ? selectionItem.subtitleColorPressed : selectionItem.subtitleColor
			}
		}
	}
	MouseArea {
		id: mouseArea;
		anchors.fill: parent
		onClicked: {
			selectionItem.clicked();
		}
	}

	Image {
		id: icon
		source: "image://theme/icon-m-textinput-combobox-arrow"
		anchors {
			right: parent.right;
			rightMargin: 8;
			verticalCenter: parent.verticalCenter;
		}
	}
}
