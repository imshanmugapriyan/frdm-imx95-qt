pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: panel
    property var rows: []
    property var times: []
    property real maximumHz: 0
    property real uiScale: 1
    property real floorDb: -100
    property real ceilingDb: 20
    color: "#211a2f"; radius: 12 * uiScale; border.color: "#4c3b5d"
    onRowsChanged: map.requestPaint()
    onFloorDbChanged: map.requestPaint()
    onCeilingDbChanged: map.requestPaint()
    Text { x: 16 * panel.uiScale; y: 10 * panel.uiScale; color: "#e8dbf7"; font.pixelSize: Math.round(18 * panel.uiScale)
        text: qsTr("Spectrogram · dB PSD · %1 to %2 dB").arg(panel.floorDb).arg(panel.ceilingDb) }
    Canvas {
        id: map
        anchors.fill: parent; anchors.margins: 18 * panel.uiScale; anchors.topMargin: 45 * panel.uiScale; anchors.bottomMargin: 36 * panel.uiScale
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx=getContext("2d");ctx.clearRect(0,0,width,height)
            for(var r=0;r<panel.rows.length;++r) {
                var row=panel.rows[r]
                for(var c=0;c<row.length;++c) {
                    if(row[c]===null || !isFinite(row[c]))continue
                    var v=Math.max(0,Math.min(1,(row[c]-panel.floorDb)/Math.max(1,panel.ceilingDb-panel.floorDb)))
                    ctx.fillStyle=Qt.hsla(0.72-0.62*v,0.75,0.12+0.55*v,1)
                    ctx.fillRect(c*width/row.length,(panel.rows.length-1-r)*height/panel.rows.length,Math.ceil(width/row.length),Math.ceil(height/panel.rows.length))
                }
            }
        }
    }
    Text { anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.margins: 8 * panel.uiScale; color: "#bfb0cf"; font.pixelSize: Math.round(13 * panel.uiScale)
        text: panel.times.length ? qsTr("Newest at top · %1 to %2 s").arg(panel.times[0].toFixed(2)).arg(panel.times[panel.times.length-1].toFixed(2)) : qsTr("Waiting for spectral frames") }
    Text { anchors.bottom: parent.bottom; anchors.right: parent.right; anchors.margins: 8 * panel.uiScale; color: "#bfb0cf"; font.pixelSize: Math.round(13 * panel.uiScale)
        text: qsTr("0 — %1 Hz").arg(panel.maximumHz.toFixed(0)) }
}
