import QtQuick 2.0
import Sailfish.Silica 1.0
import EmuMaster 1.0
import QtMultimedia 5.0

ApplicationWindow
{
    property bool startupAudioHack: true

    Timer {
        interval: 500; running: true; repeat: false
        onTriggered: emuView.pause()
    }

    initialPage: Page {
        allowedOrientations: Orientation.Landscape

        Rectangle {
            anchors.fill: parent
            color: "green"

            FrameItem {
                id: frameItem

                anchors.fill: parent

                IconButton {
                    icon.source: "../../data/buttons/settings.png"
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

    Connections {
        target: emuView
        onPauseStage2Finished: {
            if (startupAudioHack) {
                emuView.sleepMs(100)
                emuView.showEmulationView()
                emuView.resume()
                startupAudioHack = false
            }
        }
    }

    SettingsPage {
        id: settingsPage
    }

    MediaPlayer { }
}

