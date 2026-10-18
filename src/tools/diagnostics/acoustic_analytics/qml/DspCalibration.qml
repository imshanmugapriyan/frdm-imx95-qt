pragma ComponentBehavior: Bound
import QtQuick
import FRDM_IMX95_QT_DSW
import "../../../../services/diagnostics/qml/Analysis.js" as Analysis
import "../../../../shared/qml/controls"

Item {
    id: view
    required property var session
    property real uiScale: 1
    property var settings: Analysis.dspDefaults()
    property string tab: "Setup"
    readonly property var target: Analysis.dspTargets(settings)
    readonly property var observation: session.observation
    function change(name,value) { var next=Analysis.copy(settings); next[name]=value; settings=next }
    function fmt(value,digits) { return Analysis.finite(value) ? value.toFixed(digits) : "--" }

    Row {
        spacing: 8*view.uiScale
        Repeater {
            model: ["Setup","Controller","Channel monitor","Electrodogram","Results","Event log"]
            delegate: ActionButton { required property string modelData; text: modelData; uiScale: view.uiScale
                checked: view.tab===modelData; onClicked: view.tab=modelData }
        }
    }
    Row {
        anchors.right: parent.right; spacing: 10*view.uiScale
        ActionButton { text: qsTr("Start calibration"); primary: true; uiScale: view.uiScale
            enabled: !view.session.busy && view.session.available("dsp-calibration")
            onClicked: view.session.start("dsp-calibration",view.settings) }
        ActionButton { text: qsTr("Stop"); uiScale: view.uiScale
            enabled: view.session.mode === "dsp-calibration" && ["starting","running"].indexOf(view.session.phase)>=0
            onClicked: view.session.finish() }
    }

    Item {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Controller"
        Rectangle { anchors.fill: parent; color: "#231c30"; radius: 16*view.uiScale; border.color: "#4e3d60" }
        Text { x: 32*view.uiScale; y: 26*view.uiScale; text: qsTr("Adaptive stimulus control")
            color: "#f9f1ff"; font.pixelSize: Math.round(28*view.uiScale); font.weight: Font.DemiBold }
        Grid {
            x: 32*view.uiScale; y: 98*view.uiScale; columns: 3; columnSpacing: 55*view.uiScale; rowSpacing: 30*view.uiScale
            enabled: !view.session.busy
            ValueField { label: qsTr("Proportional gain (P)"); value: view.settings.kp; minimum: 0.001; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("kp",v) } }
            ValueField { label: qsTr("Integral gain (I)"); value: view.settings.ki; minimum: 0; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("ki",v) } }
            ValueField { label: qsTr("Derivative gain (D)"); value: view.settings.kd; minimum: 0; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("kd",v) } }
            ValueField { label: qsTr("Minimum correction (dB)"); value: view.settings.minimumCorrection; minimum: 0.001; maximum: view.settings.maximumCorrection; uiScale: view.uiScale; onEdited: function(v) { view.change("minimumCorrection",v) } }
            ValueField { label: qsTr("Maximum correction (dB)"); value: view.settings.maximumCorrection; minimum: view.settings.minimumCorrection; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("maximumCorrection",v) } }
            ValueField { label: qsTr("Integral limit (dB)"); value: view.settings.integralLimit; minimum: 0; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("integralLimit",v) } }
            ValueField { label: qsTr("Minimum stimulus (dB)"); value: view.settings.minimumLevel; minimum: 0; maximum: view.settings.maximumLevel-0.01; uiScale: view.uiScale; onEdited: function(v) { view.change("minimumLevel",v) } }
            ValueField { label: qsTr("Maximum stimulus (dB)"); value: view.settings.maximumLevel; minimum: view.settings.minimumLevel+0.01; maximum: 160; uiScale: view.uiScale; onEdited: function(v) { view.change("maximumLevel",v) } }
            ValueField { label: qsTr("Iterations (0 = unlimited)"); value: view.settings.maxIterations; minimum: 0; maximum: 10000; uiScale: view.uiScale; onEdited: function(v) { view.change("maxIterations",Math.round(v)) } }
        }
        Text { x: 1050*view.uiScale; y: 100*view.uiScale; width: 650*view.uiScale; wrapMode: Text.WordWrap
            color: "#c2b5cf"; font.pixelSize: Math.round(22*view.uiScale)
            text: qsTr("The controller uses electrical Y error to adjust the stimulus. Once measurements bracket the target, interpolation refines the level.\n\nCorrections and output levels stay within these limits. Each band keeps its own controller history.\n\nSave these settings with the profile on the Setup page.") }
    }

    Item {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y
        visible: view.tab === "Setup"
        Rectangle { anchors.fill: parent; color: "#231c30"; radius: 16*view.uiScale; border.color: "#4e3d60" }
        Text { x: 32*view.uiScale; y: 26*view.uiScale; text: qsTr("Targets and convergence")
            color: "#f9f1ff"; font.pixelSize: Math.round(28*view.uiScale); font.weight: Font.DemiBold }
        Grid {
            x: 32*view.uiScale; y: 98*view.uiScale; columns: 3; columnSpacing: 30*view.uiScale; rowSpacing: 30*view.uiScale
            enabled: !view.session.busy
            ValueField { label: qsTr("MCL (Y units)"); value: view.settings.mcl; minimum: view.settings.thr+0.01; maximum: 5000; uiScale: view.uiScale; onEdited: function(v) { view.change("mcl",v) } }
            ValueField { label: qsTr("THR (Y units)"); value: view.settings.thr; minimum: 1; maximum: Math.min(1000,view.settings.mcl-0.01); uiScale: view.uiScale; onEdited: function(v) { view.change("thr",v) } }
            ValueField { label: qsTr("Maximum Y error"); value: view.settings.maxError; minimum: 0; maximum: 100; uiScale: view.uiScale; onEdited: function(v) { view.change("maxError",v) } }
            ValueField { label: qsTr("Averaging time (s)"); value: view.settings.averagingSeconds; minimum: 0.01; maximum: 60; uiScale: view.uiScale; onEdited: function(v) { view.change("averagingSeconds",v) } }
            ValueField { label: qsTr("Samples per frame"); value: view.settings.samplesPerFrame; minimum: 1; maximum: 8192; uiScale: view.uiScale; onEdited: function(v) { view.change("samplesPerFrame",Math.round(v)) } }
            ValueField { label: qsTr("Reference sample rate (Hz)"); value: view.settings.referenceSampleRate; minimum: 1000; maximum: 192000; uiScale: view.uiScale; onEdited: function(v) { view.change("referenceSampleRate",v) } }
        }
        ChoiceField { x: 32*view.uiScale; y: 335*view.uiScale; label: qsTr("Input dynamic range (dB)"); options: [20,30,40,50,60,70,80]
            value: view.settings.idr; uiScale: view.uiScale; enabled: !view.session.busy; onSelected: function(v) { view.change("idr",v) } }
        ChoiceField { x: 32*view.uiScale; y: 450*view.uiScale; label: qsTr("Stimulus"); options: ["Sine tone","1/3-octave noise"]
            value: view.settings.stimulus; uiScale: view.uiScale; enabled: !view.session.busy; onSelected: function(v) { view.change("stimulus",v) } }
        Row {
            x: 32*view.uiScale; y: 586*view.uiScale; spacing: 12*view.uiScale
            ActionButton { text: qsTr("Restore defaults"); uiScale: view.uiScale; enabled: !view.session.busy; onClicked: view.settings=Analysis.dspDefaults() }
            ActionButton { text: qsTr("Save profile"); uiScale: view.uiScale; enabled: !view.session.busy; onClicked: view.session.saveProfile("dsp",view.settings) }
            ActionButton { text: qsTr("Load profile"); uiScale: view.uiScale; enabled: !view.session.busy; onClicked: { var p=view.session.loadProfile("dsp"); if(p) view.settings=p } }
        }
        Column {
            x: 1000*view.uiScale; y: 100*view.uiScale; spacing: 20*view.uiScale
            Metric { label: qsTr("TARGET Y"); value: view.fmt(view.target.y,3); width: 660*view.uiScale; uiScale: view.uiScale }
            Metric { label: qsTr("MIDPOINT SPL"); value: view.fmt(view.target.spl,1)+" dB SPL"; width: 660*view.uiScale; uiScale: view.uiScale }
            Text { width: 630*view.uiScale; wrapMode: Text.WordWrap; color: "#c2b5cf"; font.pixelSize: Math.round(20*view.uiScale)
                text: qsTr("Calibrate each active band against the reference microphone. Review the channel response, electrodogram and converged results during the run.") }
            Text { width: 630*view.uiScale; wrapMode: Text.WordWrap; color: "#ddc590"; font.pixelSize: Math.round(19*view.uiScale)
                text: view.session.available("dsp-calibration") ? qsTr("Acquisition is available.") : qsTr("Waiting for the acquisition service. Settings can be prepared before connecting.") }
        }
    }

    Item {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Channel monitor"
        Row {
            id: monitorMetrics
            width: parent.width; spacing: 16*view.uiScale
            readonly property real cardWidth: (width-4*spacing)/5
            Metric { label: qsTr("BAND"); value: view.observation ? String(view.observation.band) : "--"; width: monitorMetrics.cardWidth; uiScale: view.uiScale }
            Metric { label: qsTr("FREQUENCY"); value: view.observation ? view.fmt(view.observation.frequencyHz,0)+" Hz" : "--"; width: monitorMetrics.cardWidth; uiScale: view.uiScale }
            Metric { label: qsTr("MEASURED Y"); value: view.observation ? view.fmt(view.observation.measuredY,3) : "--"; width: monitorMetrics.cardWidth; uiScale: view.uiScale }
            Metric { label: qsTr("MICROPHONE SPL"); value: view.observation ? view.fmt(view.observation.measuredSpl,2)+" dB" : "--"; width: monitorMetrics.cardWidth; uiScale: view.uiScale }
            Metric { label: qsTr("ABS. Y ERROR"); value: view.observation ? view.fmt(view.observation.error,3) : "--"; width: monitorMetrics.cardWidth; uiScale: view.uiScale }
        }
        Item {
            id: monitorPlots
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: monitorMetrics.bottom; anchors.topMargin: 12*view.uiScale
            anchors.bottom: iterationStatus.top; anchors.bottomMargin: 8*view.uiScale
            DiagnosticPlot {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                height: (parent.height-12*view.uiScale)/2; uiScale: view.uiScale
                minimum: 0
                maximum: {
                    view.session.revision
                    return view.session.model.dspConfig.mcl
                }
                xLabel: qsTr("Observations · electrical Y")
                series: { view.session.revision; return [{label:"Y",values:view.session.model.monitorY || []}] }
            }
            DiagnosticPlot {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: (parent.height-12*view.uiScale)/2; uiScale: view.uiScale
                minimum: 20; maximum: 120; xLabel: qsTr("Observations · microphone dB SPL")
                series: { view.session.revision; return [{label:"SPL",values:view.session.model.monitorSpl || []}] }
            }
        }
        Text {
            id: iterationStatus
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 26*view.uiScale; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            text: view.observation ? qsTr("Iteration %1 · Next stimulus: %2")
                .arg(view.observation.iteration).arg(view.observation.nextLevelDb===null ? "--" : view.fmt(view.observation.nextLevelDb,3)+" dB") : ""
            color: "#cfbadf"; font.pixelSize: Math.round(18*view.uiScale) }
    }
    DiagnosticPlot {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Electrodogram"
        uiScale: view.uiScale; stacked: true; minimum: 0; maximum: 1; xLabel: qsTr("Data points · normalized electrode amplitude")
        series: { view.session.revision; return Analysis.seriesFromRows(view.session.model.dspRows || [],Array(16).fill(true),0,1000) }
    }
    Item {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Results"
        ActionButton { text: qsTr("Export CSV"); uiScale: view.uiScale; enabled: view.session.results.length>0; onClicked: view.session.exportResults() }
        Row {
            y: 77*view.uiScale; spacing: 20*view.uiScale
            Repeater {
                model: [qsTr("Band"),qsTr("Frequency (Hz)"),qsTr("Measured Y"),qsTr("Measured SPL"),qsTr("EQ coefficient"),qsTr("Error (%)")]
                delegate: Text { required property string modelData; text: modelData; width: 235*view.uiScale
                    color: "#bfaecc"; font.pixelSize: Math.round(20*view.uiScale) }
            }
        }
        Column {
            y: 126*view.uiScale; spacing: 9*view.uiScale
            Repeater {
                model: view.session.results
                delegate: Row {
                    id: resultRow
                    required property var modelData
                    spacing: 20*view.uiScale
                    Repeater {
                        model: [String(resultRow.modelData.band),view.fmt(resultRow.modelData.frequencyHz,0),view.fmt(resultRow.modelData.measuredY,3),view.fmt(resultRow.modelData.measuredSpl,2),view.fmt(resultRow.modelData.coefficient,6),view.fmt(resultRow.modelData.percentError,3)]
                        delegate: Text { required property string modelData; text: modelData; width: 235*view.uiScale; color: "#eee4fb"; font.pixelSize: Math.round(20*view.uiScale) }
                    }
                }
            }
        }
        Text { y: 180*view.uiScale; visible: view.session.results.length===0; text: qsTr("Converged band results will appear here."); color: "#bfaecc"; font.pixelSize: Math.round(22*view.uiScale) }
    }
    Flickable {
        y: 60*view.uiScale; width: parent.width; height: parent.height-y; visible: view.tab === "Event log"
        contentHeight: logText.height; clip: true
        Text { id: logText; width: parent.width; text: view.session.events.length ? view.session.events.join("\n\n") : qsTr("No session events.")
            wrapMode: Text.WordWrap; color: "#d8cce5"; font.pixelSize: Math.round(20*view.uiScale) }
    }
}
