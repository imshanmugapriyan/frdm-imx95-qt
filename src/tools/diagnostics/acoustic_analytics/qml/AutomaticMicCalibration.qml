pragma ComponentBehavior: Bound
import QtQuick
import FRDM_IMX95_QT_DSW
import "../../../../services/diagnostics/qml/Analysis.js" as Analysis
import "../../../../shared/qml/controls"

Item {
    id: view
    required property var session
    property real uiScale: 1
    property var settings: Analysis.micDefaults()
    property string tab: "Settings"
    property string displayChannel: "Front"
    property bool showRmsTrend: false
    readonly property real measurementTarget: {
        session.revision
        return Object.keys(session.model.microphones).length ? session.model.micConfig.target : settings.target
    }
    function change(name,value) { var next=Analysis.copy(settings); next[name]=value; settings=next }
    function channel(name) { session.revision; return session.model.microphones[name] || null }
    function metric(name,key,unit) {
        var c=channel(name)
        if (!c || !c.stats || c.stale) return "--"
        var value=key === "frequency" ? c.spectrum.peakHz : key === "deviation" ? c.stats.dbfs-measurementTarget : c.stats[key]
        return Analysis.finite(value) ? value.toFixed(key === "frequency" ? 0 : 2)+(unit || "") : "--"
    }

    Row {
        spacing: 8*view.uiScale
        Repeater {
            model: ["Settings","Calibration","Diagnostics"]
            delegate: ActionButton { required property string modelData; text: modelData; checked: view.tab===modelData
                uiScale: view.uiScale; onClicked: view.tab=modelData }
        }
    }
    Row {
        anchors.right: parent.right; spacing: 12*view.uiScale
        ActionButton { text: qsTr("Start calibration"); primary: true; uiScale: view.uiScale
            enabled: !view.session.busy && view.session.available("automatic-mic")
            onClicked: { view.session.start("automatic-mic",view.settings); if(view.session.busy) view.tab="Calibration" } }
        ActionButton { text: qsTr("Finish"); uiScale: view.uiScale
            enabled: view.session.mode === "automatic-mic" && ["running","starting"].indexOf(view.session.phase)>=0
            onClicked: view.session.finish() }
    }
    Rectangle {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Settings"
        color: "#231c30"; border.color: "#4e3d60"; radius: 16*view.uiScale
        Grid {
            x: 32*view.uiScale; y: 32*view.uiScale; columns: 4; columnSpacing: 50*view.uiScale; rowSpacing: 26*view.uiScale
            enabled: !view.session.busy
            ValueField { label: qsTr("Target RMS (dBFS)"); width: 375*view.uiScale; value: view.settings.target; minimum: -160; maximum: 0; uiScale: view.uiScale; onEdited: function(v) { view.change("target",v) } }
            ValueField { label: qsTr("RMS tolerance (dB)"); width: 375*view.uiScale; value: view.settings.tolerance; minimum: 0.01; maximum: 30; uiScale: view.uiScale; onEdited: function(v) { view.change("tolerance",v) } }
            ValueField { label: qsTr("Hold duration (s)"); width: 375*view.uiScale; value: view.settings.holdSeconds; minimum: 0.1; maximum: 600; uiScale: view.uiScale; onEdited: function(v) { view.change("holdSeconds",v) } }
            ValueField { label: qsTr("RMS window (ms)"); width: 375*view.uiScale; value: view.settings.windowMs; minimum: 20; maximum: 60000; uiScale: view.uiScale; onEdited: function(v) { view.change("windowMs",Math.round(v)) } }
            ValueField { label: qsTr("Stability span (dB)"); width: 375*view.uiScale; value: view.settings.stability; minimum: 0; maximum: 20; uiScale: view.uiScale; onEdited: function(v) { view.change("stability",v) } }
            ValueField { label: qsTr("Hysteresis (dB)"); width: 375*view.uiScale; value: view.settings.hysteresis; minimum: 0; maximum: view.settings.tolerance-0.001; uiScale: view.uiScale; onEdited: function(v) { view.change("hysteresis",v) } }
            ValueField { label: qsTr("Fine adjustment multiplier"); width: 375*view.uiScale; value: view.settings.fineMultiplier; minimum: 1.01; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("fineMultiplier",v) } }
            ValueField { label: qsTr("Maximum clipping (%)"); width: 375*view.uiScale; value: view.settings.maxClipping; minimum: 0; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("maxClipping",v) } }
            ValueField { label: qsTr("Expected tone (Hz)"); width: 375*view.uiScale; value: view.settings.expectedHz; minimum: 1; maximum: 96000; uiScale: view.uiScale; onEdited: function(v) { view.change("expectedHz",v) } }
            ValueField { label: qsTr("Tone tolerance (Hz)"); width: 375*view.uiScale; value: view.settings.frequencyTolerance; minimum: 0; maximum: 20000; uiScale: view.uiScale; onEdited: function(v) { view.change("frequencyTolerance",v) } }
        }
        ChoiceField { x: 32*view.uiScale; y: 378*view.uiScale; label: qsTr("Microphone"); options: ["Auto","Front","Back","Both"]
            uiScale: view.uiScale; value: view.settings.microphone; enabled: !view.session.busy; onSelected: function(v) { view.change("microphone",v) } }
        ChoiceField { x: 830*view.uiScale; y: 378*view.uiScale; label: qsTr("FFT size"); options: [256,512,1024,2048,4096,8192]
            uiScale: view.uiScale; value: view.settings.fftSize; enabled: !view.session.busy; onSelected: function(v) { view.change("fftSize",v) } }
        ChoiceField { x: 32*view.uiScale; y: 490*view.uiScale; label: qsTr("Spectrum window"); options: ["Hamming","Hann","Rectangular"]
            uiScale: view.uiScale; value: view.settings.window; enabled: !view.session.busy; onSelected: function(v) { view.change("window",v) } }
        ActionButton { x: 830*view.uiScale; y: 524*view.uiScale; text: view.settings.validateTone ? qsTr("Test tone validation: on") : qsTr("Test tone validation: off")
            uiScale: view.uiScale; checked: view.settings.validateTone; enabled: !view.session.busy; onClicked: view.change("validateTone",!view.settings.validateTone) }
        Row {
            x: 32*view.uiScale; y: 615*view.uiScale; spacing: 12*view.uiScale
            ActionButton { text: qsTr("Restore defaults"); uiScale: view.uiScale; enabled: !view.session.busy; onClicked: view.settings=Analysis.micDefaults() }
            ActionButton { text: qsTr("Save profile"); uiScale: view.uiScale; enabled: !view.session.busy; onClicked: view.session.saveProfile("automatic-mic",view.settings) }
            ActionButton { text: qsTr("Load profile"); uiScale: view.uiScale; enabled: !view.session.busy; onClicked: { var p=view.session.loadProfile("automatic-mic"); if(p) view.settings=p } }
        }
    }
    Item {
        id: calibrationPage
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Calibration"
        Column {
            id: microphoneMetrics
            width: parent.width
            spacing: 14*view.uiScale
            Repeater {
                model: ["Front","Back"]
                delegate: Row {
                    id: micRow
                    required property string modelData
                    width: calibrationPage.width
                    spacing: 14*view.uiScale
                    readonly property real cardWidth: (width-115*view.uiScale-4*spacing)/4
                    Text { text: micRow.modelData; width: 115*view.uiScale; anchors.verticalCenter: parent.verticalCenter; color: "#eaddf8"; font.pixelSize: Math.round(23*view.uiScale) }
                    Metric { label: qsTr("CURRENT RMS"); value: view.metric(micRow.modelData,"dbfs"," dBFS"); width: micRow.cardWidth; uiScale: view.uiScale }
                    Metric { label: qsTr("DEVIATION FROM TARGET"); value: view.metric(micRow.modelData,"deviation"," dB"); width: micRow.cardWidth; uiScale: view.uiScale }
                    Metric { label: qsTr("MICROPHONE SPL"); value: view.metric(micRow.modelData,"spl"," dB SPL"); width: micRow.cardWidth; uiScale: view.uiScale }
                    Metric { label: qsTr("DOMINANT FREQUENCY"); value: view.metric(micRow.modelData,"frequency"," Hz"); width: micRow.cardWidth; uiScale: view.uiScale }
                }
            }
        }
        Text {
            id: qualityStatus
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: microphoneMetrics.bottom; anchors.topMargin: 12*view.uiScale
            text: view.session.quality.status; color: view.session.quality.achieved ? "#90dfba" : "#dcc0ff"
            elide: Text.ElideRight
            font.pixelSize: Math.round(25*view.uiScale); font.weight: Font.DemiBold }
        Rectangle {
            id: holdProgress
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: qualityStatus.bottom; anchors.topMargin: 12*view.uiScale
            height: 9*view.uiScale; radius: height/2; color: "#493b5a"
            Rectangle { width: parent.width*view.session.quality.progress; height: parent.height; radius: height/2; color: "#b89aee" }
        }
        Row {
            id: plotControls
            anchors.top: holdProgress.bottom; anchors.topMargin: 16*view.uiScale
            spacing: 8*view.uiScale
            ActionButton { text: qsTr("Front view"); checked: view.displayChannel === "Front"; uiScale: view.uiScale; onClicked: view.displayChannel="Front" }
            ActionButton { text: qsTr("Back view"); checked: view.displayChannel === "Back"; uiScale: view.uiScale; onClicked: view.displayChannel="Back" }
            ActionButton { text: qsTr("Waveform"); checked: !view.showRmsTrend; uiScale: view.uiScale; onClicked: view.showRmsTrend=false }
            ActionButton { text: qsTr("RMS trend"); checked: view.showRmsTrend; uiScale: view.uiScale; onClicked: view.showRmsTrend=true }
            Text { text: qsTr("Target %1 dBFS · capture continues until Finish").arg(view.measurementTarget); anchors.verticalCenter: parent.verticalCenter
                color: "#b9afc8"; font.pixelSize: Math.round(19*view.uiScale) }
        }
        DiagnosticPlot {
            id: waveformPlot
            anchors.left: parent.left; anchors.bottom: parent.bottom
            anchors.top: plotControls.bottom; anchors.topMargin: 12*view.uiScale
            width: (parent.width-16*view.uiScale)/2; uiScale: view.uiScale
            minimum: view.showRmsTrend ? -120 : -1; maximum: view.showRmsTrend ? 0 : 1
            xLabel: view.showRmsTrend ? qsTr("Observations · RMS dBFS") : qsTr("Seconds · normalized PCM")
            xStep: { var c=view.channel(view.displayChannel); return c && !view.showRmsTrend ? 1/c.sampleRate : 1 }
            series: { var c=view.channel(view.displayChannel); return c && !c.stale ? [{label:view.displayChannel,values:view.showRmsTrend ? c.history : c.samples}] : [] }
        }
        DiagnosticPlot {
            anchors.left: waveformPlot.right; anchors.leftMargin: 16*view.uiScale
            anchors.right: parent.right; anchors.top: waveformPlot.top; anchors.bottom: parent.bottom
            uiScale: view.uiScale
            minimum: -120; maximum: 0; xLabel: qsTr("Frequency (Hz) · peak dBFS")
            xStep: { var c=view.channel(view.displayChannel); return c && c.spectrum ? c.spectrum.binHz : 1 }
            series: { var c=view.channel(view.displayChannel); return c && c.spectrum && !c.stale ? [{label:"dBFS",values:c.spectrum.values}] : [] }
        }
    }
    Item {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Diagnostics"
        Column {
            spacing: 26*view.uiScale
            Text { text: qsTr("Measurement quality"); color: "#f6efff"; font.pixelSize: Math.round(30*view.uiScale); font.weight: Font.DemiBold }
            Repeater {
                model: ["Front","Back"]
                delegate: Text {
                    required property string modelData
                    text: { var c=view.channel(modelData); return modelData+": "+(c ? c.total+" samples · "+c.quality+" · "+view.metric(modelData,"clipping","% clipping") : "Waiting for samples") }
                    color: "#c9bcd7"; font.pixelSize: Math.round(23*view.uiScale)
                }
            }
            Text { text: qsTr("SPL is shown only when a calibrated microphone conversion is supplied.")
                color: "#c9bcd7"; font.pixelSize: Math.round(21*view.uiScale) }
            Text { text: qsTr("Results folder: %1").arg(view.session.outputDirectory); color: "#b8abc7"; font.pixelSize: Math.round(19*view.uiScale) }
            ActionButton { text: qsTr("Export measurement summary"); uiScale: view.uiScale
                enabled: !view.session.busy && Object.keys(view.session.microphones).length>0; onClicked: view.session.exportMicrophoneSummary() }
        }
    }
}
