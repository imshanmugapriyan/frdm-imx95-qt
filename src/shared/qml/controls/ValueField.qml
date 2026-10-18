pragma ComponentBehavior: Bound
import QtQuick

Column {
    id: field
    property string label: ""
    property real value: 0
    property real minimum: -1000000
    property real maximum: 1000000
    property real uiScale: 1
    signal edited(real value)
    spacing: 9 * uiScale
    width: 250 * uiScale
    readonly property bool valid: input.text.trim().length > 0 && isFinite(Number(input.text))
                                 && Number(input.text) >= minimum && Number(input.text) <= maximum
    Text { text: field.label; color: "#c9bfd6"; font.pixelSize: Math.round(17 * field.uiScale) }
    Rectangle {
        width: parent.width
        height: 48 * field.uiScale
        radius: 8 * field.uiScale
        color: "#181421"
        border.color: !field.valid ? "#ff8d9e" : input.activeFocus ? "#cbb2fa" : "#584761"
        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: 12 * field.uiScale
            text: String(field.value)
            color: "#f8f3ff"
            font.pixelSize: Math.round(20 * field.uiScale)
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            clip: true
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: field.label
            onEditingFinished: {
                if (field.valid) field.edited(Number(text))
                text = Qt.binding(function() { return String(field.value) })
            }
        }
    }
}
