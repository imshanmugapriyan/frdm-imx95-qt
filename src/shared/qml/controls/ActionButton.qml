pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: button
    property string text: ""
    property real uiScale: 1
    property bool primary: false
    property bool checked: false
    signal clicked()
    implicitWidth: Math.max(110 * uiScale, label.implicitWidth + 36 * uiScale)
    implicitHeight: 48 * uiScale
    radius: 9 * uiScale
    color: primary || checked ? (mouse.pressed ? "#a589d4" : "#cbb2fa") : mouse.containsMouse ? "#423651" : "#30273f"
    border.color: activeFocus ? "#ffffff" : "#685579"
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: { if (enabled) clicked() }
    function activate(event) { if (!event.isAutoRepeat && enabled) clicked(); event.accepted = true }
    Keys.onReturnPressed: function(event) { activate(event) }
    Keys.onEnterPressed: function(event) { activate(event) }
    Keys.onSpacePressed: function(event) { activate(event) }
    Text {
        id: label
        anchors.centerIn: parent
        text: button.text
        color: button.primary || button.checked ? "#21132f" : "#f3ebff"
        font.pixelSize: Math.round(18 * button.uiScale)
        font.weight: Font.Medium
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { button.forceActiveFocus(); button.clicked() }
    }
}
