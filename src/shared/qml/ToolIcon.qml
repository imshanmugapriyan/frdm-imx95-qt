pragma ComponentBehavior: Bound

import QtQuick

// Decorative symbols made from QML geometry, not sampled diagnostic data.
Item {
    id: icon

    property string kind: "electrodogram"
    property color accent: "#be9bff"
    readonly property var barHeights: [0.18, 0.36, 0.58, 0.4, 0.76, 0.52, 0.3, 0.48, 0.2]

    Rectangle {
        anchors.fill: parent
        radius: width * 0.24
        color: Qt.rgba(icon.accent.r, icon.accent.g, icon.accent.b, 0.1)
        border.color: Qt.rgba(icon.accent.r, icon.accent.g, icon.accent.b, 0.22)
        antialiasing: true
    }

    Repeater {
        model: icon.kind === "electrodogram" ? 4 : 0
        delegate: Item {
            id: lane
            required property int index

            x: icon.width * 0.2
            y: icon.height * (0.23 + index * 0.16)
            width: icon.width * 0.6
            height: icon.height * 0.1

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: Math.max(1, icon.height * 0.018)
                color: Qt.rgba(icon.accent.r, icon.accent.g, icon.accent.b, 0.35)
            }
            Rectangle {
                x: lane.width * (0.16 + lane.index * 0.16)
                width: icon.width * 0.055
                height: lane.height
                radius: width / 2
                color: icon.accent
                antialiasing: true
            }
            Rectangle {
                x: lane.width * (0.16 + lane.index * 0.16) + icon.width * 0.07
                anchors.verticalCenter: parent.verticalCenter
                width: icon.width * 0.04
                height: lane.height * 0.5
                radius: width / 2
                color: icon.accent
                opacity: 0.65
                antialiasing: true
            }
        }
    }

    Repeater {
        model: icon.kind === "acoustic" ? icon.barHeights.length : 0
        delegate: Rectangle {
            required property int index

            x: icon.width * (0.2 + index * 0.07)
            anchors.verticalCenter: parent.verticalCenter
            width: icon.width * 0.035
            height: icon.height * icon.barHeights[index]
            radius: width / 2
            color: icon.accent
            antialiasing: true
        }
    }

    Item {
        visible: icon.kind === "lanxi"
        anchors.fill: parent
        // Two acquisition modules joined by the session's network connection.
        Rectangle {
            x: icon.width*0.48; y: icon.height*0.32
            width: icon.width*0.04; height: icon.height*0.36
            color: icon.accent
        }
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                x: icon.width*0.2; y: icon.height*(0.2+index*0.39)
                width: icon.width*0.6; height: icon.height*0.22
                radius: icon.width*0.035
                color: "#273c40"; border.color: icon.accent
                Repeater {
                    model: 4
                    delegate: Rectangle {
                        required property int index
                        x: parent.width*(0.12+index*0.21)
                        anchors.verticalCenter: parent.verticalCenter
                        width: icon.width*0.055; height: width; radius: width/2
                        color: icon.accent; antialiasing: true
                    }
                }
            }
        }
    }
    Rectangle {
        visible: icon.kind === "notepad"
        x: icon.width*0.25; y: icon.height*0.17; width: icon.width*0.5; height: icon.height*0.66
        radius: icon.width*0.04; color: "#f0e7d5"
        Repeater {
            model: 5
            delegate: Rectangle { required property int index
                x: parent.width*0.18; y: parent.height*(0.24+index*0.12)
                width: parent.width*0.65; height: Math.max(1,icon.height*0.018); color: "#a197ba" }
        }
        Rectangle { x: parent.width*0.14; width: parent.width*0.72; height: parent.height*0.09; color: "#d4b56b" }
    }
    Rectangle {
        visible: icon.kind === "calculator"
        x: icon.width*0.24; y: icon.height*0.16; width: icon.width*0.52; height: icon.height*0.68
        radius: icon.width*0.07; color: "#273c50"; border.color: icon.accent
        Rectangle { x: parent.width*0.16; y: parent.height*0.12; width: parent.width*0.68; height: parent.height*0.19
            radius: 2; color: "#a8d5cf" }
        Repeater { model: 9
            delegate: Rectangle { required property int index
                x: parent.width*(0.16+(index%3)*0.26); y: parent.height*(0.4+Math.floor(index/3)*0.17)
                width: parent.width*0.16; height: parent.height*0.11; radius: 2
                color: index%3===2 ? icon.accent : "#aac1cf" } }
    }
    Item {
        visible: icon.kind === "results"; anchors.fill: parent
        Rectangle { x: icon.width*0.18; y: icon.height*0.26; width: icon.width*0.3; height: icon.height*0.22
            radius: icon.width*0.045; color: icon.accent }
        Rectangle { x: icon.width*0.16; y: icon.height*0.36; width: icon.width*0.68; height: icon.height*0.43
            radius: icon.width*0.045; color: icon.accent }
        Rectangle { x: icon.width*0.28; y: icon.height*0.48; width: icon.width*0.44; height: icon.height*0.035
            radius: 1; color: "#e1f2ff"; opacity: 0.8 }
    }
    Item {
        visible: icon.kind === "settings"; anchors.fill: parent
        Repeater { model: 8
            delegate: Item { required property int index; anchors.centerIn: parent
                width: icon.width*0.61; height: width; rotation: index*45
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: parent.width*0.22; height: parent.height*0.27
                    radius: width*0.2; color: icon.accent } } }
        Rectangle { anchors.centerIn: parent; width: icon.width*0.48; height: width; radius: width/2; color: icon.accent }
        Rectangle { anchors.centerIn: parent; width: icon.width*0.2; height: width; radius: width/2; color: "#273347" }
    }
    Grid {
        visible: icon.kind === "desktop"; anchors.centerIn: parent; columns: 2
        spacing: icon.width*0.07
        Repeater { model: 4
            delegate: Rectangle { width: icon.width*0.23; height: width; radius: width*0.18; color: icon.accent } }
    }
}
