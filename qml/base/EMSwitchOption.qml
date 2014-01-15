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

Item {
	id: switchItem
	width: parent.width

	property alias text: switchLabel.text
	property alias checked: enableSwitch.checked

	Label {
		id: switchLabel
		anchors.verticalCenter: parent.verticalCenter
	}
	Switch {
		id: enableSwitch
		anchors.right: parent.right
	}
	Component.onCompleted: switchItem.height = Math.max(switchLabel.height, enableSwitch.height)
}
