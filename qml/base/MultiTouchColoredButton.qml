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

Rectangle {
    id: button

    radius: this.width / 2
    antialiasing: false

    property alias text: buttonText.text

    signal pressed()
    signal released()

    Text {
        id: buttonText
        anchors.centerIn: parent
        font.pointSize: parent.width / 2
        font.bold: true
    }

    MultiPointTouchArea {
        id: buttonArea
        anchors.fill: parent

        minimumTouchPoints: 1
        maximumTouchPoints: 1

        onPressed: button.pressed()
        onReleased: button.released()
        onCanceled: button.released()
        onGestureStarted: button.released()
    }
}
