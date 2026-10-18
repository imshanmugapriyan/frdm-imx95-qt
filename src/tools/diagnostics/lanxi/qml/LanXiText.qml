pragma ComponentBehavior: Bound
import QtQuick

Column {
    id: field
    property string label: ""
    property string value: ""
    property real uiScale: 1
    signal edited(string value)
    spacing: 8 * uiScale
    width: 300 * uiScale
    Text { text: field.label; color: "#c9bfd6"; font.pixelSize: Math.round(17 * field.uiScale) }
    Rectangle {
        width: parent.width; height: 48 * field.uiScale; radius: 8 * field.uiScale
        color: "#181421"; border.color: input.activeFocus ? "#cbb2fa" : "#584761"
        TextInput {
            id: input
            anchors.fill: parent; anchors.margins: 12 * field.uiScale
            text: field.value; color: "#f8f3ff"; font.pixelSize: Math.round(19 * field.uiScale)
            verticalAlignment: TextInput.AlignVCenter; selectByMouse: true; clip: true
            Accessible.name: field.label
            onTextEdited: field.edited(text)
        }
    }
}
