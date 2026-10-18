pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Item {
    id: tool
    required property var session
    property real uiScale: 1
    readonly property var backend: session.lanxi
    readonly property var status: backend.state
    property var config: ({})
    property string page: "Setup"
    property int device: 0
    property int channel: 1
    property string selectedRun: ""
    property string selectedProfile: ""
    property bool coherenceView: false
    property bool acousticView: false
    property bool inspectorVisible: true
    readonly property bool advancedPage: ["Processing", "Orders", "Structural", "Limits"].indexOf(page) >= 0
    property string reportOperator: ""
    property string reportDut: ""
    property string reportNotes: ""
    property var batchRuns: []
    property string runFilter: ""
    function toggleBatch(id) {var next=batchRuns.slice();var i=next.indexOf(id);if(i<0)next.push(id);else next.splice(i,1);batchRuns=next}
    property real calibratorLevel: 94
    property real calibratorHz: 1000
    readonly property bool configured: config.devices !== undefined
    readonly property var currentDevice: configured ? config.devices[device] : ({channels:[]})
    readonly property var currentChannel: configured ? currentDevice.channels[channel - 1] : ({})
    readonly property var hardware: status.devices && status.devices.length > device ? status.devices[device].info : ({})
    readonly property var plot: status.plot || ({})
    readonly property var referencePlot: status.comparison || ({})
    readonly property bool compatibleComparison: referencePlot.unit === plot.unit && referencePlot.frequencyStep === plot.frequencyStep
    readonly property bool idle: !session.busy
    function copy(value) { return JSON.parse(JSON.stringify(value)) }
    function receiveConfig(value) {
        var next = copy(value)
        delete next.sequence
        if (!next.analysis) next.analysis = {window:"hann",averaging:"linear",averages:8,removeMean:false,weighting:"Z",timeWeighting:"Fast",fraction:3,highPassHz:0,lowPassHz:0,toneHz:1000,harmonics:5}
        if (!next.trigger) next.trigger = {enabled:false,device:0,channel:1,rising:true,level:0,hysteresis:0.01,preSeconds:0.1,postSeconds:0.5}
        config = next
    }
    function edit(path, value) {
        var next = copy(config), target = next
        for (var i = 0; i < path.length - 1; ++i) target = target[path[i]]
        target[path[path.length - 1]] = value
        if (path[0] !== "generator" || path[1] !== "armed") next.generator.armed = false
        config = next
    }
    function editDevice(key, value) { edit(["devices", device, key], value) }
    function editChannel(key, value) { edit(["devices", device, "channels", channel - 1, key], value) }
    function selectChannel(d, c) { device = d; channel = c; backend.selectChannel(d, c) }
    function applySensor(sensor) {
        var next = copy(config), target = next.devices[device].channels[channel - 1]
        var fields = ["unit","sensitivity","sensorSerial","calibrationUtc","calibrationHz","calibrationLevelDb"]
        for (var i=0;i<fields.length;++i) {
            if (sensor[fields[i]] !== undefined) target[fields[i]]=sensor[fields[i]]
            else delete target[fields[i]]
        }
        next.generator.armed = false; config=next
    }
    function options(entries, id, label) {
        return (entries || []).map(function(entry) { return {value:entry[id], label:entry[label]} })
    }
    function workflow(value) {
        var next = copy(config); next.workflow = value; next.generator.armed = false
        for (var d = 0; d < 2; ++d) for (var c = 0; c < 4; ++c) {
            next.devices[d].channels[c].unit = "V"
            next.devices[d].channels[c].sensitivity = 1
            next.devices[d].channels[c].ccld = false
            next.devices[d].channels[c].polarization = false
            delete next.devices[d].channels[c].calibrationUtc
            delete next.devices[d].channels[c].calibrationHz
            delete next.devices[d].channels[c].calibrationLevelDb
        }
        config = next
    }
    Component.onCompleted: receiveConfig(backend.settings)
    onVisibleChanged: {if(!visible)backend.muteAudio()}
    Connections { target: tool.backend; function onSettingsChanged() { tool.receiveConfig(tool.backend.settings) } }

    Rectangle { anchors.fill: parent; color: "#101b26"; z: -1 }
    Column {
        id: commands
        width: parent.width; spacing: 6 * tool.uiScale
        Flow {
            width: parent.width; spacing: 10 * tool.uiScale
            LanXiButton { text: qsTr("Start measurement"); primary: true; uiScale: tool.uiScale
                enabled: tool.idle && tool.backend.ready && tool.configured
                onClicked: { tool.session.start("lanxi-measurement", tool.config); tool.page = "Measure" } }
            LanXiButton { text: qsTr("Stop"); uiScale: tool.uiScale; enabled: tool.session.busy; onClicked: tool.session.finish() }
            LanXiButton { text: qsTr("Retry stop"); uiScale: tool.uiScale; visible: tool.status.phase === "recovery"; onClicked: tool.backend.recover() }
            LanXiButton { text: qsTr("Output off / Stop"); uiScale: tool.uiScale
                enabled: tool.backend.busy && tool.status.phase !== "inspecting" && !tool.status.review; onClicked: tool.backend.outputOff() }
            LanXiButton { text: tool.status.paused ? qsTr("Resume display") : qsTr("Pause display"); uiScale: tool.uiScale
                onClicked: tool.backend.pauseDisplay(!tool.status.paused) }
            Text { height: 34 * tool.uiScale; verticalAlignment: Text.AlignVCenter
                text: qsTr("Output: %1  ·  %2").arg(tool.status.output || "Not checked").arg(tool.status.recording ? qsTr("Recording") : tool.status.phase || "")
                color: tool.status.output === "Unknown" ? "#ff9ca9" : "#80d4ba"; font.pixelSize: Math.round(14 * tool.uiScale) }
        }
        Flow {
            width: parent.width; spacing: 10 * tool.uiScale
            Text { height: 34 * tool.uiScale; verticalAlignment: Text.AlignVCenter; text: "MEASUREMENT STUDIO  /  " + tool.page; color: "#bdcedc"; font.pixelSize: 15 * tool.uiScale; width: 390 * tool.uiScale }
            LanXiButton { text: tool.inspectorVisible ? qsTr("Hide properties") : qsTr("Show properties"); uiScale: tool.uiScale; onClicked: tool.inspectorVisible = !tool.inspectorVisible }
            LanXiButton { text: qsTr("Save workspace"); uiScale: tool.uiScale; enabled: tool.idle; onClicked: tool.backend.saveWorkspace({recipe:advanced.recipe,geometry:advanced.geometry,inspector:tool.inspectorVisible,modal:modalPage.workspaceSettings(),soundPower:powerPage.workspaceSettings()}) }
            LanXiButton { visible: tool.page === "Measure"; text: tool.acousticView ? qsTr("Waveform / FRF") : qsTr("Acoustics / events"); uiScale: tool.uiScale; onClicked: tool.acousticView = !tool.acousticView }
            LanXiButton { visible: tool.page === "Measure"; text: tool.coherenceView ? qsTr("Phase") : qsTr("Coherence"); uiScale: tool.uiScale; onClicked: tool.coherenceView = !tool.coherenceView }
        }
    }
    LanXiNavigator { id: navigator; width: 224 * tool.uiScale; host: tool; uiScale: tool.uiScale
        anchors.left: parent.left; anchors.top: commands.bottom; anchors.topMargin: 10 * tool.uiScale; anchors.bottom: message.top; anchors.bottomMargin: 8 * tool.uiScale }
    LanXiInspector { id: inspector; width: tool.inspectorVisible ? 220 * tool.uiScale : 0; visible: tool.inspectorVisible; host: tool; uiScale: tool.uiScale
        anchors.right: parent.right; anchors.top: commands.bottom; anchors.topMargin: 10 * tool.uiScale; anchors.bottom: message.top; anchors.bottomMargin: 8 * tool.uiScale }
    LanXiAdvancedPage { id: advanced; host: tool; uiScale: tool.uiScale; visible: tool.advancedPage
        anchors.left: navigator.right; anchors.leftMargin: 12 * tool.uiScale; anchors.right: inspector.left; anchors.rightMargin: 12 * tool.uiScale
        anchors.top: commands.bottom; anchors.topMargin: 10 * tool.uiScale; anchors.bottom: message.top; anchors.bottomMargin: 8 * tool.uiScale }
    LanXiExchangePage { host:tool; uiScale:tool.uiScale; visible:tool.page==="Interchange"
        anchors.left:navigator.right; anchors.leftMargin:12*tool.uiScale; anchors.right:inspector.left; anchors.rightMargin:12*tool.uiScale
        anchors.top:commands.bottom; anchors.topMargin:10*tool.uiScale; anchors.bottom:message.top; anchors.bottomMargin:8*tool.uiScale }
    LanXiModalPage { id:modalPage; host:tool; uiScale:tool.uiScale; visible:tool.page==="Modal / ODS"
        anchors.left:navigator.right; anchors.leftMargin:12*tool.uiScale; anchors.right:inspector.left; anchors.rightMargin:12*tool.uiScale
        anchors.top:commands.bottom; anchors.topMargin:10*tool.uiScale; anchors.bottom:message.top; anchors.bottomMargin:8*tool.uiScale }
    LanXiSoundPowerPage { id:powerPage; host:tool; uiScale:tool.uiScale; visible:tool.page==="Sound power"
        anchors.left:navigator.right; anchors.leftMargin:12*tool.uiScale; anchors.right:inspector.left; anchors.rightMargin:12*tool.uiScale
        anchors.top:commands.bottom; anchors.topMargin:10*tool.uiScale; anchors.bottom:message.top; anchors.bottomMargin:8*tool.uiScale }
    Text {
        id: message
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        text: tool.status.message || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap
        color: tool.status.phase === "error" || tool.status.phase === "recovery" ? "#ffb5bf" : "#c9bad9"
        font.pixelSize: Math.round(17 * tool.uiScale)
    }
    Flickable {
        id: settingsScroll
        anchors.left: navigator.right; anchors.leftMargin: 12 * tool.uiScale; anchors.right: inspector.left; anchors.rightMargin: 12 * tool.uiScale; anchors.top: commands.bottom; anchors.topMargin: 10 * tool.uiScale
        anchors.bottom: message.top; anchors.bottomMargin: 12 * tool.uiScale
        visible: tool.configured && ["Measure","Interchange","Modal / ODS","Sound power"].indexOf(tool.page)<0 && !tool.advancedPage; clip: true
        contentWidth: width; contentHeight: settingsContent.height; boundsBehavior: Flickable.StopAtBounds
        Column {
            id: settingsContent
            width: settingsScroll.width; spacing: 24 * tool.uiScale
            Column {
                width: parent.width; spacing: 18 * tool.uiScale; visible: tool.page === "Setup"
                Flow {
                    width: parent.width; spacing: 20 * tool.uiScale; enabled: tool.idle
                    LanXiText { label: qsTr("Session / profile name"); value: tool.config.name || ""; uiScale: tool.uiScale; onEdited: function(value) { tool.edit(["name"], value) } }
                    LanXiSelect { label: qsTr("Measurement workflow"); uiScale: tool.uiScale; value: tool.config.workflow
                        options: [{value:"acoustic",label:"Acoustic measurement"},{value:"microphone",label:"Microphone under test"},{value:"amplifier",label:"Amplifier under test"}]
                        onSelected: function(value) { tool.workflow(value) } }
                    LanXiSelect { label: qsTr("Saved profile"); uiScale: tool.uiScale; value: tool.selectedProfile
                        options: tool.options(tool.status.profiles, "id", "name"); onSelected: function(value) { tool.selectedProfile = value; tool.backend.loadProfile(value) } }
                    LanXiButton { text: qsTr("Save profile"); uiScale: tool.uiScale; onClicked: tool.backend.saveProfile(tool.config.name, tool.config) }
                    LanXiButton { text: qsTr("New profile"); uiScale: tool.uiScale; onClicked: tool.backend.resetProfile(tool.config.workflow) }
                }
                Flow {
                    width: parent.width; spacing: 20 * tool.uiScale; enabled: tool.idle
                    LanXiText { width: 380 * tool.uiScale; label: qsTr("Type 3676 address (http://IP)"); value: tool.configured ? tool.config.devices[0].url : ""; uiScale: tool.uiScale
                        onEdited: function(value) { tool.edit(["devices", 0, "url"], value) } }
                    LanXiText { width: 380 * tool.uiScale; label: qsTr("Type 3677 address (http://IP)"); value: tool.configured ? tool.config.devices[1].url : ""; uiScale: tool.uiScale
                        onEdited: function(value) { tool.edit(["devices", 1, "url"], value) } }
                    LanXiButton { text: qsTr("Inspect both modules"); uiScale: tool.uiScale; onClicked: tool.backend.inspect(tool.config) }
                }
                Text { width: parent.width; text: qsTr("The modules use independent clocks. Each frequency-response pair must share a module. New profiles use voltage inputs with sensor powering off.")
                    wrapMode: Text.WordWrap; color: "#bfb0cf"; font.pixelSize: Math.round(18 * tool.uiScale) }
            }
            Flow {
                width: parent.width; spacing: 14 * tool.uiScale; visible: tool.page === "Setup" || tool.page === "Calibrate"
                LanXiButton { text: "Type 3676"; checked: tool.device === 0; uiScale: tool.uiScale; onClicked: tool.selectChannel(0, tool.channel) }
                LanXiButton { text: "Type 3677"; checked: tool.device === 1; uiScale: tool.uiScale; onClicked: tool.selectChannel(1, tool.channel) }
                Repeater { model: 4
                    delegate: LanXiButton { required property int index; text: "Ch " + (index + 1); checked: tool.channel === index + 1; uiScale: tool.uiScale
                        onClicked: tool.selectChannel(tool.device, index + 1) } }
            }
            Column {
                width: parent.width; spacing: 18 * tool.uiScale; visible: tool.page === "Setup"; enabled: tool.idle
                Text { text: qsTr("Device: %1 · serial %2").arg(tool.device === 0 ? "3676" : "3677").arg(tool.currentDevice.serial || "not inspected")
                    color: "#80d4ba"; font.pixelSize: Math.round(20 * tool.uiScale) }
                Flow {
                    width: parent.width; spacing: 18 * tool.uiScale
                    LanXiSelect { label: qsTr("Sample rate (Hz)"); uiScale: tool.uiScale; value: tool.currentDevice.sampleRate
                        options: tool.hardware.supportedSampleRates || [65536,32768,16384,8192]; onSelected: function(value) { tool.editDevice("sampleRate", value) } }
                    LanXiSelect { label: qsTr("Local reference channel"); uiScale: tool.uiScale; value: tool.currentDevice.reference
                        options: [{value:0,label:"None (levels only)"},1,2,3,4]; onSelected: function(value) { tool.editDevice("reference", value) } }
                    LanXiSelect { label: qsTr("FFT size · non-overlapping blocks"); uiScale: tool.uiScale; value: tool.config.fftSize
                        options: [1024,2048,4096,8192,16384,32768]; onSelected: function(value) { tool.edit(["fftSize"], value) } }
                }
                Flow {
                    width: parent.width; spacing: 18 * tool.uiScale
                    LanXiButton { text: qsTr("Channel enabled"); checked: tool.currentChannel.enabled || false; uiScale: tool.uiScale
                        onClicked: tool.editChannel("enabled", !tool.currentChannel.enabled) }
                    LanXiText { label: qsTr("Channel name"); value: tool.currentChannel.name || ""; uiScale: tool.uiScale; onEdited: function(value) { tool.editChannel("name", value) } }
                    LanXiSelect { label: qsTr("Engineering unit"); value: tool.currentChannel.unit; uiScale: tool.uiScale
                        options: tool.config.workflow === "amplifier" ? ["V"] : ["V","Pa","N","m/s2","m/s","m"]; onSelected: function(value) { tool.editChannel("unit", value) } }
                    ValueField { label: qsTr("Sensitivity (V / unit)"); value: tool.currentChannel.sensitivity || 1; minimum: 0.000000000001; maximum: 1000000000; uiScale: tool.uiScale
                        onEdited: function(value) { tool.editChannel("sensitivity", value) } }
                }
                Flow {
                    width: parent.width; spacing: 18 * tool.uiScale
                    LanXiSelect { label: qsTr("Input range"); value: tool.currentChannel.range; uiScale: tool.uiScale
                        options: tool.hardware.supportedRanges || ["10 Vpeak"]; onSelected: function(value) { tool.editChannel("range", value) } }
                    LanXiSelect { label: qsTr("High-pass filter"); value: tool.currentChannel.filter; uiScale: tool.uiScale
                        options: tool.hardware.supportedFilters || ["7.0 Hz","DC"]; onSelected: function(value) { tool.editChannel("filter", value) } }
                    LanXiButton { text: qsTr("CCLD power"); checked: tool.currentChannel.ccld || false; enabled: tool.config.workflow !== "amplifier"; uiScale: tool.uiScale
                        onClicked: tool.editChannel("ccld", !tool.currentChannel.ccld) }
                    LanXiButton { text: qsTr("200 V polarization"); checked: tool.currentChannel.polarization || false; enabled: tool.device === 1 && tool.config.workflow !== "amplifier"; uiScale: tool.uiScale
                        onClicked: tool.editChannel("polarization", !tool.currentChannel.polarization) }
                }
                Text { width: parent.width; text: qsTr("For a 10:1 attenuator, use V and sensitivity 0.1 V/V. Microphone sensitivity is in V/Pa. Polarization requires the matching front panel and the same setting on all four channels.")
                    color: "#bfb0cf"; wrapMode: Text.WordWrap; font.pixelSize: Math.round(17 * tool.uiScale) }
                Flow {
                    width: parent.width; spacing: 18 * tool.uiScale
                    ValueField { label: qsTr("Capture duration (s; 0 = manual)"); value: tool.config.durationSeconds || 0; minimum: 0; maximum: 86400; uiScale: tool.uiScale
                        onEdited: function(value) { tool.edit(["durationSeconds"], value) } }
                    ValueField { label: qsTr("Reference floor (RMS per bin)"); value: tool.config.referenceFloor || 0.000001; minimum: 0.000000000001; maximum: 1000000; uiScale: tool.uiScale
                        onEdited: function(value) { tool.edit(["referenceFloor"], value) } }
                    LanXiButton { text: qsTr("Record raw streams"); checked: tool.config.record || false; uiScale: tool.uiScale; onClicked: tool.edit(["record"], !tool.config.record) }
                }
            }
            Column {
                width: parent.width; spacing: 20 * tool.uiScale; visible: tool.page === "Calibrate"
                Text { width: parent.width; text: qsTr("Capture the calibrator tone with stimulus off, then stop. Calibration requires two seconds of stable, valid level. Apply it to the selected channel, save the profile, then verify with a new capture.")
                    color: "#ddd1eb"; wrapMode: Text.WordWrap; font.pixelSize: Math.round(21 * tool.uiScale) }
                Flow {
                    width: parent.width; spacing: 20 * tool.uiScale; enabled: tool.idle
                    ValueField { label: qsTr("Calibrator level (dB SPL)"); value: tool.calibratorLevel; minimum: 40; maximum: 150; uiScale: tool.uiScale; onEdited: function(value) { tool.calibratorLevel = value } }
                    ValueField { label: qsTr("Calibrator frequency (Hz)"); value: tool.calibratorHz; minimum: 20; maximum: 25600; uiScale: tool.uiScale; onEdited: function(value) { tool.calibratorHz = value } }
                    LanXiButton { text: qsTr("Apply calibration"); uiScale: tool.uiScale; enabled: tool.config.workflow !== "amplifier"
                        onClicked: tool.backend.calibrate(tool.device, tool.channel, tool.calibratorLevel, tool.calibratorHz) }
                }
                LanXiPlot { width: parent.width; height: 360 * tool.uiScale; uiScale: tool.uiScale
                    title: qsTr("Selected channel · RMS spectrum"); axis: qsTr("Frequency (Hz)"); values: tool.plot.spectrum || []; xStep: tool.plot.frequencyStep || 1 }
            }
            Column {
                width: parent.width; spacing: 20 * tool.uiScale; visible: tool.page === "Stimulus"; enabled: tool.idle
                Text { width: parent.width; text: qsTr("Type 3677 output 1 supplies stimulus. Check the DUT interface, amplifier load, reference wiring and permitted voltage. Both input streams must be active before output starts.")
                    color: "#ddd1eb"; wrapMode: Text.WordWrap; font.pixelSize: Math.round(21 * tool.uiScale) }
                Flow {
                    width: parent.width; spacing: 18 * tool.uiScale
                    LanXiSelect { label: qsTr("Waveform"); uiScale: tool.uiScale; value: tool.configured ? tool.config.generator.mode : "off"
                        options: [{value:"off",label:"Off · capture only"},{value:"sine",label:"Sine"},{value:"steps",label:"Stepped sine"},{value:"noise",label:"Band-limited noise"}]
                        onSelected: function(value) { tool.edit(["generator","mode"], value) } }
                    Repeater {
                        model: [{key:"levelVp",label:"Output amplitude (Vpeak)",low:0.00001,high:9.99999},
                            {key:"limitVp",label:"DUT amplitude limit (Vpeak)",low:0.00001,high:9.99999},
                            {key:"frequency",label:"Sine frequency (Hz)",low:20,high:25600},
                            {key:"startHz",label:"Start / noise lower edge (Hz)",low:20,high:25600},
                            {key:"stopHz",label:"Stop / noise upper edge (Hz)",low:20,high:25600},
                            {key:"steps",label:"Logarithmic steps",low:2,high:256},
                            {key:"settleSeconds",label:"Settling (s)",low:0.1,high:30},
                            {key:"dwellSeconds",label:"Dwell per step (s)",low:0.1,high:30},
                            {key:"rampSeconds",label:"Startup ramp (s)",low:0.05,high:5}]
                        delegate: ValueField { required property var modelData
                            label: modelData.label; value: tool.configured ? tool.config.generator[modelData.key] : modelData.low
                            minimum: modelData.low; maximum: modelData.high; uiScale: tool.uiScale
                            onEdited: function(value) { tool.edit(["generator",modelData.key], value) } }
                    }
                }
                LanXiButton { text: qsTr("Wiring and limits checked — arm output"); checked: tool.configured && tool.config.generator.armed; uiScale: tool.uiScale
                    onClicked: tool.edit(["generator","armed"], !tool.config.generator.armed) }
                Text { width: parent.width; text: qsTr("Setup edits disarm output. Loading a profile never starts the generator. Noise amplitude is a hardware full-scale setting; use the measured reference for RMS level and frequency response.")
                    color: "#bfb0cf"; wrapMode: Text.WordWrap; font.pixelSize: Math.round(18 * tool.uiScale) }
            }
            LanXiDevices { width: parent.width; host: tool; uiScale: tool.uiScale; visible: tool.page === "Setup" }
            LanXiAnalysisPage { width: parent.width; host: tool; uiScale: tool.uiScale; visible: tool.page === "Analysis" }
            Column {
                width: parent.width; spacing: 14 * tool.uiScale; visible: tool.page === "Review"
                Flow {
                    width: parent.width; spacing: 12 * tool.uiScale; enabled: tool.idle
                    LanXiButton { text: qsTr("Refresh sessions"); uiScale: tool.uiScale; onClicked: tool.backend.refreshRuns() }
                    LanXiButton { text: qsTr("Replay selected"); uiScale: tool.uiScale; enabled: tool.selectedRun.length > 0; onClicked: tool.backend.review(tool.selectedRun) }
                    LanXiButton { text: qsTr("Reprocess with analysis settings"); uiScale: tool.uiScale; enabled: tool.selectedRun.length > 0
                        onClicked: tool.backend.reprocess(tool.selectedRun, tool.config.analysis) }
                    LanXiButton { text: qsTr("Export CSV"); uiScale: tool.uiScale; enabled: tool.selectedRun.length > 0; onClicked: tool.backend.exportRun(tool.selectedRun, "csv") }
                    LanXiButton { text: qsTr("Export WAV"); uiScale: tool.uiScale; enabled: tool.selectedRun.length > 0; onClicked: tool.backend.exportRun(tool.selectedRun, "wav") }
                    LanXiButton { text: qsTr("View plots"); uiScale: tool.uiScale; onClicked: tool.page = "Measure" }
                    LanXiButton { text: qsTr("Pin comparison curve"); uiScale: tool.uiScale; onClicked: tool.backend.pinComparison() }
                    LanXiButton { text: qsTr("Clear comparison"); uiScale: tool.uiScale; onClicked: tool.backend.clearComparison() }
                    LanXiButton { text: qsTr("Export report"); uiScale: tool.uiScale; enabled: tool.selectedRun.length > 0
                        onClicked: tool.backend.report(tool.selectedRun, {title:tool.config.name,operator:tool.reportOperator,dut:tool.reportDut,notes:tool.reportNotes,analysis:tool.config.analysis}) }
                    LanXiButton { text: qsTr("Batch reports (%1)").arg(tool.batchRuns.length); uiScale: tool.uiScale; enabled: tool.batchRuns.length > 0
                        onClicked: tool.backend.batchReports(tool.batchRuns,{operator:tool.reportOperator,dut:tool.reportDut,notes:tool.reportNotes,analysis:tool.config.analysis}) }
                }
                Flow { width: parent.width; spacing: 16 * tool.uiScale
                    LanXiText { label: qsTr("Report operator"); value: tool.reportOperator; uiScale: tool.uiScale; onEdited: function(value) { tool.reportOperator = value } }
                    LanXiText { label: qsTr("DUT / serial / fixture"); value: tool.reportDut; uiScale: tool.uiScale; onEdited: function(value) { tool.reportDut = value } }
                    LanXiText { width: 500 * tool.uiScale; label: qsTr("Report notes"); value: tool.reportNotes; uiScale: tool.uiScale; onEdited: function(value) { tool.reportNotes = value } }
                }
                Text { width: parent.width; wrapMode: Text.WrapAnywhere; text: tool.status.reportPath || ""; color: "#80d4ba"; font.pixelSize: Math.round(17 * tool.uiScale) }
                LanXiText { width: 500 * tool.uiScale; label: qsTr("Find sessions by name, time or outcome"); value: tool.runFilter; uiScale: tool.uiScale; onEdited: function(value) { tool.runFilter=value } }
                Text { width: parent.width; text: qsTr("Recordings preserve each device's clock and full-rate data. CSV and floating-point WAV exports contain one physical channel per file in its engineering unit. The manifest records calibration and run quality.")
                    color: "#bfb0cf"; wrapMode: Text.WordWrap; font.pixelSize: Math.round(18 * tool.uiScale) }
                Repeater { model: (tool.status.runs || []).filter(function(run) { return (run.name+" "+run.started+" "+run.outcome).toLowerCase().indexOf(tool.runFilter.toLowerCase())>=0 })
                    delegate: Row { id: runRow; required property var modelData
                        width: settingsContent.width; spacing: 10 * tool.uiScale
                        LanXiButton { width: 110 * tool.uiScale; text: qsTr("Batch"); uiScale: tool.uiScale; checked: tool.batchRuns.indexOf(runRow.modelData.id)>=0
                            enabled: tool.idle && runRow.modelData.recorded; onClicked: tool.toggleBatch(runRow.modelData.id) }
                        LanXiButton { width: parent.width-120*tool.uiScale; uiScale: tool.uiScale; checked: tool.selectedRun === runRow.modelData.id
                            text: runRow.modelData.started + " · " + runRow.modelData.name + " · " + runRow.modelData.outcome + (runRow.modelData.recorded ? "" : " · no raw recording")
                            enabled: tool.idle && runRow.modelData.recorded; onClicked: tool.selectedRun = runRow.modelData.id }
                    } }
                Repeater { model: tool.status.batchResults || []
                    delegate: Text { required property var modelData; width: settingsContent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
                        text: (modelData.success ? "Saved: " : "Failed: ")+modelData.runId+" · "+modelData.message; color: modelData.success ? "#80d4ba" : "#ffb5bf"; font.pixelSize: Math.round(16 * tool.uiScale) } }
                Text { visible: !tool.status.runs || tool.status.runs.length === 0; text: qsTr("Recorded sessions will appear here."); color: "#bfb0cf"; font.pixelSize: Math.round(20 * tool.uiScale) }
                Text { text: tool.status.runPath || ""; width: parent.width; wrapMode: Text.WrapAnywhere; color: "#80d4ba"; font.pixelSize: Math.round(17 * tool.uiScale) }
                Repeater { model: tool.status.steps || []
                    delegate: Text { required property var modelData; width: settingsContent.width; color: "#ddd1eb"; font.pixelSize: Math.round(17 * tool.uiScale)
                        text: (modelData.device === 0 ? "3676" : "3677") + " Ch " + modelData.channel + " / Ch " + modelData.reference + " · "
                            + modelData.frequencyHz.toFixed(1) + " Hz · " + modelData.magnitude.toPrecision(5) + " " + modelData.unit + " · " + modelData.phase.toFixed(2) + "°" } }
            }
        }
    }
    Item {
        anchors.left: navigator.right; anchors.leftMargin: 12 * tool.uiScale; anchors.right: inspector.left; anchors.rightMargin: 12 * tool.uiScale; anchors.top: commands.bottom; anchors.topMargin: 10 * tool.uiScale
        anchors.bottom: message.top; anchors.bottomMargin: 12 * tool.uiScale; visible: tool.page === "Measure"
        LanXiAcoustics {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: liveTable.top; anchors.bottomMargin: 10 * tool.uiScale
            visible: tool.acousticView; uiScale: tool.uiScale; plot: tool.plot
        }
        Grid {
            id: plots
            visible: !tool.acousticView
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.bottom: liveTable.top; anchors.bottomMargin: 10 * tool.uiScale
            columns: 2; spacing: 12 * tool.uiScale
            readonly property real plotWidth: Math.max(1, (width - spacing) / 2)
            readonly property real plotHeight: Math.max(1, (height - spacing) / 2)
            LanXiPlot { width: plots.plotWidth; height: plots.plotHeight; uiScale: tool.uiScale
                title: qsTr("%1 · Ch %2 waveform (%3)").arg(tool.plot.device === 0 ? "3676" : "3677").arg(tool.plot.channel || "—").arg(tool.plot.unit || "")
                values: tool.plot.wave || []; axis: qsTr("Seconds since this channel's first sample"); xStart: tool.plot.waveStart || 0; xStep: tool.plot.waveStep || 1 }
            LanXiPlot { width: plots.plotWidth; height: plots.plotHeight; uiScale: tool.uiScale
                title: qsTr("RMS spectrum · %1").arg(tool.plot.unit === "Pa" ? "dB re 20 µPa" : "dB re 1 " + (tool.plot.unit || "unit")); values: tool.plot.spectrum || []
                comparison: tool.compatibleComparison ? tool.referencePlot.spectrum || [] : []
                axis: qsTr("Frequency (Hz)"); xStep: tool.plot.frequencyStep || 1; accent: "#74caff" }
            LanXiPlot { width: plots.plotWidth; height: plots.plotHeight; uiScale: tool.uiScale
                title: qsTr("H1 response · dB re 1 %1").arg(tool.plot.responseUnit || "unit/unit"); values: tool.plot.gain || []
                comparison: tool.compatibleComparison && tool.referencePlot.responseUnit === tool.plot.responseUnit ? tool.referencePlot.gain || [] : []
                axis: qsTr("Frequency (Hz)"); xStep: tool.plot.frequencyStep || 1
                emptyText: qsTr("Requires a valid local reference and at least two averages") }
            LanXiPlot { width: plots.plotWidth; height: plots.plotHeight; uiScale: tool.uiScale
                title: tool.coherenceView ? qsTr("Magnitude-squared coherence") : qsTr("H1 phase (degrees)")
                values: (tool.coherenceView ? tool.plot.coherence : tool.plot.phase) || []; axis: qsTr("Frequency (Hz)"); xStep: tool.plot.frequencyStep || 1
                fixedRange: true; low: tool.coherenceView ? 0 : -180; high: tool.coherenceView ? 1 : 180; accent: "#be9bff" }
        }
        Rectangle {
            id: liveTable; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 104 * tool.uiScale; color: "#172431"; border.color: "#35495b"
            Flickable {
                anchors.fill: parent; anchors.margins: 8 * tool.uiScale; contentWidth: width; contentHeight: rows.height; clip: true
                Column { id: rows; width: parent.width; spacing: 4 * tool.uiScale
                    Text { text: qsTr("CHANNEL MONITOR   /   Module · Input · Name · RMS · Unit · Samples · Quality"); color: "#8aa4b8"; font.pixelSize: 12 * tool.uiScale }
                    Repeater { model: tool.status.channels || []
                        delegate: Text { required property var modelData; width: rows.width; elide: Text.ElideRight
                            text: (modelData.device === 0 ? "3676" : "3677") + "   /   " + modelData.channel + "   /   " + modelData.name + "   /   "
                                + (modelData.rms === null || modelData.rms === undefined ? "—" : Number(modelData.rms).toPrecision(6)) + "  " + modelData.unit + "   /   " + modelData.samples + "   /   " + (modelData.quality ? "INVALID " + modelData.quality : "Valid")
                            color: modelData.quality ? "#ff9ca9" : "#c8dbe9"; font.pixelSize: 13 * tool.uiScale }
                    }
                }
            }
        }
    }
}
