pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Rectangle {
    id: inspector
    required property var host
    property real uiScale: 1
    readonly property var selected: (host.status.channels || []).filter(function(c) {return c.device === inspector.host.device && c.channel === inspector.host.channel})[0] || ({})
    property real audioFullScale: 1
    property real audioVolume: .1
    color: "#172431"; border.color: "#35495b"
    Flickable {
        anchors.fill: parent; anchors.margins: 14 * inspector.uiScale; contentWidth: width; contentHeight: details.height; clip: true
        Column {
            id: details; width: parent.width; spacing: 16 * inspector.uiScale
            Text { text: qsTr("PROPERTIES"); color: "#8aa4b8"; font.pixelSize: 12 * inspector.uiScale; font.letterSpacing: 1 }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: inspector.host.currentChannel.name || qsTr("Selected channel"); color: "#f0f6fc"; font.pixelSize: 22 * inspector.uiScale }
            Repeater {
                model: [
                    {label:"Module / input",value:(inspector.host.device === 0 ? "3676" : "3677") + " / " + inspector.host.channel},
                    {label:"Engineering unit",value:inspector.host.currentChannel.unit || "V"},
                    {label:"Sensitivity (V/unit)",value:String(inspector.host.currentChannel.sensitivity || 1)},
                    {label:"Sample rate",value:(inspector.selected.rate || inspector.host.currentDevice.sampleRate || 0) + " Hz"},
                    {label:"RMS level",value:inspector.selected.rms === undefined || inspector.selected.rms === null ? "—" : Number(inspector.selected.rms).toPrecision(6)},
                    {label:"Averages",value:inspector.selected.averages || "0"},
                    {label:"Sample quality",value:inspector.selected.quality ? "INVALID: " + inspector.selected.quality : inspector.selected.samples ? "Valid" : "No samples"},
                    {label:"Generator",value:inspector.host.status.output || "Not checked"}]
                delegate: Column {
                    required property var modelData
                    width: details.width; spacing: 3 * inspector.uiScale
                    Text { width: parent.width; text: parent.modelData.label; color: "#8aa4b8"; font.pixelSize: 12 * inspector.uiScale }
                    Text { width: parent.width; text: parent.modelData.value; color: "#e5eff8"; font.pixelSize: 16 * inspector.uiScale; elide: Text.ElideRight }
                }
            }
            Rectangle { width: parent.width; height: 1; color: "#35495b" }
            Text { text: qsTr("COMPARE"); color: "#8aa4b8"; font.pixelSize: 12 * inspector.uiScale }
            LanXiButton { width: parent.width; uiScale: inspector.uiScale; text: qsTr("Pin current curve"); enabled: inspector.host.idle; onClicked: inspector.host.backend.pinComparison() }
            LanXiButton { width: parent.width; uiScale: inspector.uiScale; text: qsTr("Clear reference"); onClicked: inspector.host.backend.clearComparison() }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: inspector.host.referencePlot.name || qsTr("No pinned reference"); color: "#9bafc0"; font.pixelSize: 13 * inspector.uiScale }
            Text { width: parent.width; wrapMode: Text.WordWrap; visible: inspector.host.referencePlot.runId !== undefined; text: inspector.host.compatibleComparison ? qsTr("Frequency grid and units match") : qsTr("Overlay hidden: different grid or units"); color: "#ecc478"; font.pixelSize: 12 * inspector.uiScale }
            Rectangle { width: parent.width; height: 1; color: "#35495b" }
            Text { text: qsTr("AUDIO AUDITION"); color: "#8aa4b8"; font.pixelSize: 12 * inspector.uiScale }
            ValueField { width:parent.width; label:qsTr("Full scale (channel unit)"); value:inspector.audioFullScale; minimum:1e-9; maximum:1e9; uiScale:inspector.uiScale*.8; onEdited:function(value){inspector.audioFullScale=value} }
            ValueField { width:parent.width; label:qsTr("Playback volume (0..1)"); value:inspector.audioVolume; minimum:0; maximum:1; uiScale:inspector.uiScale*.8; onEdited:function(value){inspector.audioVolume=value} }
            LanXiButton { width:parent.width; text:qsTr("Listen to captured window"); uiScale:inspector.uiScale; enabled:inspector.host.idle&&inspector.host.backend.audio.available; onClicked:inspector.host.backend.audition(inspector.audioFullScale,inspector.audioVolume) }
            LanXiButton { width:parent.width; text:qsTr("Mute audio"); uiScale:inspector.uiScale; onClicked:inspector.host.backend.muteAudio() }
            Text { width:parent.width; wrapMode:Text.WordWrap; text:inspector.host.backend.audio.available?inspector.host.backend.audio.message:qsTr("Audio is disabled in this build."); color:"#9bafc0"; font.pixelSize:12*inspector.uiScale }
        }
    }
}
