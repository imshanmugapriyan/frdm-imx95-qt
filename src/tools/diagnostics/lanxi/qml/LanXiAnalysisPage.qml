pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Column {
    id: panel
    required property var host
    property real uiScale: 1
    readonly property var analysis: host.config.analysis || ({})
    readonly property var trigger: host.config.trigger || ({})
    spacing: 22 * uiScale
    Text { text: qsTr("Processing setup"); color: "#eee5fa"; font.pixelSize: Math.round(26 * panel.uiScale) }
    Text { width: parent.width; wrapMode: Text.WordWrap; color: "#baaecd"; font.pixelSize: Math.round(18 * panel.uiScale)
        text: qsTr("Calibrated samples → optional high/low-pass filters → FFT window → spectral averaging → frequency weighting / bands. Levels and harmonic fits use calibrated samples before these analysis filters. Raw recordings remain available for reprocessing.") }
    Flow {
        width: parent.width; spacing: 18 * panel.uiScale; enabled: panel.host.idle
        LanXiSelect { label: qsTr("FFT window"); value: panel.analysis.window; options: ["hann","hamming","blackman","flat-top","rectangular"]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["analysis","window"], value) } }
        LanXiSelect { label: qsTr("Spectral averaging"); value: panel.analysis.averaging; options: ["linear","exponential"]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["analysis","averaging"], value) } }
        ValueField { label: qsTr("Exponential average count"); value: panel.analysis.averages || 8; minimum: 2; maximum: 10000; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["analysis","averages"], Math.round(value)) } }
        LanXiButton { text: qsTr("Remove FFT mean"); checked: panel.analysis.removeMean || false; uiScale: panel.uiScale
            onClicked: panel.host.edit(["analysis","removeMean"], !panel.analysis.removeMean) }
        ValueField { label: qsTr("Analysis high-pass Hz (0 = off)"); value: panel.analysis.highPassHz || 0; minimum: 0; maximum: 25600; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["analysis","highPassHz"], value) } }
        ValueField { label: qsTr("Analysis low-pass Hz (0 = off)"); value: panel.analysis.lowPassHz || 0; minimum: 0; maximum: 25600; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["analysis","lowPassHz"], value) } }
        LanXiSelect { label: qsTr("Frequency weighting"); value: panel.analysis.weighting; options: ["Z","A","C"]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["analysis","weighting"], value) } }
        LanXiSelect { label: qsTr("Level time weighting"); value: panel.analysis.timeWeighting; options: ["Fast","Slow"]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["analysis","timeWeighting"], value) } }
        LanXiSelect { label: qsTr("FFT band integration"); value: panel.analysis.fraction; options: [{value:1,label:"Octave"},{value:3,label:"Third-octave"}]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["analysis","fraction"], value) } }
        ValueField { label: qsTr("Known fundamental (Hz)"); value: panel.analysis.toneHz || 1000; minimum: 1; maximum: 25600; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["analysis","toneHz"], value) } }
        ValueField { label: qsTr("Highest harmonic"); value: panel.analysis.harmonics || 5; minimum: 2; maximum: 10; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["analysis","harmonics"], Math.round(value)) } }
    }
    Text { text: qsTr("Event capture"); color: "#eee5fa"; font.pixelSize: Math.round(26 * panel.uiScale) }
    Text { width: parent.width; wrapMode: Text.WordWrap; color: "#baaecd"; font.pixelSize: Math.round(18 * panel.uiScale)
        text: qsTr("Capture the first threshold crossing after the pre-trigger buffer fills. This captures one channel in its own module clock; full-session raw recording continues. Event CSV is included in report export. Maximum event length: 1,048,576 samples.") }
    Flow {
        width: parent.width; spacing: 18 * panel.uiScale; enabled: panel.host.idle
        LanXiButton { text: qsTr("Enable event capture"); checked: panel.trigger.enabled || false; uiScale: panel.uiScale
            onClicked: panel.host.edit(["trigger","enabled"], !panel.trigger.enabled) }
        LanXiSelect { label: qsTr("Trigger module"); value: panel.trigger.device; options: [{value:0,label:"3676"},{value:1,label:"3677"}]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["trigger","device"], value) } }
        LanXiSelect { label: qsTr("Trigger channel"); value: panel.trigger.channel; options: [1,2,3,4]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["trigger","channel"], value) } }
        LanXiSelect { label: qsTr("Trigger edge"); value: panel.trigger.rising; options: [{value:true,label:"Rising"},{value:false,label:"Falling"}]; uiScale: panel.uiScale
            onSelected: function(value) { panel.host.edit(["trigger","rising"], value) } }
        ValueField { label: qsTr("Threshold (channel unit)"); value: panel.trigger.level || 0; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["trigger","level"], value) } }
        ValueField { label: qsTr("Hysteresis (channel unit)"); value: panel.trigger.hysteresis || 0; minimum: 0; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["trigger","hysteresis"], value) } }
        ValueField { label: qsTr("Pre-trigger seconds"); value: panel.trigger.preSeconds || 0; minimum: 0; maximum: 60; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["trigger","preSeconds"], value) } }
        ValueField { label: qsTr("Post-trigger seconds"); value: panel.trigger.postSeconds || 0.5; minimum: 0.001; maximum: 60; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.edit(["trigger","postSeconds"], value) } }
    }
    Text { width: parent.width; wrapMode: Text.WordWrap; color: "#baaecd"; font.pixelSize: Math.round(18 * panel.uiScale)
        text: qsTr("Fast = 125 ms; Slow = 1 s. A/C levels use bilinear digital filters. Octave bands are FFT energy estimates; unresolved bands are omitted. These functions have not been qualified as an IEC-compliant sound-level meter or octave-filter instrument.") }
}
