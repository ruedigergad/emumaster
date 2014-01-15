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

AdvancedLaunchPage {
	SelectionItem {
		titleText: qsTr("Flash Device")
		subtitleText: flashDeviceModel.get(flashDeviceDialog.selectedIndex).name
		onClicked: flashDeviceDialog.open()
	}

	children: [
		SelectionDialog {
			id: flashDeviceDialog
			model: ListModel {
				id: flashDeviceModel
				ListElement { name: QT_TR_NOOP("Auto");				code: 0xffff }
				ListElement { name: QT_TR_NOOP("MACRONIX 64KB");	code: 0xc21c }
				ListElement { name: QT_TR_NOOP("MACRONIX 128KB");	code: 0xc209 }
				ListElement { name: QT_TR_NOOP("ATMEL 64KB");		code: 0x1f3d }
				ListElement { name: QT_TR_NOOP("SST 64KB");			code: 0xbfd4 }
				ListElement { name: QT_TR_NOOP("PANASONIC 64KB");	code: 0x321b }
			}
			titleText: qsTr("Select Flash Device")
			selectedIndex: 0
		}
	]

	function confString() {
		var str = ""
		if (flashDeviceDialog.selectedIndex > 0) {
			var index = flashDeviceDialog.selectedIndex
			str += "gbaMem.flashDevice=" + flashDeviceModel.get(index).code
		}
		// str += ","
		return str
	}
}
