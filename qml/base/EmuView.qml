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
    property int _baseUnitButtonWidth: 96
    property int _baseControlBoxWidth: 256

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

                onFpsUpdated: fpsText.text = "FPS: " + fps

                IconButton {
                    icon.source: "../../data/buttons/settings.png"

                    scale: frameItem.scalingFactor

                    anchors {left: parent.left; top: parent.top}

                    onClicked: {
                        console.log("Opening settings page...")
                        emuView.pause()
                        pageStack.push(settingsPage)
                    }
                }

                IconButton {
                    icon.source: "../../data/buttons/close.png"

                    scale: frameItem.scalingFactor

                    anchors {right: parent.right; top: parent.top}

                    onClicked: {
                        console.log("Closing...")
                        console.log(EmuPad.Button_Start)
                    }
                }

                Item {
                    anchors {left: parent.left; bottom: parent.bottom}

                    height: this.width
                    width: _baseControlBoxWidth * frameItem.scalingFactor

                    opacity: 0.6

                    MultiTouchIconButton {
                        source: "../../data/buttons/up.png"

                        anchors {top: parent.top; horizontalCenter: parent.horizontalCenter}
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_Up)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_Up)
                    }

                    MultiTouchIconButton {
                        source: "../../data/buttons/right.png"

                        anchors {right: parent.right; verticalCenter: parent.verticalCenter}
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_Right)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_Right)
                    }

                    MultiTouchIconButton {
                        source: "../../data/buttons/down.png"

                        anchors {bottom: parent.bottom; horizontalCenter: parent.horizontalCenter}
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_Down)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_Down)
                    }

                    MultiTouchIconButton {
                        source: "../../data/buttons/left.png"

                        anchors {left: parent.left; verticalCenter: parent.verticalCenter}
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_Left)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_Left)
                    }
                }

                Item {
                    anchors {right: parent.right; bottom: parent.bottom}

                    height: this.width
                    width: _baseControlBoxWidth * frameItem.scalingFactor

                    opacity: 0.5

                    MultiTouchColoredButton {
                        anchors {top: parent.top; horizontalCenter: parent.horizontalCenter}
                        color: "blue"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "X"

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_X)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_X)
                    }

                    MultiTouchColoredButton {
                        anchors {right: parent.right; verticalCenter: parent.verticalCenter}
                        color: "red"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "A"

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_A)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_A)
                    }

                    MultiTouchColoredButton {
                        anchors {bottom: parent.bottom; horizontalCenter: parent.horizontalCenter}
                        color: "yellow"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "B"

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_B)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_B)
                    }

                    MultiTouchColoredButton {
                        anchors {left: parent.left; verticalCenter: parent.verticalCenter}
                        color: "green"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "Y"

                        onPressed: emuView.addButtonPress(0, EmuPad.Button_Y)
                        onReleased: emuView.removeButtonPress(0, EmuPad.Button_Y)
                    }
                }

                Item {
                    anchors {bottom: parent.bottom; horizontalCenter: parent.horizontalCenter}

                    height: this.width/2
                    width: _baseControlBoxWidth * 0.75 * frameItem.scalingFactor

                    IconButton {
                        icon.source: "../../data/buttons/select.png"
                        scale: frameItem.scalingFactor
                        anchors {left: parent.left; bottom: parent.bottom}
                    }
                    IconButton {
                        icon.source: "../../data/buttons/start.png"
                        scale: frameItem.scalingFactor
                        anchors {right: parent.right; bottom: parent.bottom}
                    }
                }
            }

            Text {
                id: fpsText
                anchors {top: parent.top; horizontalCenter: parent.horizontalCenter}
                font.pointSize: _baseUnitButtonWidth / 2
                color: "red"
                text: "FPS:"
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

