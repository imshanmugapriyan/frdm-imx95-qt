pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: metric
    property string label: ""
    property string value: "--"
    property real uiScale: 1
    color: "#282135"
    border.color: "#4c3d5d"
    radius: 12 * uiScale
    implicitHeight: 102 * uiScale
    Text {
        x: 20 * metric.uiScale; y: 15 * metric.uiScale
        text: metric.label; color: "#b8acc8"
        font.pixelSize: Math.round(15 * metric.uiScale)
    }
    Text {
        x: 20 * metric.uiScale; y: 42 * metric.uiScale
        width: parent.width - 40 * metric.uiScale
        text: metric.value; color: "#f8f1ff"
        font.pixelSize: Math.round(28 * metric.uiScale)
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
}
