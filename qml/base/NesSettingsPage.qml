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
import com.nokia.meego 1.1

SettingsPage {
	videoContent: [
		EMSwitchOption {
			text: qsTr("Disable Sprite Limit")
			onCheckedChanged: emu.ppu.spriteLimit = !checked
			Component.onCompleted: checked = !emu.ppu.spriteLimit
		}
	]

	miscContent: [
		Label { width: parent.width; text: qsTr("Disk Info: ") + emu.diskInfo }
	]

	columnContent: [
		SectionSeperator { text: qsTr("ChEaTs") },
		NesCheats {}
	]
}
