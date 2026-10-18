pragma ComponentBehavior: Bound
import QtQuick
import "../../shared/qml" as Shared

Rectangle {
    id: tile
    property string title: ""
    property string iconKind: ""
    property color accent: "#be9bff"
    property bool compact: false
    property bool running: false
    property bool selected: false
    signal activated()
    implicitWidth: compact ? 88 : 250
    implicitHeight: compact ? 86 : 174
    radius: 16
    color: mouse.containsMouse || activeFocus || selected ? "#24d0ccff" : "transparent"
    border.color: activeFocus ? "#cfbdff" : "transparent"
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.onPressAction: if (enabled) activated()
    Keys.onReturnPressed: function(event) { if (!event.isAutoRepeat) activated(); event.accepted=true }
    Keys.onEnterPressed: function(event) { if (!event.isAutoRepeat) activated(); event.accepted=true }
    Keys.onSpacePressed: function(event) { if (!event.isAutoRepeat) activated(); event.accepted=true }
    Shared.ToolIcon {
        anchors.horizontalCenter: parent.horizontalCenter
        y: tile.compact ? 8 : 12
        width: tile.compact ? 62 : 82
        height: width
        kind: tile.iconKind; accent: tile.accent
    }
    Text {
        x: 10; y: 108; width: parent.width-20; height: 56
        text: tile.title; visible: !tile.compact
        textFormat: Text.PlainText; wrapMode: Text.WordWrap; maximumLineCount: 2
        horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
        color: "#f4efff"; font.pixelSize: 20; font.weight: Font.Medium
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; y: parent.height-8
        width: 5; height: 5; radius: 3; color: tile.accent
        visible: tile.compact && tile.running
    }
    Rectangle {
        visible: tile.compact && (mouse.containsMouse || tile.activeFocus)
        anchors.horizontalCenter: parent.horizontalCenter; y: -44
        width: tooltip.implicitWidth+24; height: 34; radius: 8; color: "#ee171d32"
        Text { id: tooltip; anchors.centerIn: parent; text: tile.title; color: "#faf5ff"; font.pixelSize: 16 }
    }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
        onClicked: { tile.forceActiveFocus(); tile.activated() } }
}
