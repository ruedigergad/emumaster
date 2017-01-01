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
    property int _baseUnitButtonWidth: 64
    property int _baseControlBoxWidth: _baseUnitButtonWidth * 4

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

                    IconButton {
                        icon.source: "../../data/buttons/up.png"
                        scale: frameItem.scalingFactor
                        anchors {top: parent.top; horizontalCenter: parent.horizontalCenter}

                        onDownChanged: {
                            if (down) {
                                emuView.addButtonPress(0, EmuPad.Button_Up)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_Up)
                            }
                        }
                    }

                    IconButton {
                        icon.source: "../../data/buttons/right.png"
                        scale: frameItem.scalingFactor
                        anchors {right: parent.right; verticalCenter: parent.verticalCenter}

                        onDownChanged: {
                            if (down) {
                                emuView.addButtonPress(0, EmuPad.Button_Right)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_Right)
                            }
                        }
                    }

                    IconButton {
                        icon.source: "../../data/buttons/down.png"
                        scale: frameItem.scalingFactor
                        anchors {bottom: parent.bottom; horizontalCenter: parent.horizontalCenter}

                        onDownChanged: {
                            if (down) {
                                emuView.addButtonPress(0, EmuPad.Button_Down)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_Down)
                            }
                        }
                    }

                    IconButton {
                        icon.source: "../../data/buttons/left.png"
                        scale: frameItem.scalingFactor
                        anchors {left: parent.left; verticalCenter: parent.verticalCenter}

                        onDownChanged: {
                            if (down) {
                                emuView.addButtonPress(0, EmuPad.Button_Left)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_Left)
                            }
                        }
                    }
                }

                Item {
                    anchors {right: parent.right; bottom: parent.bottom}

                    height: this.width
                    width: _baseControlBoxWidth * frameItem.scalingFactor

                    opacity: 0.5

                    ColoredButton {
                        anchors {top: parent.top; horizontalCenter: parent.horizontalCenter}
                        color: "blue"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "X"

                        onPressedChanged: {
                            if (pressed) {
                                emuView.addButtonPress(0, EmuPad.Button_X)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_X)
                            }
                        }
                    }

                    ColoredButton {
                        anchors {right: parent.right; verticalCenter: parent.verticalCenter}
                        color: "red"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "A"

                        onPressedChanged: {
                            if (pressed) {
                                emuView.addButtonPress(0, EmuPad.Button_A)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_A)
                            }
                        }
                    }

                    ColoredButton {
                        anchors {bottom: parent.bottom; horizontalCenter: parent.horizontalCenter}
                        color: "yellow"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "B"

                        onPressedChanged: {
                            if (pressed) {
                                emuView.addButtonPress(0, EmuPad.Button_B)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_B)
                            }
                        }
                    }

                    ColoredButton {
                        anchors {left: parent.left; verticalCenter: parent.verticalCenter}
                        color: "green"
                        height: this.width
                        width: _baseUnitButtonWidth * frameItem.scalingFactor
                        text: "Y"

                        onPressedChanged: {
                            if (pressed) {
                                emuView.addButtonPress(0, EmuPad.Button_Y)
                            } else {
                                emuView.removeButtonPress(0, EmuPad.Button_Y)
                            }
                        }
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

