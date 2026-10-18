pragma ComponentBehavior: Bound
import QtQuick
import FRDM_IMX95_QT_DSW
import "../../../../shared/qml/controls"

Rectangle {
    id: panel
    property string title: ""
    property string axis: ""
    property string emptyText: qsTr("No valid measurement data")
    property var values: []
    property var comparison: []
    property real xStart: 0
    property real xStep: 1
    property real uiScale: 1
    property bool fixedRange: false
    property real low: 0
    property real high: 1
    property color accent: "#80d4ba"
    property int zoom: 1
    property real position: 0
    property int cursorIndex: -1
    readonly property int visibleCount: Math.max(2, Math.ceil(values.length / zoom))
    readonly property int first: Math.max(0, Math.min(Math.max(0,values.length-visibleCount),Math.round(position * values.length)))
    readonly property var displayed: values.slice(first, first+visibleCount)
    readonly property var reference: comparison.slice(first, first+visibleCount)
    readonly property bool hasData: values.some(function(v) { return v !== null && v !== undefined && isFinite(v) })
    readonly property var extent: {
        if (fixedRange) return [low, high]
        var lo = Infinity, hi = -Infinity
        var combined = displayed.concat(reference)
        for (var i = 0; i < combined.length; ++i) {
            var v = combined[i]
            if (v === null || v === undefined || !isFinite(v)) continue
            lo = Math.min(lo, v); hi = Math.max(hi, v)
        }
        if (!isFinite(lo)) return [0, 1]
        var margin = Math.max((hi - lo) * 0.08, Math.abs(hi) * 0.01, 0.000001)
        return [lo - margin, hi + margin]
    }
    radius: 12 * uiScale; color: "#172431"; border.color: "#35495b"
    Text {
        x: 18 * panel.uiScale; y: 10 * panel.uiScale; width: Math.max(0,parent.width - 205 * panel.uiScale)
        text: panel.title; color: "#e8dbf7"; font.pixelSize: Math.round(18 * panel.uiScale); elide: Text.ElideRight
    }
    DiagnosticPlot {
        id: graph
        anchors.fill: parent; anchors.topMargin: 34 * panel.uiScale
        uiScale: panel.uiScale; minimum: panel.extent[0]; maximum: panel.extent[1]
        series: panel.hasData ? (panel.reference.length ? [{label:"Current",color:String(panel.accent),values:panel.displayed},{label:"Pinned",color:"#e9bd79",values:panel.reference}]
                                                       : [{label:"", color:String(panel.accent), values:panel.displayed}]) : []
        xStart: panel.xStart+panel.first*panel.xStep; xStep: panel.xStep; xLabel: panel.axis; emptyText: panel.emptyText
    }
    Row {
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 4 * panel.uiScale; spacing: 3 * panel.uiScale
        Repeater { model: ["−","+","←","→","Fit"]
            delegate: LanXiButton { required property string modelData; width: 32 * panel.uiScale; height: 27 * panel.uiScale; uiScale: panel.uiScale; text: modelData
                onClicked: {
                    if(modelData === "+") panel.zoom=Math.min(64,panel.zoom*2)
                    else if(modelData === "−") panel.zoom=Math.max(1,panel.zoom/2)
                    else if(modelData === "←") panel.position=Math.max(0,panel.position-0.5/panel.zoom)
                    else if(modelData === "→") panel.position=Math.min(1-1/panel.zoom,panel.position+0.5/panel.zoom)
                    else {panel.zoom=1;panel.position=0}
                } }
        }
    }
    MouseArea {
        x: 76 * panel.uiScale; y: 58 * panel.uiScale; width: Math.max(1,parent.width-100*panel.uiScale); height: Math.max(1,parent.height-116*panel.uiScale)
        hoverEnabled: true
        onPositionChanged: function(mouse) { panel.cursorIndex=panel.first+Math.round(Math.max(0,Math.min(1,mouse.x/width))*(panel.displayed.length-1)) }
        onExited: panel.cursorIndex=-1
    }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 5 * panel.uiScale; color: "#ffe0a8"; font.pixelSize: Math.round(12 * panel.uiScale)
        visible: panel.cursorIndex>=0 && panel.cursorIndex<panel.values.length
        text: visible ? "x: "+(panel.xStart+panel.cursorIndex*panel.xStep).toPrecision(6)+"  y: "+(panel.values[panel.cursorIndex] === null ? "invalid" : Number(panel.values[panel.cursorIndex]).toPrecision(6)) : "" }
}
