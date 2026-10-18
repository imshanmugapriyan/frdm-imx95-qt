pragma ComponentBehavior: Bound
import QtQuick
import FRDM_IMX95_QT_DSW
import "Analysis.js" as Analysis
import "SessionModel.js" as Core

Item {
    id: session
    objectName: "diagnosticSession"
    visible: false
    property var model: new Core.Session()
    property int revision: 0
    property bool recording: false
    property string recordingError: ""
    property string feedback: "Acquisition is not connected."
    property real commandStartedAt: 0
    property var backendModes: ({m7: [], lanxi: []})
    property alias lanxi: lanxiBackend
    readonly property string phase: { revision; return model.busy() ? model.phase : lanxiBackend.busy ? lanxiBackend.state.phase : model.phase }
    readonly property bool busy: { revision; return model.busy() || lanxiBackend.busy }
    readonly property string mode: { revision; return model.mode }
    readonly property var events: { revision; return model.events.slice() }
    readonly property var results: { revision; return model.results.slice() }
    readonly property var observation: { revision; return model.observation }
    readonly property var rows: { revision; return model.displayRows }
    readonly property real displayTotal: { revision; return model.displayTotal }
    readonly property int liveGeneration: { revision; return model.liveGeneration }
    readonly property var liveConfig: { revision; return model.liveConfig }
    readonly property real liveLastElapsed: { revision; return model.liveLastElapsed }
    readonly property bool paused: { revision; return model.paused }
    readonly property var microphones: { revision; return model.microphones }
    readonly property var quality: { revision; return model.micQuality() }
    readonly property string outputDirectory: files.outputDirectory
    signal startRequested(int ticket, string mode, var settings)
    signal stopRequested(int ticket)
    signal calibrationEvaluated(int ticket, var evaluation)

    DiagnosticFiles { id: files }
    LanXiBackend {
        id: lanxiBackend
        onChanged: {
            var next = ready ? ["lanxi-measurement"] : []
            if (next.join() !== session.backendModes.lanxi.join()) session.setBackendModes("lanxi", next)
            if (session.model.mode === "lanxi-measurement" || busy) session.feedback = state.message || ""
        }
        onRunStarted: function(ticket) { session.started(ticket) }
        onRunEnded: function(ticket, success, message) {
            if (success) session.stopped(ticket)
            else session.fail(ticket, message)
            session.feedback = message
        }
        onRecoveryRequired: function(ticket, message) {
            if (ticket !== session.model.ticket || !session.model.busy()) return
            if (session.model.phase !== "stopping") session.model.stop()
            session.feedback = message
            ++session.revision
        }
    }
    Timer {
        interval: 250
        running: session.busy
        repeat: true
        onTriggered: {
            var now=files.monotonicMs()
            if (session.model.expire(now)) ++session.revision
            if (session.mode !== "lanxi-measurement" && (session.phase === "starting" || session.phase === "stopping") && now-session.commandStartedAt>10000)
                session.feedback=qsTr("Acquisition has not acknowledged the request. Waiting for the service.")
        }
    }
    function available(toolMode) { revision; return model.availableModes.indexOf(toolMode) >= 0 }
    function setAvailableModes(modes) {
        // Existing M7 adapters report only their own capabilities.
        setBackendModes("m7", modes)
    }
    function setBackendModes(backend, modes) {
        backendModes[backend] = modes.slice()
        var combined = []
        Object.keys(backendModes).forEach(function(key) {
            backendModes[key].forEach(function(value) { if (combined.indexOf(value) < 0) combined.push(value) })
        })
        model.availableModes = combined
        if (model.busy() && mode !== "lanxi-measurement" && combined.indexOf(mode) < 0) rejectData(model.ticket,"Acquisition disconnected.")
        if (!busy) feedback=combined.length ? qsTr("Acquisition available.") : qsTr("Acquisition is not connected.")
        ++revision
    }
    function start(toolMode, settings) {
        try {
            if (lanxiBackend.busy) throw new Error("Wait for the LAN-XI operation to finish.")
            var request=model.start(toolMode,settings)
            recordingError=""
            commandStartedAt=files.monotonicMs()
            feedback=model.message; ++revision
            if (toolMode === "lanxi-measurement") lanxiBackend.start(request.ticket, request.settings)
            else startRequested(request.ticket,request.mode,request.settings)
        } catch (error) { feedback=error.message }
    }
    function started(ticket) { if (model.started(ticket)) { feedback=model.message; ++revision } }
    function finish() {
        if (lanxiBackend.busy && (!model.busy() || model.mode === "lanxi-measurement")) {
            if (model.busy() && model.phase !== "stopping") model.stop()
            commandStartedAt=files.monotonicMs(); ++revision
            lanxiBackend.stop()
            return
        }
        try { var ticket=model.stop(); commandStartedAt=files.monotonicMs(); feedback=model.message; ++revision; stopRequested(ticket) }
        catch (error) { feedback=error.message }
    }
    function stopped(ticket) {
        if (!model.stopped(ticket)) return
        var flushed=stopRecording(); if (flushed) feedback=model.message; ++revision
    }
    function fail(ticket, message) {
        if (!model.failed(ticket,message)) return
        var flushed=stopRecording(); feedback=message+(flushed ? "" : " "+files.error); ++revision
    }
    function rejectData(ticket, message) {
        // Do not release hardware ownership just because a malformed frame arrived.
        // The adapter must acknowledge that acquisition has stopped first.
        if (ticket !== model.ticket || !busy) return
        if (phase !== "stopping") finish()
        feedback=message+" Acquisition stop requested."
    }
    function receiveElectrodogram(ticket, batch) {
        try {
            if (!model.acceptRows(ticket,batch)) return
            if (recording) {
                var csv=""
                for (var i=0;i<batch.rows.length;++i)
                    csv+=(batch.sequence+i)+","+batch.elapsedMs+","+batch.rows[i].map(function(v){return v===null ? "" : String(v)}).join(",")+"\n"
                if (!files.appendRecording(csv)) { recording=false; recordingError=files.error; feedback=recordingError }
            }
            ++revision
        } catch (error) { rejectData(ticket,error.message) }
    }
    function receiveDsp(ticket, observation) {
        try {
            if (model.acceptDsp(ticket,observation)) {
                var message=model.message, evaluation=model.observation
                ++revision
                if (evaluation.iterationLimit && phase === "running") finish()
                feedback=message; calibrationEvaluated(ticket,evaluation)
            }
        } catch (error) { rejectData(ticket,error.message) }
    }
    function receiveMicrophone(ticket, block) {
        try { if (model.acceptMic(ticket,block,files.monotonicMs())) ++revision }
        catch (error) { rejectData(ticket,error.message) }
    }
    function togglePause() { model.pause(!model.paused); ++revision }
    function toggleRecording() {
        if (recording) { stopRecording(); return }
        if (phase !== "running" || mode !== "live-electrodogram") return
        var path=files.beginRecording()
        if (path.length) recordingError=""
        recording=path.length>0; feedback=recording ? "Recording: "+path : files.error
    }
    function stopRecording() {
        var ok=files.finishRecording()
        if (!ok) recordingError=files.error
        if (recordingError.length) feedback="Recording error: "+recordingError
        recording=false
        return ok && !recordingError.length
    }
    function saveProfile(name, settings) {
        try {
            if (name === "dsp") Analysis.validateDsp(settings); else Analysis.validateMic(settings)
            feedback=files.saveProfile(name,settings) ? "Profile saved." : files.error
        } catch (error) { feedback=error.message }
    }
    function loadProfile(name) {
        var settings=files.loadProfile(name)
        try {
            if (files.error.length) throw new Error(files.error)
            if (name === "dsp") Analysis.validateDsp(settings); else Analysis.validateMic(settings)
            feedback="Profile loaded."; return settings
        } catch (error) { feedback=error.message; return null }
    }
    function exportResults() {
        var csv="band,frequency_hz,measured_y,measured_spl,target_y,target_spl,eq_coefficient,error_percent,iterations,applied_stimulus_db\n"
        model.results.forEach(function(r) { csv+=[r.band,r.frequencyHz,r.measuredY,r.measuredSpl,r.targetY,r.targetSpl,r.coefficient,r.percentError,r.iteration,r.appliedLevelDb===null ? "" : r.appliedLevelDb].join(",")+"\n" })
        if (!model.results.length) return
        var path=files.exportCsv("DspCalibration",csv); feedback=path.length ? "Saved: "+path : files.error
    }
    function exportMicrophoneSummary() {
        var csv="channel,samples,sample_rate_hz,rms_dbfs,spl_db,peak_hz,clipping_percent,target_dbfs,tolerance_db,hold_seconds,stale,target_achieved\n"
        Object.keys(model.microphones).forEach(function(name) {
            var c=model.microphones[name]
            if (!c.stats) return
            csv += [name,c.total,c.sampleRate,c.stats.dbfs,c.stats.spl===null ? "" : c.stats.spl,
                c.spectrum.peakHz===null ? "" : c.spectrum.peakHz,c.stats.clipping,
                session.model.micConfig.target,session.model.micConfig.tolerance,session.model.micConfig.holdSeconds,
                c.stale,c.hold.achieved && !c.stale].join(",")+"\n"
        })
        if (!Object.keys(model.microphones).length) return
        var path=files.exportCsv("AutomaticMic",csv); feedback=path.length ? "Saved: "+path : files.error
    }
}
