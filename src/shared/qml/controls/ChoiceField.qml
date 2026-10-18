pragma ComponentBehavior: Bound
import QtQuick

Column {
    id: field
    property string label: ""
    property var options: []
    property var value
    property real uiScale: 1
    signal selected(var value)
    spacing: 9 * uiScale
    Text { text: field.label; color: "#c9bfd6"; font.pixelSize: Math.round(17 * field.uiScale) }
    Row {
        spacing: 6 * field.uiScale
        Repeater {
            model: field.options
            delegate: ActionButton {
                required property var modelData
                uiScale: field.uiScale
                text: String(modelData)
                checked: field.value === modelData
                onClicked: field.selected(modelData)
            }
        }
    }
}
