pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: panel
    property var plot: ({})
    property real uiScale: 1
    readonly property var metricsData: plot.analysis || ({})
    readonly property var distortion: metricsData.distortion || ({})
    readonly property var trigger: metricsData.trigger || ({})
    function format(value, suffix) { return value === undefined || value === null || !isFinite(value) ? "—" : value.toFixed(2) + " " + suffix }
    Flow {
        id: metrics
        width: parent.width; spacing: 12 * panel.uiScale
        Repeater { model: [
            {label:(panel.metricsData.weighting || "Z")+" · "+(panel.metricsData.timeWeighting || "Fast"),value:panel.format(panel.metricsData.level,"dB")},
            {label:"Equivalent level",value:panel.format(panel.metricsData.leq,"dB")},
            {label:"Maximum Fast",value:panel.format(panel.metricsData.maxFast,"dB")},
            {label:"THD",value:panel.format(panel.distortion.thdPercent,"%")},
            {label:"THD + N",value:panel.format(panel.distortion.thdnPercent,"%")},
            {label:"SINAD",value:panel.format(panel.distortion.sinadDb,"dB")}]
            delegate: Rectangle {
                required property var modelData
                width: Math.max(120 * panel.uiScale,(metrics.width-60*panel.uiScale)/6); height: 85 * panel.uiScale
                color: "#292137"; radius: 10 * panel.uiScale
                Text { x: 12 * panel.uiScale; y: 10 * panel.uiScale; text: parent.modelData.label; color: "#bfb0cf"; font.pixelSize: Math.round(15 * panel.uiScale) }
                Text { x: 12 * panel.uiScale; y: 35 * panel.uiScale; text: parent.modelData.value; color: "#80d4ba"; font.pixelSize: Math.round(25 * panel.uiScale) }
            }
        }
    }
    Text { id: note; anchors.top: metrics.bottom; anchors.topMargin: 6 * panel.uiScale; width: parent.width; wrapMode: Text.WordWrap
        text: (panel.metricsData.levelUnit || "") + " · " + (panel.metricsData.distortionError || qsTr("Harmonic fit uses the configured fundamental; only meaningful for a steady tone."))
        color: "#baaecd"; font.pixelSize: Math.round(14 * panel.uiScale) }
    Grid {
        id: plots
        anchors.top: note.bottom; anchors.topMargin: 8 * panel.uiScale; anchors.bottom: parent.bottom; width: parent.width
        columns: 2; spacing: 12 * panel.uiScale
        readonly property real paneWidth: Math.max(1,(width-spacing)/2)
        readonly property real paneHeight: Math.max(1,(height-spacing)/2)
        LanXiBands { width: plots.paneWidth; height: plots.paneHeight; uiScale: panel.uiScale; bands: panel.metricsData.bands || []; unit: panel.metricsData.levelUnit || "dB" }
        LanXiHeatmap { width: plots.paneWidth; height: plots.paneHeight; uiScale: panel.uiScale; rows: panel.metricsData.spectrogram || []; times: panel.metricsData.times || []; maximumHz: panel.metricsData.maximumHz || 0
            floorDb: panel.plot.unit === "Pa" ? 0 : -100; ceilingDb: panel.plot.unit === "Pa" ? 100 : 20 }
        LanXiPlot { width: plots.paneWidth; height: plots.paneHeight; uiScale: panel.uiScale
            title: qsTr("%1-weighted RMS spectrum").arg(panel.metricsData.weighting || "Z"); axis: qsTr("Frequency (Hz)")
            values: panel.metricsData.weightedSpectrum || []; xStep: panel.plot.frequencyStep || 1 }
        LanXiPlot { width: plots.paneWidth; height: plots.paneHeight; uiScale: panel.uiScale
            title: panel.trigger.complete ? qsTr("Captured event · %1 samples").arg(panel.trigger.samples) : qsTr("Event capture · waiting for threshold / post-trigger data")
            values: panel.trigger.values || []; xStart: panel.trigger.start || 0; xStep: panel.trigger.step || 1; axis: qsTr("Seconds from trigger") }
    }
}
