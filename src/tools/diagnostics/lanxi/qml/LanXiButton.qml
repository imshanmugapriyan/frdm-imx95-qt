pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: control
    property string text: ""
    property real uiScale: 1
    property bool checked: false
    property bool primary: false
    signal clicked()
    implicitHeight: 34 * uiScale
    implicitWidth: Math.max(74 * uiScale, caption.implicitWidth + 24 * uiScale)
    radius: 4 * uiScale
    color: checked || primary ? "#217b9b" : pointer.containsMouse ? "#344b60" : "#243546"
    border.color: activeFocus ? "#c5efff" : checked ? "#4cc5e5" : "#42596c"
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: { if (enabled) clicked() }
    Keys.onReturnPressed: function(event) { if (!event.isAutoRepeat) clicked(); event.accepted = true }
    Keys.onSpacePressed: function(event) { if (!event.isAutoRepeat) clicked(); event.accepted = true }
    Text { id: caption; anchors.centerIn: parent; width: Math.max(0,parent.width-20*control.uiScale); horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; text: control.text; color: "#f1f6fa"; font.pixelSize: Math.round(14 * control.uiScale) }
    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: {control.forceActiveFocus(); control.clicked()} }
}
