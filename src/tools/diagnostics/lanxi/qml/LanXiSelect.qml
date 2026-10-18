pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Column {
    id: field
    property string label: ""
    property var options: []
    property var value
    property real uiScale: 1
    property bool expanded: false
    signal selected(var value)
    width: 250 * uiScale
    spacing: 8 * uiScale
    function optionValue(option) { return typeof option === "object" ? option.value : option }
    function optionLabel(option) { return typeof option === "object" ? option.label : String(option) }
    function currentLabel() {
        for (var i = 0; i < options.length; ++i)
            if (optionValue(options[i]) === value) return optionLabel(options[i])
        return value === undefined ? qsTr("Select") : String(value)
    }
    Text { text: field.label; color: "#c9bfd6"; font.pixelSize: Math.round(17 * field.uiScale) }
    LanXiButton {
        width: field.width; text: field.currentLabel() + (field.expanded ? "  −" : "  +")
        uiScale: field.uiScale
        onClicked: field.expanded = !field.expanded
    }
    // Expands within the page's scroll layout, without off-screen popups.
    Rectangle {
        visible: field.expanded
        width: field.width; height: Math.min(240 * field.uiScale, choices.contentHeight)
        radius: 8 * field.uiScale; color: "#30273f"; border.color: "#685579"
        ListView {
            id: choices
            anchors.fill: parent; clip: true; model: field.options
            boundsBehavior: Flickable.StopAtBounds
            delegate: LanXiButton {
                required property var modelData
                width: choices.width; height: 42 * field.uiScale; uiScale: field.uiScale
                text: field.optionLabel(modelData); checked: field.optionValue(modelData) === field.value
                onClicked: { field.selected(field.optionValue(modelData)); field.expanded = false }
            }
        }
    }
    onEnabledChanged: if (!enabled) expanded = false
}
