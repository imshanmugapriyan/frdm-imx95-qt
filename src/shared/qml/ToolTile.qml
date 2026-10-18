pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: tile

    required property string toolTitle
    required property string summary
    required property string category
    required property string iconKind
    required property color accent
    property real uiScale: 1
    property bool selected: false
    signal activated()

    Accessible.role: Accessible.Button
    Accessible.name: toolTitle
    Accessible.description: summary
    Accessible.focusable: true
    Accessible.focused: activeFocus
    Accessible.onPressAction: activated()

    function activateFromKey(event) {
        if (!event.isAutoRepeat)
            activated()
        event.accepted = true
    }

    Keys.onReturnPressed: function(event) { activateFromKey(event) }
    Keys.onEnterPressed: function(event) { activateFromKey(event) }
    Keys.onSpacePressed: function(event) { activateFromKey(event) }

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: 24 * tile.uiScale
        antialiasing: true
        border.width: (tile.selected ? 2 : 1) * tile.uiScale
        border.color: tile.selected ? tile.accent : pointer.containsMouse ? "#7c7099" : "#49405e"
        gradient: Gradient {
            GradientStop { position: 0; color: pointer.pressed ? "#39304f" : "#2e2644" }
            GradientStop { position: 1; color: pointer.containsMouse ? "#29253f" : "#231f37" }
        }

        Rectangle {
            x: 40 * tile.uiScale
            y: 0
            width: 64 * tile.uiScale
            height: 3 * tile.uiScale
            radius: height / 2
            color: tile.accent
        }

        ToolIcon {
            x: 40 * tile.uiScale
            y: 40 * tile.uiScale
            width: 100 * tile.uiScale
            height: width
            kind: tile.iconKind
            accent: tile.accent
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 40 * tile.uiScale
            y: 50 * tile.uiScale
            text: tile.category
            color: tile.accent
            font.pixelSize: Math.round(14 * tile.uiScale)
            font.weight: Font.DemiBold
            font.letterSpacing: 2 * tile.uiScale
        }

        Text {
            id: titleLabel
            x: 40 * tile.uiScale
            y: 178 * tile.uiScale
            width: parent.width - 80 * tile.uiScale
            text: tile.toolTitle
            color: "#f8f5ff"
            font.pixelSize: Math.round(32 * tile.uiScale)
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Text {
            x: titleLabel.x
            anchors.top: titleLabel.bottom
            anchors.topMargin: 12 * tile.uiScale
            width: titleLabel.width
            text: tile.summary
            color: "#bfb6d1"
            font.pixelSize: Math.round(20 * tile.uiScale)
            elide: Text.ElideRight
        }

        Rectangle {
            x: 40 * tile.uiScale
            y: parent.height - 102 * tile.uiScale
            width: parent.width - 80 * tile.uiScale
            height: 1
            color: "#443b57"
        }

        Text {
            x: 40 * tile.uiScale
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 42 * tile.uiScale
            text: qsTr("Open tool")
            color: tile.accent
            font.pixelSize: Math.round(20 * tile.uiScale)
            font.weight: Font.Medium
        }

        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 32 * tile.uiScale
            width: 52 * tile.uiScale
            height: width
            radius: width / 2
            color: Qt.rgba(tile.accent.r, tile.accent.g, tile.accent.b, 0.12)
            antialiasing: true

            // Arrow built from geometry to avoid relying on an icon font.
            Rectangle {
                anchors.centerIn: parent
                width: 19 * tile.uiScale
                height: 2 * tile.uiScale
                color: tile.accent
            }
            Rectangle {
                x: 27 * tile.uiScale
                y: 20 * tile.uiScale
                width: 10 * tile.uiScale
                height: 2 * tile.uiScale
                rotation: 45
                color: tile.accent
                antialiasing: true
            }
            Rectangle {
                x: 27 * tile.uiScale
                y: 27 * tile.uiScale
                width: 10 * tile.uiScale
                height: 2 * tile.uiScale
                rotation: -45
                color: tile.accent
                antialiasing: true
            }
        }
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: tile.activated()
    }
}
