import QtQuick 2.0
import Sailfish.Silica 1.0
import EmuMaster 1.0
import QtMultimedia 5.0

ApplicationWindow
{
    property int _baseUnitWidth: 960
    property int _baseControlBoxWidth: _baseUnitWidth / 5

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

                property double scalingFactor: frameItem.width / _baseUnitWidth

                onFpsUpdated: console.log("FPS: " + fps)

                IconButton {
                    icon.source: "../../data/buttons/settings.png"

                    scale: frameItem.scalingFactor

                    x: 0
                    y: 0

                    onClicked: {
                        console.log("Opening settings page...")
                        emuView.pause()
                        pageStack.push(settingsPage)
                    }
                }

                IconButton {
                    icon.source: "../../data/buttons/close.png"

                    scale: frameItem.scalingFactor

                    x: frameItem.width - this.width
                    y: 0

                    onClicked: {
                        console.log("Closing...")
                    }
                }

                Rectangle {
                    color: "blue"

                    x: 0
                    y: frameItem.height - this.height

                    height: this.width
                    width: _baseControlBoxWidth * frameItem.scalingFactor
                }

                Rectangle {
                    color: "red"

                    x: frameItem.width - this.width
                    y: frameItem.height - this.height

                    height: this.width
                    width: _baseControlBoxWidth * frameItem.scalingFactor
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

