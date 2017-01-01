import QtQuick 2.0
import Sailfish.Silica 1.0
import EmuMaster 1.0
import QtMultimedia 5.0

/*
   Button enum entries in EmuTab:
        Button_Right	= (1 <<  0),
        Button_Down		= (1 <<  1),
        Button_Up		= (1 <<  2),
        Button_Left		= (1 <<  3),

        Button_A		= (1 <<  4),
        Button_B		= (1 <<  5),
        Button_X		= (1 <<  6),
        Button_Y		= (1 <<  7),

        Button_L1		= (1 <<  8),
        Button_R1		= (1 <<  9),
        Button_L2		= (1 << 10),
        Button_R2		= (1 << 11),

        Button_Start	= (1 << 12),
        Button_Select	= (1 << 13)
 */

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
                        console.log(EmuPad.Button_Start)
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

