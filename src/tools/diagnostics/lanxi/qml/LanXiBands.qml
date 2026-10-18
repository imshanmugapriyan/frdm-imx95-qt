pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: panel
    property var bands: []
    property string unit: "dB"
    property real uiScale: 1
    readonly property real low: {
        var levels = bands.filter(function(b) { return b.resolved && b.level !== null }).map(function(b) { return b.level })
        return levels.length ? Math.floor((Math.min.apply(null, levels) - 10) / 10) * 10 : 0
    }
    readonly property real high: {
        var levels = bands.filter(function(b) { return b.resolved && b.level !== null }).map(function(b) { return b.level })
        return levels.length ? Math.max(low + 20, Math.ceil((Math.max.apply(null, levels) + 5) / 10) * 10) : 100
    }
    color: "#211a2f"; radius: 12 * uiScale; border.color: "#4c3b5d"
    Text { x: 16 * panel.uiScale; y: 10 * panel.uiScale; text: qsTr("Octave-band energy · %1").arg(panel.unit); color: "#e8dbf7"; font.pixelSize: Math.round(18 * panel.uiScale) }
    Row {
        id: bars
        anchors.fill: parent; anchors.margins: 18 * panel.uiScale; anchors.topMargin: 48 * panel.uiScale; anchors.bottomMargin: 42 * panel.uiScale
        spacing: 3 * panel.uiScale
        Repeater {
            model: panel.bands
            delegate: Item {
                id: band
                required property var modelData
                width: Math.max(1, (bars.width - Math.max(0, panel.bands.length - 1) * bars.spacing) / Math.max(1,panel.bands.length)); height: bars.height
                Rectangle { anchors.bottom: parent.bottom; width: parent.width
                    height: band.modelData.level !== null && band.modelData.resolved ? Math.max(1,(band.modelData.level-panel.low)/(panel.high-panel.low)*band.height) : 2
                    color: band.modelData.resolved ? "#80d4ba" : "#594968"; radius: 2 }
                Text { anchors.top: parent.bottom; anchors.topMargin: 8 * panel.uiScale; anchors.horizontalCenter: parent.horizontalCenter
                    text: band.modelData.hz >= 1000 ? (band.modelData.hz/1000).toPrecision(2)+"k" : band.modelData.hz.toPrecision(2)
                    color: "#bfb0cf"; font.pixelSize: Math.round(11 * panel.uiScale); rotation: -45 }
                MouseArea { anchors.fill: parent; hoverEnabled: true
                    onEntered: tooltip.text = band.modelData.hz.toFixed(1)+" Hz: "+(band.modelData.resolved && band.modelData.level !== null ? band.modelData.level.toFixed(2)+" "+panel.unit : "unresolved at this FFT size") }
            }
        }
    }
    Text { id: tooltip; anchors.bottom: parent.bottom; anchors.right: parent.right; anchors.margins: 6 * panel.uiScale
        text: qsTr("Hover a band for its value"); color: "#c9bad9"; font.pixelSize: Math.round(13 * panel.uiScale) }
}
