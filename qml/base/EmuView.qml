import QtQuick 2.0
import Sailfish.Silica 1.0
import EmuMaster 1.0

ApplicationWindow
{
    initialPage: Page {
        allowedOrientations: Orientation.Landscape

        Rectangle {
            anchors.fill: parent
            color: "green"

            FrameItem {
                id: frameItem

                anchors.fill: parent

		IconButton {
			icon.source: "../../data/settings_button.png"
			x: 0
			y: 0
			onClicked: {
				emuView.pause()
				pageStack.push(settingsPage)
			}
		}
            }
        }
    }

    Component.onCompleted: {
        frameItem.setEmuView(emuView)
    }

    SettingsPage {
        id: settingsPage
    }
}

