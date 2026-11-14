import QtQuick
import QtQuick.Window

// Test screen for step 1: display, rotation, touch.
// Layout is drawn at the native panel resolution (720×1280) and scaled to the window,
// so the desktop preview on the Mac matches the device.
Window {
    id: window

    required property int rotation_
    required property var systemInfo

    readonly property int panelWidth: 720
    readonly property int panelHeight: 1280
    readonly property bool landscape: rotation_ === 90 || rotation_ === 270

    width: 405
    height: 720
    visible: true
    color: "black"
    title: "Smart Terminal 3D-Print"

    Item {
        id: canvas

        width: window.landscape ? window.panelHeight : window.panelWidth
        height: window.landscape ? window.panelWidth : window.panelHeight
        anchors.centerIn: parent
        rotation: window.rotation_
        scale: Math.min(window.width / window.panelWidth, window.height / window.panelHeight)

        Rectangle {
            anchors.fill: parent
            color: "#101418"
        }

        Column {
            id: header
            anchors {
                left: parent.left
                right: window.landscape ? touchArea.left : parent.right
                top: parent.top
                margins: 32
            }
            spacing: 8

            Text {
                text: "Smart Terminal 3D-Print"
                color: "#f1f5f9"
                font.pixelSize: 44
                font.bold: true
            }
            Text {
                text: "Testscreen Schritt 1"
                color: "#94a3b8"
                font.pixelSize: 28
            }
            Item { width: 1; height: 16 }
            Text {
                text: "Host: " + window.systemInfo.hostname
                color: "#e2e8f0"
                font.pixelSize: 28
            }
            Text {
                text: "IP: " + window.systemInfo.addresses
                color: "#e2e8f0"
                font.pixelSize: 28
                width: parent.width
                wrapMode: Text.WrapAnywhere
            }
            Text {
                text: "Version: " + window.systemInfo.version
                color: "#e2e8f0"
                font.pixelSize: 28
            }
            Text {
                text: "Drehung: " + window.rotation_ + "°  ·  Fläche " + canvas.width + "×" + canvas.height
                color: "#e2e8f0"
                font.pixelSize: 28
            }
        }

        Grid {
            id: buttons
            anchors {
                left: parent.left
                right: window.landscape ? touchArea.left : parent.right
                top: header.bottom
                margins: 32
                topMargin: window.landscape ? 32 : 48
            }
            columns: window.landscape ? 4 : 2
            spacing: 24

            Repeater {
                model: 8

                Rectangle {
                    id: button

                    property int taps: 0

                    width: (buttons.width - (buttons.columns - 1) * buttons.spacing) / buttons.columns
                    height: window.landscape ? 120 : 140
                    radius: 20
                    color: tap.pressed ? "#16a34a" : "#1e293b"
                    border.color: "#334155"
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "Taste " + (index + 1) + "\n" + button.taps
                        horizontalAlignment: Text.AlignHCenter
                        color: "#f1f5f9"
                        font.pixelSize: 32
                    }

                    TapHandler {
                        id: tap
                        onTapped: button.taps++
                    }
                }
            }
        }

        // Multi-touch: one circle per finger.
        Rectangle {
            id: touchArea
            // Portrait: below the buttons. Landscape: right column.
            anchors {
                left: window.landscape ? undefined : parent.left
                right: parent.right
                top: window.landscape ? parent.top : buttons.bottom
                bottom: parent.bottom
                margins: 32
            }
            width: window.landscape ? 360 : undefined
            radius: 20
            color: "#0b1220"
            border.color: "#334155"
            border.width: 2

            Text {
                anchors.centerIn: parent
                visible: points.touchPoints.length === 0
                text: "Mehrfinger-Test"
                color: "#64748b"
                font.pixelSize: 32
            }

            MultiPointTouchArea {
                id: points
                anchors.fill: parent
                mouseEnabled: true
                maximumTouchPoints: 10
            }

            Repeater {
                model: points.touchPoints

                Rectangle {
                    required property var modelData
                    x: modelData.x - width / 2
                    y: modelData.y - height / 2
                    width: 110
                    height: 110
                    radius: 55
                    color: "transparent"
                    border.color: "#38bdf8"
                    border.width: 6
                }
            }
        }
    }
}
