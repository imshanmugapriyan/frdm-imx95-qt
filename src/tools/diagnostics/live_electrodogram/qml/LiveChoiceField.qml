pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Column {
    id: field
    property string label: ""
    property var options: []
    property var value
    property int columns: 4
    property real uiScale: 1
    signal selected(var value)
    spacing: 8*uiScale
    Text { text: field.label; color: "#beb5cd"; font.pixelSize: Math.round(15*field.uiScale) }
    Grid {
        width: parent.width; columns: field.columns; spacing: 6*field.uiScale
        Repeater {
            model: field.options
            delegate: ActionButton {
                required property var modelData
                width: (field.width-(field.columns-1)*6*field.uiScale)/field.columns
                height: 36*field.uiScale; uiScale: field.uiScale
                text: String(modelData); checked: modelData === field.value
                Accessible.name: field.label + ": " + text
                onClicked: field.selected(modelData)
            }
        }
    }
}
