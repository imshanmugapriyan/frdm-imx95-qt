pragma ComponentBehavior: Bound
import QtQuick
import FRDM_IMX95_QT_DSW
import "LiveModel.js" as Live
import "../../../../shared/qml/controls"

FocusScope {
    id: view
    required property var session
    property real uiScale: 1
    property var settings: Live.defaults()
    property int visiblePoints: 1000
    property bool settingsOpen: false
    property bool following: true
    property real historyStart: 0
    property int discardedGeneration: -1
    readonly property var rows: session.liveGeneration>discardedGeneration ? session.rows : []
    readonly property real displayTotal: rows.length ? session.displayTotal : 0
    readonly property var viewport: Live.windowFor(displayTotal,rows.length,visiblePoints,following,historyStart)
    readonly property var enabledElectrodes: Live.enabled(settings)
    readonly property int enabledCount: enabledElectrodes.filter(function(value) { return value }).length
    readonly property bool liveBusy: session.busy && session.mode==="live-electrodogram"
    property date clockNow: new Date()

    function changeSource(source) {
        if (session.busy || settingsOpen || settings.plotSource===source) return
        var next=Live.copy(settings); next.plotSource=source; settings=next
        // Never re-label an old STIM snapshot as ASK (or vice versa).
        discardedGeneration=session.liveGeneration; following=true; historyStart=0
    }
    function toggleElectrode(index) {
        var next=Live.copy(settings); next.electrodes=Live.enabled(settings)
        next.selectionMode="custom"; next.electrodes[index]=!next.electrodes[index]; settings=next
    }
    function selectAll(value) {
        var next=Live.copy(settings)
        if (value) next.selectionMode="all"
        else { next.selectionMode="custom"; next.electrodes=Array(16).fill(false) }
        settings=next
    }
    function seek(start) {
        historyStart=Math.max(viewport.oldest,Math.min(viewport.latest,Math.round(start)))
        following=historyStart>=viewport.latest
    }
    function setPoints(value) {
        visiblePoints=Live.points(value)
        historyStart=viewport.start
    }
    function applySettings(next) {
        try {
            Live.validate(next)
            if (session.busy && JSON.stringify(Live.captureSettings(next))!==JSON.stringify(Live.captureSettings(settings)))
                throw new Error(qsTr("Stop capture before changing signal settings."))
            settings=Live.copy(next); settingsOpen=false
        } catch (error) { session.feedback=error.message }
    }
    function startCapture() {
        try {
            var request=Live.captureSettings(settings)
            following=true; historyStart=0
            session.start("live-electrodogram",request)
        } catch (error) { session.feedback=error.message }
    }
    Keys.onEscapePressed: function(event) {
        if (settingsOpen) { settingsOpen=false; event.accepted=true }
        else event.accepted=false
    }
    Timer { interval: 1000; repeat: true; running: view.visible; onTriggered: view.clockNow=new Date() }

    Item {
        id: toolbar
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 48*view.uiScale
        Row {
            id: captureActions
            spacing: 8*view.uiScale
            ActionButton { text: qsTr("Start capture"); primary: true; uiScale: view.uiScale
                enabled: !view.session.busy && !view.settingsOpen && view.session.available("live-electrodogram")
                onClicked: view.startCapture() }
            ActionButton { text: qsTr("Stop"); width: 90*view.uiScale; uiScale: view.uiScale
                enabled: view.liveBusy && ["starting","running"].indexOf(view.session.phase)>=0
                onClicked: view.session.finish() }
            ActionButton { text: view.session.paused ? qsTr("Resume view") : qsTr("Pause view"); uiScale: view.uiScale
                enabled: view.rows.length>0; checked: view.session.paused; onClicked: view.session.togglePause() }
            ActionButton { text: view.session.recording ? qsTr("Stop recording") : qsTr("Record CSV"); uiScale: view.uiScale
                checked: view.session.recording; enabled: view.liveBusy && view.session.phase==="running"
                onClicked: view.session.toggleRecording() }
            Rectangle { width: 1; height: 32*view.uiScale; color: "#4b425c"; anchors.verticalCenter: parent.verticalCenter }
            ActionButton { text: qsTr("STIM E-Value"); uiScale: view.uiScale; checked: view.settings.plotSource==="STIM_EVALUE"
                enabled: !view.session.busy && !view.settingsOpen; onClicked: view.changeSource("STIM_EVALUE") }
            ActionButton { text: qsTr("ASK E-Value"); uiScale: view.uiScale; checked: view.settings.plotSource==="ASK_EVALUE"
                enabled: !view.session.busy && !view.settingsOpen; onClicked: view.changeSource("ASK_EVALUE") }
        }
        Text {
            anchors.left: captureActions.right; anchors.right: settingsButton.left
            anchors.margins: 18*view.uiScale; anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight; elide: Text.ElideRight
            color: view.liveBusy ? "#7de0bb" : "#b5a9c8"; font.pixelSize: Math.round(16*view.uiScale)
            text: view.liveBusy ? (view.session.phase==="running" ? qsTr("● Capturing") : view.session.phase==="starting" ? qsTr("Starting…") : qsTr("Stopping…"))
                : view.session.available("live-electrodogram") ? qsTr("Ready") : qsTr("Acquisition offline")
        }
        ActionButton {
            id: settingsButton
            anchors.right: parent.right; text: qsTr("Settings"); uiScale: view.uiScale; checked: view.settingsOpen
            onClicked: {
                if (view.settingsOpen) view.settingsOpen=false
                else { settingsPanel.begin(view.settings); view.settingsOpen=true }
            }
        }
    }
    LiveSettingsPanel {
        id: settingsPanel
        anchors.top: toolbar.bottom; anchors.bottom: parent.bottom; anchors.right: parent.right
        anchors.topMargin: 12*view.uiScale; width: 480*view.uiScale
        visible: view.settingsOpen; uiScale: view.uiScale; acquisitionBusy: view.session.busy
        onApplied: function(settings) { view.applySettings(settings) }
        onCancelled: view.settingsOpen=false
    }
    Item {
        id: graphArea
        anchors.left: parent.left; anchors.right: view.settingsOpen ? settingsPanel.left : parent.right
        anchors.rightMargin: view.settingsOpen ? 12*view.uiScale : 0
        anchors.top: toolbar.bottom; anchors.topMargin: 12*view.uiScale; anchors.bottom: parent.bottom

        Rectangle {
            id: graphFrame
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.bottom: channels.top; anchors.bottomMargin: 10*view.uiScale
            color: "#1b1627"; border.color: "#4d3e5e"; radius: 12*view.uiScale
            Item {
                id: graphHeading
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 14*view.uiScale; height: 66*view.uiScale
                Column {
                    anchors.left: parent.left; anchors.right: pointControls.left
                    anchors.rightMargin: 20*view.uiScale; anchors.verticalCenter: parent.verticalCenter; spacing: 5*view.uiScale
                    Text { width: parent.width; elide: Text.ElideRight; text: qsTr("Electrode amplitude"); color: "#eee6fa"
                        font.pixelSize: Math.round(22*view.uiScale); font.weight: Font.DemiBold }
                    Text { width: parent.width; elide: Text.ElideRight; color: "#afa3bf"; font.pixelSize: Math.round(15*view.uiScale)
                        text: Live.sourceLabel(view.settings.plotSource)+"  ·  "+qsTr("%1 / 16 electrodes · Normalized 0–1").arg(view.enabledCount) }
                }
                Row {
                    id: pointControls
                    anchors.right: parent.right; spacing: 8*view.uiScale
                    LiveNumberField { label: qsTr("Visible points"); value: view.visiblePoints; minimum: 10; maximum: 8000; step: 10
                        width: 140*view.uiScale; uiScale: view.uiScale; onEdited: function(value) { view.setPoints(value) } }
                    ActionButton { text: qsTr("Zoom −"); width: 100*view.uiScale; height: 40*view.uiScale; anchors.bottom: parent.bottom
                        uiScale: view.uiScale; enabled: view.visiblePoints<8000; onClicked: view.setPoints(Live.zoom(view.visiblePoints,1)) }
                    ActionButton { text: qsTr("Zoom +"); width: 100*view.uiScale; height: 40*view.uiScale; anchors.bottom: parent.bottom
                        uiScale: view.uiScale; enabled: view.visiblePoints>10; onClicked: view.setPoints(Live.zoom(view.visiblePoints,-1)) }
                }
            }
            DiagnosticPlot {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: graphHeading.bottom; anchors.bottom: parent.bottom
                anchors.margins: 8*view.uiScale
                uiScale: view.uiScale; stacked: true; laneScales: true; integerTicks: true; minimum: 0; maximum: 1
                xStart: view.viewport.start; xLabel: qsTr("Data points")
                series: Live.series(view.rows,view.enabledElectrodes,view.viewport.offset,view.visiblePoints)
                emptyText: view.enabledCount===0 ? qsTr("Select electrodes to display")
                    : view.rows.length===0 && view.discardedGeneration>=0 ? qsTr("Start a new capture for this source") : qsTr("Waiting for measurement data")
            }
        }
        Row {
            id: channels
            anchors.left: parent.left; anchors.bottom: history.top; anchors.bottomMargin: 10*view.uiScale
            spacing: 5*view.uiScale; enabled: !view.settingsOpen
            Text { width: 86*view.uiScale; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Electrodes")
                color: "#bfb2d0"; font.pixelSize: Math.round(15*view.uiScale) }
            Repeater {
                model: 16
                delegate: ActionButton {
                    id: channelButton
                    required property int index
                    text: "E"+(index+1); width: 53*view.uiScale; height: 36*view.uiScale; uiScale: view.uiScale
                    checked: view.enabledElectrodes[index]; Accessible.name: qsTr("Electrode %1").arg(index+1)
                    onClicked: view.toggleElectrode(index)
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                        anchors.margins: 5*view.uiScale; height: 2*view.uiScale; color: Live.channelColor(channelButton.index) }
                }
            }
            ActionButton { text: qsTr("All"); width: 70*view.uiScale; height: 36*view.uiScale; uiScale: view.uiScale; onClicked: view.selectAll(true) }
            ActionButton { text: qsTr("None"); width: 70*view.uiScale; height: 36*view.uiScale; uiScale: view.uiScale; onClicked: view.selectAll(false) }
        }
        Item {
            id: history
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: summary.top; anchors.bottomMargin: 10*view.uiScale
            height: 42*view.uiScale
            Row {
                id: historyActions
                spacing: 7*view.uiScale
                ActionButton { text: qsTr("Earlier"); height: history.height; uiScale: view.uiScale
                    enabled: view.viewport.start>view.viewport.oldest
                    onClicked: view.seek(view.viewport.start-Math.max(10,Math.floor(view.visiblePoints/2))) }
                ActionButton { text: qsTr("Later"); height: history.height; uiScale: view.uiScale
                    enabled: view.viewport.start<view.viewport.latest
                    onClicked: view.seek(view.viewport.start+Math.max(10,Math.floor(view.visiblePoints/2))) }
                ActionButton { text: qsTr("Follow latest"); height: history.height; uiScale: view.uiScale; checked: view.following
                    onClicked: view.following=true }
            }
            Rectangle {
                id: scrubber
                anchors.left: historyActions.right; anchors.right: parent.right; anchors.leftMargin: 16*view.uiScale
                anchors.verticalCenter: parent.verticalCenter; height: 24*view.uiScale; radius: height/2; color: "#292336"
                property real span: view.viewport.latest-view.viewport.oldest
                property real handleWidth: Math.max(32*view.uiScale,width*Math.min(1,view.visiblePoints/Math.max(1,view.rows.length)))
                enabled: span>0; activeFocusOnTab: true
                Accessible.role: Accessible.Slider; Accessible.name: qsTr("History position")
                function moveAt(x) {
                    if (width>handleWidth) view.seek(view.viewport.oldest+Math.max(0,Math.min(1,x/(width-handleWidth)))*span)
                }
                Keys.onLeftPressed: view.seek(view.viewport.start-10)
                Keys.onRightPressed: view.seek(view.viewport.start+10)
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Home) {
                        view.seek(view.viewport.oldest)
                        event.accepted=true
                    } else if (event.key === Qt.Key_End) {
                        view.following=true
                        event.accepted=true
                    } else event.accepted=false
                }
                Rectangle {
                    id: historyThumb
                    width: scrubber.handleWidth; height: parent.height; radius: height/2
                    x: scrubber.span>0 ? (parent.width-width)*(view.viewport.start-view.viewport.oldest)/scrubber.span : 0
                    color: scrubber.activeFocus ? "#ddcaff" : "#8f78b0"
                }
                MouseArea {
                    property real grabOffset: 0
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onPressed: function(mouse) {
                        grabOffset=mouse.x>=historyThumb.x && mouse.x<=historyThumb.x+historyThumb.width
                            ? mouse.x-historyThumb.x : historyThumb.width/2
                        scrubber.forceActiveFocus(); scrubber.moveAt(mouse.x-grabOffset)
                    }
                    onPositionChanged: function(mouse) { if (pressed) scrubber.moveAt(mouse.x-grabOffset) }
                    onWheel: function(wheel) {
                        var delta=wheel.angleDelta.y || wheel.angleDelta.x
                        if (delta) view.seek(view.viewport.start-(delta>0 ? 1 : -1)*Math.max(10,Math.floor(view.visiblePoints/5)))
                        wheel.accepted=true
                    }
                }
            }
        }
        Item {
            id: summary
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 24*view.uiScale
            Text {
                anchors.left: parent.left; anchors.right: timestamps.left; anchors.rightMargin: 16*view.uiScale
                font.pixelSize: Math.round(14*view.uiScale); color: view.session.paused ? "#e5bd86" : "#afa3bf"; elide: Text.ElideRight
                text: {
                    view.session.revision
                    var range=view.rows.length ? qsTr("Rows %1–%2 · %3 buffered").arg(view.viewport.start+1).arg(view.viewport.start+view.viewport.count).arg(view.rows.length) : qsTr("No captured rows")
                    var state=view.session.paused ? qsTr(" · View paused") : !view.following ? qsTr(" · History") : ""
                    return range+state+(view.rows.length && view.session.mode==="live-electrodogram" ? qsTr(" · %1 missing").arg(view.session.model.dropped) : "")
                }
            }
            Text {
                id: timestamps
                anchors.right: parent.right; font.pixelSize: Math.round(14*view.uiScale); color: "#afa3bf"
                text: (view.session.liveLastElapsed>=0 && view.rows.length ? qsTr("Last batch: %1 ms").arg(view.session.liveLastElapsed) : qsTr("Last batch: —"))
                    + "   ·   " + Qt.formatDateTime(view.clockNow,"hh:mm:ss")
            }
        }
    }
}
