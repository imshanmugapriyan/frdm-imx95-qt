pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Item {
    id: page
    required property var host
    property real uiScale: 1
    readonly property string operation: ({Processing:"processing",Orders:"orders",Structural:"structural",Limits:"limits"})[host.page] || "processing"
    property bool fullRecording: false
    readonly property var result: fullRecording && operation==="processing" ? host.status.recordResult || ({}) : host.status.workbench || ({})
    readonly property bool currentResult: result.operation === operation && result.device === host.device && result.channel === host.channel
    property bool restored: false
    property var recipe: ({windowSamples:262144,referenceChannel:0,outputUnit:"",stages:[{type:"removeMean",value:0}],tachChannel:1,pulsesPerRevolution:1,samplesPerRevolution:256,revolutionsPerBlock:4,threshold:0,hysteresis:.1,maximumOrder:20,extractOrder:1,lowerHz:80,upperHz:120,coherenceFloor:.8,responseKind:"accelerance",metric:"gain",limits:[{hz:100,lower:-3,upper:3},{hz:10000,lower:-3,upper:3}]})
    property var geometry: [{channel:1,x:-1.5,y:0,z:0},{channel:2,x:-.5,y:0,z:0},{channel:3,x:.5,y:0,z:0},{channel:4,x:1.5,y:0,z:0}]
    property string newStage: "highPass"
    property int repetitions: 3
    property real sequenceDelay: 1
    property bool stopOnFail: true
    function change(key,value) {var next=host.copy(recipe);next[key]=value;recipe=next}
    function rowChange(key,index,field,value) {var next=host.copy(recipe);next[key][index][field]=value;recipe=next}
    function removeStage(index) {var next=host.copy(recipe);next.stages.splice(index,1);recipe=next}
    function geometryChange(index,field,value) {var next=host.copy(geometry);next[index][field]=value;geometry=next}
    function calculate() {
        var options=host.copy(recipe);options.operation=operation;options.geometry=geometry
        if(fullRecording && operation==="processing") {options.device=host.device;options.channel=host.channel;options.referenceChannel=0;delete options.windowSamples;if(!options.outputUnit)delete options.outputUnit;host.backend.processRecording(host.selectedRun,options)}
        else {if(!options.outputUnit)options.outputUnit=host.plot.unit||host.currentChannel.unit;host.backend.analyze(options)}
    }
    Connections { target: page.host.backend
        function onChanged() {
            var saved=page.host.status.workspace
            if(!page.restored && saved && saved.schema===1) {
                if(saved.recipe) {var merged=page.host.copy(page.recipe);Object.keys(saved.recipe).forEach(function(k){merged[k]=saved.recipe[k]});page.recipe=merged}
                if(saved.geometry)page.geometry=saved.geometry
                if(saved.inspector!==undefined)page.host.inspectorVisible=saved.inspector
                page.restored=true
            }
        }
    }
    Column {
        id: heading; width: parent.width; spacing: 7 * page.uiScale
        Row { spacing: 8 * page.uiScale
            LanXiButton { text: qsTr("Calculate %1").arg(page.host.page.toLowerCase()); uiScale: page.uiScale; primary: true; enabled: page.host.idle; onClicked: page.calculate() }
            LanXiButton { visible: !page.fullRecording || page.operation!=="processing"; text: qsTr("Export result"); uiScale: page.uiScale; enabled: page.host.idle && page.currentResult; onClicked: page.host.backend.exportAnalysis() }
            Text { height: 34 * page.uiScale; verticalAlignment: Text.AlignVCenter; text: page.currentResult ? page.result.samples + " samples · " + Number(page.result.duration).toFixed(3) + " s" : qsTr("Capture or replay, stop, then calculate"); color: "#9bafc0"; font.pixelSize: 13 * page.uiScale }
        }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: page.result.error || (page.fullRecording && page.operation==="processing" ? qsTr("Entire saved recording: all samples go to CSV and UFF. Stateful causal filters; integration is leaky and differentiation is backward difference. Mean removal must be first. Envelope and correlation are window-only.") : qsTr("Analysis uses the final contiguous window (up to 262144 samples). Source recordings remain unchanged. References and tachometers must be on this module.")); color: page.result.error ? "#ffacb5" : "#9bafc0"; font.pixelSize: 12 * page.uiScale }
    }
    Rectangle {
        id: setup; anchors.top: heading.bottom; anchors.topMargin: 8 * page.uiScale; width: parent.width; height: Math.min(parent.height*.36,240*page.uiScale)
        color: "#172431"; border.color: "#35495b"
        Flickable {
            anchors.fill: parent; anchors.margins: 12 * page.uiScale; clip: true; contentWidth: width; contentHeight: settings.height; boundsBehavior: Flickable.StopAtBounds
            Column {
                id: settings; width: parent.width; spacing: 12 * page.uiScale; enabled: page.host.idle
                Flow { width: parent.width; spacing: 12 * page.uiScale
                    LanXiButton { visible: page.operation==="processing"; text: qsTr("Entire recording"); checked:page.fullRecording; uiScale:page.uiScale; onClicked:{page.fullRecording=!page.fullRecording;if(page.fullRecording)page.host.backend.refreshRuns()} }
                    LanXiSelect { visible:page.fullRecording && page.operation==="processing"; width:320*page.uiScale; label:qsTr("Saved recording"); uiScale:page.uiScale; value:page.host.selectedRun; options:(page.host.status.runs||[]).filter(function(run){return run.recorded && run.outcome==="complete"}).map(function(run){return {value:run.id,label:run.name+" · "+run.started}}); onSelected:function(value){page.host.selectedRun=value} }
                    LanXiSelect { visible:!page.fullRecording || page.operation!=="processing"; width: 180 * page.uiScale; label: qsTr("Analysis window samples"); uiScale: page.uiScale; value: page.recipe.windowSamples; options:[8192,16384,32768,65536,131072,262144]; onSelected: function(value){page.change("windowSamples",value)} }
                    LanXiSelect { visible: page.operation==="processing" && !page.fullRecording; width: 180 * page.uiScale; label: qsTr("Correlation reference"); uiScale: page.uiScale; value: page.recipe.referenceChannel; options:[{value:0,label:"Off"},1,2,3,4]; onSelected: function(value){page.change("referenceChannel",value)} }
                    LanXiText { visible: page.operation==="processing"; width: 180 * page.uiScale; label: qsTr("Processed unit annotation"); uiScale: page.uiScale; value: page.recipe.outputUnit; onEdited: function(value){page.change("outputUnit",value)} }
                    LanXiSelect { visible: page.operation==="orders"; width: 160 * page.uiScale; label: qsTr("Tachometer input"); uiScale: page.uiScale; value: page.recipe.tachChannel; options:[1,2,3,4]; onSelected: function(value){page.change("tachChannel",value)} }
                    LanXiSelect { visible: page.operation==="orders"; width: 160 * page.uiScale; label: qsTr("Angular samples / rev"); uiScale: page.uiScale; value: page.recipe.samplesPerRevolution; options:[64,128,256,512,1024]; onSelected: function(value){page.change("samplesPerRevolution",value)} }
                    LanXiSelect { visible: page.operation==="orders"; width: 160 * page.uiScale; label: qsTr("Revolutions / block"); uiScale: page.uiScale; value: page.recipe.revolutionsPerBlock; options:[1,2,4,8,16,32]; onSelected: function(value){page.change("revolutionsPerBlock",value)} }
                    LanXiSelect { visible: page.operation==="structural"; width: 200 * page.uiScale; label: qsTr("Measured response / force"); uiScale: page.uiScale; value: page.recipe.responseKind; options:["accelerance","mobility","receptance"]; onSelected: function(value){page.change("responseKind",value)} }
                    LanXiSelect { visible: page.operation==="limits"; width: 220 * page.uiScale; label: qsTr("Acceptance function"); uiScale: page.uiScale; value: page.recipe.metric; options:[{value:"gain",label:"H1 magnitude (dB)"},{value:"spectrum",label:"RMS spectrum (dB)"},{value:"coherence",label:"Coherence (0..1)"}]; onSelected: function(value){page.change("metric",value)} }
                }
                Column {
                    width: parent.width; spacing: 6 * page.uiScale; visible: page.operation==="processing"
                    Repeater { model: page.recipe.stages
                        delegate: Row { id: stageRow; required property int index; required property var modelData; spacing: 10 * page.uiScale
                            Text { width: 200 * page.uiScale; height: 36 * page.uiScale; verticalAlignment: Text.AlignVCenter; text: (stageRow.index+1)+"  ·  "+stageRow.modelData.type; color: "#d8e8f3"; font.pixelSize: 15 * page.uiScale }
                            ValueField { width: 180 * page.uiScale; label: qsTr("Hz / gain (where used)"); value: stageRow.modelData.value; minimum: -1e9; maximum: 1e9; uiScale: page.uiScale*.8; onEdited: function(value){page.rowChange("stages",stageRow.index,"value",value)} }
                            LanXiButton { text: qsTr("Remove"); uiScale: page.uiScale; onClicked: page.removeStage(stageRow.index) }
                        }
                    }
                    Row { spacing: 10 * page.uiScale
                        LanXiSelect { width: 220*page.uiScale; label: qsTr("Append processing stage"); value: page.newStage; options:["removeMean","gain","highPass","lowPass","differentiate","integrate","envelope"]; uiScale: page.uiScale; onSelected: function(value){page.newStage=value} }
                        LanXiButton { text: qsTr("Add stage"); uiScale: page.uiScale; enabled: page.recipe.stages.length<16; onClicked: {var rows=page.host.copy(page.recipe.stages);rows.push({type:page.newStage,value:page.newStage==="gain"?1:20});page.change("stages",rows)} }
                    }
                    Text { width: parent.width; text: qsTr("Stages execute top to bottom. Integration requires a positive cutoff. Finite-window filters and Hilbert envelopes contain edge transients; unit annotations are user supplied."); color: "#9bafc0"; wrapMode: Text.WordWrap; font.pixelSize: 12 * page.uiScale }
                }
                Flow { width: parent.width; spacing: 12*page.uiScale; visible: page.operation==="orders" || page.operation==="structural"
                    Repeater { model: page.operation==="orders" ? [{key:"pulsesPerRevolution",label:"Pulses / revolution",low:1,high:1024},{key:"threshold",label:"Tach threshold (unit)",low:-1e6,high:1e6},{key:"hysteresis",label:"Tach hysteresis",low:.000001,high:1e6},{key:"maximumOrder",label:"Maximum order",low:1,high:1000},{key:"extractOrder",label:"Extract order",low:0,high:1000}]
                        : [{key:"lowerHz",label:"Fit start (Hz)",low:.1,high:25600},{key:"upperHz",label:"Fit end (Hz)",low:1,high:25600},{key:"coherenceFloor",label:"Minimum coherence",low:0,high:1}]
                        delegate: ValueField { required property var modelData; width: 170 * page.uiScale; uiScale: page.uiScale*.8; label: modelData.label; value: page.recipe[modelData.key]; minimum:modelData.low; maximum:modelData.high; onEdited: function(value){page.change(modelData.key,value)} }
                    }
                }
                Text { visible: page.operation==="structural"; width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Single isolated mode: complex receptance fit, damping and FRAC. Requires an N reference and response units matching the selected motion quantity. Geometry joins nodes in row order; animation shows Z motion of the normalized measured FRF."); color:"#9bafc0"; font.pixelSize:12*page.uiScale }
                Column { width: parent.width; spacing: 8*page.uiScale; visible: page.operation==="structural"
                    Repeater { model: page.geometry
                        delegate: Row { id: pointRow; required property var modelData; required property int index; spacing: 10*page.uiScale
                            Text { width: 90*page.uiScale; text:"Ch "+pointRow.modelData.channel; color:"#d7e8f3"; font.pixelSize:14*page.uiScale }
                            Repeater { model:["x","y","z"]
                                delegate: ValueField { required property string modelData; width: 150*page.uiScale; label:modelData; value:pointRow.modelData[modelData]; minimum:-1e6; maximum:1e6; uiScale:page.uiScale*.8; onEdited:function(value){page.geometryChange(pointRow.index,modelData,value)} }
                            }
                        }
                    }
                }
                Column { width: parent.width; spacing: 8*page.uiScale; visible: page.operation==="limits"
                    Repeater { model:page.recipe.limits
                        delegate: Row { id: limitRow; required property var modelData; required property int index; spacing:10*page.uiScale
                            Repeater { model:["hz","lower","upper"]
                                delegate: ValueField { required property string modelData; width: 170*page.uiScale; uiScale:page.uiScale*.8; label:modelData; value:limitRow.modelData[modelData]; minimum:modelData==="hz"?0:-1e6; maximum:1e6; onEdited:function(value){page.rowChange("limits",limitRow.index,modelData,value)} }
                            }
                            LanXiButton { text:qsTr("Remove"); uiScale:page.uiScale; enabled:page.recipe.limits.length>2; onClicked:{var rows=page.host.copy(page.recipe.limits);rows.splice(limitRow.index,1);page.change("limits",rows)} }
                        }
                    }
                    LanXiButton { text:qsTr("Add breakpoint"); uiScale:page.uiScale; onClicked:{var rows=page.host.copy(page.recipe.limits);var last=rows[rows.length-1];rows.push({hz:last.hz+100,lower:last.lower,upper:last.upper});page.change("limits",rows)} }
                    Text { width:parent.width; text:qsTr("Breakpoints must be in increasing frequency order. Limits interpolate linearly; unavailable bins and incomplete frequency coverage produce INVALID."); wrapMode:Text.WordWrap; color:"#9bafc0"; font.pixelSize:12*page.uiScale }
                    Flow { width:parent.width; spacing:12*page.uiScale
                        ValueField { width:160*page.uiScale; label:qsTr("Sequence repetitions"); value:page.repetitions; minimum:1; maximum:100; uiScale:page.uiScale*.8; onEdited:function(value){page.repetitions=Math.round(value)} }
                        ValueField { width:160*page.uiScale; label:qsTr("Between runs (s)"); value:page.sequenceDelay; minimum:0; maximum:3600; uiScale:page.uiScale*.8; onEdited:function(value){page.sequenceDelay=value} }
                        LanXiButton { text:qsTr("Stop on failed limit"); checked:page.stopOnFail; uiScale:page.uiScale; onClicked:page.stopOnFail=!page.stopOnFail }
                        LanXiButton { text:qsTr("Start test sequence"); primary:true; uiScale:page.uiScale; enabled:page.host.idle&&page.host.backend.ready
                            onClicked:{var setup=page.host.copy(page.host.config);var acceptance=page.host.copy(page.recipe);acceptance.device=page.host.device;acceptance.channel=page.host.channel;setup.sequence={repetitions:page.repetitions,delaySeconds:page.sequenceDelay,stopOnFail:page.stopOnFail,acceptance:acceptance};page.host.session.start("lanxi-measurement",setup)} }
                    }
                    Text { width:parent.width; text:qsTr("Uses the current acquisition duration, channel configuration and armed generator settings. Every repetition stops the output and closes both modules before its limit check and next run. Stop cancels remaining repetitions."); wrapMode:Text.WordWrap; color:"#9bafc0"; font.pixelSize:12*page.uiScale }
                    Repeater { model:page.host.status.sequenceResults||[]
                        delegate:Text { required property var modelData; width:settings.width; text:modelData.iteration+"  /  "+String(modelData.verdict).toUpperCase()+"  /  "+modelData.runId; color:modelData.verdict==="pass"?"#80d4ba":"#ecc478"; font.pixelSize:13*page.uiScale; elide:Text.ElideRight }
                    }
                }
            }
        }
    }
    Grid {
        id: display; anchors.top:setup.bottom; anchors.topMargin:10*page.uiScale; anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:summary.top; anchors.bottomMargin:8*page.uiScale
        columns:2; spacing:10*page.uiScale
        readonly property real cellWidth:Math.max(1,(width-spacing)/2)
        readonly property real cellHeight:Math.max(1,(height-spacing)/2)
        LanXiPlot { width:display.cellWidth; height:display.cellHeight; uiScale:page.uiScale
            title:page.operation==="processing"?qsTr("Processed signal"):page.operation==="orders"?qsTr("Order spectrum · RMS"):page.operation==="limits"?qsTr("Acceptance function"):qsTr("Measured / fitted receptance")
            values:!page.currentResult?[]:page.operation==="processing"?page.result.wave||[]:page.operation==="orders"?page.result.orders||[]:page.operation==="limits"?page.result.spectrum||[]:page.result.measuredPlot||[]
            comparison:page.currentResult&&page.operation==="structural"?page.result.fittedPlot||[]:[]
            xStart:page.operation==="structural"?page.result.frequencyStart||0:0
            xStep:page.operation==="processing"?page.result.waveStep||1:page.operation==="orders"?page.result.orderStep||1:page.result.frequencyStep||1
            axis:page.operation==="processing"?"Time (s)":page.operation==="orders"?"Order":"Frequency (Hz)"
        }
        LanXiPlot { visible:page.operation!=="structural"; width:display.cellWidth; height:display.cellHeight; uiScale:page.uiScale
            title:page.operation==="processing"?qsTr("Processed spectrum · RMS"):page.operation==="orders"?qsTr("Shaft speed per angular block"):qsTr("Coherence")
            values:!page.currentResult?[]:page.operation==="processing"?page.result.spectrum||[]:page.operation==="orders"?page.result.rpm||[]:page.host.plot.coherence||[]
            xStep:page.operation==="orders"?1:page.result.frequencyStep||page.host.plot.frequencyStep||1
            axis:page.operation==="orders"?"Angular block index":"Frequency (Hz)"; accent:"#6bcaff"
        }
        LanXiGeometry { visible:page.operation==="structural"; width:display.cellWidth; height:display.cellHeight; uiScale:page.uiScale; points:page.geometry; shape:page.currentResult?page.result.shape||[]:[] }
        LanXiPlot { width:display.cellWidth; height:display.cellHeight; uiScale:page.uiScale
            title:page.operation==="processing"?qsTr("Normalized cross-correlation"):page.operation==="orders"?qsTr("Extracted order %1 · RMS").arg(page.result.extractOrder||page.recipe.extractOrder):qsTr("H1 magnitude · dB")
            values:!page.currentResult?[]:page.operation==="processing"?page.result.correlation||[]:page.operation==="orders"?page.result.trace||[]:page.host.plot.gain||[]
            xStart:page.operation==="processing"?page.result.lagStart||0:0
            xStep:page.operation==="processing"?page.result.lagStep||1:page.operation==="orders"?1:page.host.plot.frequencyStep||1
            axis:page.operation==="processing"?"Lag (s); positive = response delayed":page.operation==="orders"?"Angular block index":"Frequency (Hz)"; accent:"#e4c477"
        }
        LanXiOrderMap { visible:page.operation==="orders"; width:display.cellWidth; height:display.cellHeight; uiScale:page.uiScale; frames:page.currentResult?page.result.map||[]:[]; rpm:page.currentResult?page.result.rpm||[]:[]; orderStep:page.result.orderStep||1 }
        Rectangle { visible:page.operation!=="orders"; width:display.cellWidth; height:display.cellHeight; color:"#172431"; border.color:"#35495b"
            Flickable { anchors.fill:parent; anchors.margins:16*page.uiScale; clip:true; contentWidth:width; contentHeight:resultText.height
                Text { id:resultText; width:parent.width; textFormat:Text.PlainText; wrapMode:Text.WordWrap; color:"#dbeaf6"; font.pixelSize:16*page.uiScale
                    text:!page.currentResult?qsTr("RESULT SUMMARY\n\nSelect a channel, configure this operation and calculate."):page.operation==="processing"?"PROCESSING\n\nRMS: "+Number(page.result.rms).toPrecision(6)+" "+page.result.processedUnit+"\nDelay: "+(page.result.delaySeconds===undefined?"not evaluated":Number(page.result.delaySeconds*1000).toFixed(3)+" ms")+"\nCorrelation: "+(page.result.coefficient===undefined?"—":Number(page.result.coefficient).toFixed(5))
                    :page.operation==="orders"?"ORDER ANALYSIS\n\n"+page.result.blocks+" angular blocks\n"+page.result.pulses+" tachometer pulses\nRPM: "+Number(page.result.minimumRpm).toFixed(1)+" – "+Number(page.result.maximumRpm).toFixed(1)+"\nResolution: "+page.result.orderStep+" order\n\nFull order map is included in the result export."
                    :page.operation==="structural"?"SINGLE-MODE FIT\n\nNatural frequency: "+Number(page.result.frequency).toFixed(3)+" Hz\nDamping: "+Number(page.result.dampingPercent).toFixed(3)+" %\nRelative fit error: "+Number(100*page.result.relativeError).toFixed(2)+" %\nFRAC: "+Number(page.result.frac).toFixed(5)+"\n\n"+page.result.method
                    :"ACCEPTANCE: "+String(page.result.verdict).toUpperCase()+"\n\nEvaluated bins: "+page.result.evaluated+"\nFailed bins: "+page.result.failed+"\nMissing coverage: "+page.result.missing+"\nWorst margin: "+(page.result.worstMargin===null?"—":Number(page.result.worstMargin).toPrecision(6)) }
            }
        }
    }
    Text { id:summary; anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:parent.bottom; text:page.result.exportPath||qsTr("Save workspace to retain recipes, limit curves and geometry. Export result writes JSON provenance and available full-resolution CSV data."); wrapMode:Text.WrapAnywhere; color:"#8aa4b8"; font.pixelSize:12*page.uiScale }
}
