pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Item {
    id: page
    required property var host
    property real uiScale:1
    property string operation:"modal"
    property string source:"capture"
    property string seeds:"100, 112"
    property real lowerHz:70
    property real upperHz:150
    property real frequency:100
    property real coherenceFloor:0.8
    property bool allCompatible:false
    property bool residualTerms:true
    property int selectedMode:0
    property bool editGeometry:false
    property var geometry:[{node:1,channel:1,direction:3,x:-1.5,y:0,z:0},{node:2,channel:2,direction:3,x:-.5,y:0,z:0},{node:3,channel:3,direction:3,x:.5,y:0,z:0},{node:4,channel:4,direction:3,x:1.5,y:0,z:0}]
    readonly property var result:host.status.modalResult||({})
    readonly property var modes:result.modes||[]
    readonly property var imported:host.status.interchange||({})
    readonly property var importedSelection:imported.selected||({})
    property bool restored:false
    function workspaceSettings(){return {seeds:seeds,lowerHz:lowerHz,upperHz:upperHz,frequency:frequency,coherenceFloor:coherenceFloor,residualTerms:residualTerms,geometry:geometry}}
    Connections { target:page.host.backend
        function onChanged(){var saved=page.host.status.workspace;if(page.restored||!saved||saved.schema!==1)return;page.restored=true;var m=saved.modal;if(!m)return;page.seeds=m.seeds;page.lowerHz=m.lowerHz;page.upperHz=m.upperHz;page.frequency=m.frequency;page.coherenceFloor=m.coherenceFloor;page.residualTerms=m.residualTerms;page.geometry=m.geometry}
    }
    function calculate(){selectedMode=0;host.backend.structuralAnalysis({operation:operation,source:source,lowerHz:lowerHz,upperHz:upperHz,coherenceFloor:coherenceFloor,frequency:frequency,seeds:seeds.split(",").map(function(s){return Number(s.trim())}),residualTerms:residualTerms,index:importedSelection.index===undefined?-1:importedSelection.index,allCompatible:allCompatible,geometry:geometry})}
    function editPoint(index,key,value){var rows=host.copy(geometry);rows[index][key]=["node","channel","direction"].indexOf(key)>=0?Math.round(value):value;geometry=rows}
    Column {
        id: header; width:parent.width; spacing:8*page.uiScale
        Flow { width:parent.width; spacing:10*page.uiScale; enabled:page.host.idle
            LanXiSelect { width:200*page.uiScale; label:qsTr("Analysis"); value:page.operation; options:[{value:"modal",label:"Multi-mode fit"},{value:"ods",label:"Spectral ODS"}]; uiScale:page.uiScale; onSelected:function(value){page.operation=value;if(value==="ods")page.source="capture"} }
            LanXiSelect { width:220*page.uiScale; label:qsTr("Data source"); value:page.source; options:page.operation==="ods"?[{value:"capture",label:"Captured / replayed"}]:[{value:"capture",label:"Captured / replayed"},{value:"import",label:"Imported UFF FRFs"}]; uiScale:page.uiScale; onSelected:function(value){page.source=value} }
            LanXiText { visible:page.operation==="modal"; width:220*page.uiScale; label:qsTr("Mode seeds (Hz, comma separated)"); value:page.seeds; uiScale:page.uiScale; onEdited:function(value){page.seeds=value} }
            LanXiButton { text:qsTr("Calculate and save"); uiScale:page.uiScale; primary:true; onClicked:page.calculate() }
            LanXiButton { text:qsTr("Geometry"); uiScale:page.uiScale; checked:page.editGeometry; onClicked:page.editGeometry=!page.editGeometry }
        }
        Flow { width:parent.width; spacing:12*page.uiScale; enabled:page.host.idle
            ValueField { visible:page.operation==="modal"; width:150*page.uiScale; label:qsTr("Start Hz"); value:page.lowerHz; minimum:.1; maximum:100000; uiScale:page.uiScale*.8; onEdited:function(value){page.lowerHz=value} }
            ValueField { visible:page.operation==="modal"; width:150*page.uiScale; label:qsTr("End Hz"); value:page.upperHz; minimum:1; maximum:100000; uiScale:page.uiScale*.8; onEdited:function(value){page.upperHz=value} }
            ValueField { visible:page.operation==="ods"; width:150*page.uiScale; label:qsTr("ODS Hz"); value:page.frequency; minimum:.1; maximum:100000; uiScale:page.uiScale*.8; onEdited:function(value){page.frequency=value} }
            ValueField { visible:page.source==="capture"; width:150*page.uiScale; label:qsTr("Coherence minimum"); value:page.coherenceFloor; minimum:0; maximum:1; uiScale:page.uiScale*.8; onEdited:function(value){page.coherenceFloor=value} }
            LanXiButton { visible:page.operation==="modal"; text:qsTr("Residual terms"); checked:page.residualTerms; uiScale:page.uiScale; onClicked:page.residualTerms=!page.residualTerms }
            LanXiSelect { visible:page.source==="import"; width:270*page.uiScale; label:qsTr("Imported FRF"); value:page.importedSelection.index===undefined?-1:page.importedSelection.index; options:page.host.options(page.imported.functions,"index","name"); uiScale:page.uiScale; onSelected:function(value){page.host.backend.selectUff(value)} }
            LanXiButton { visible:page.source==="import"; text:qsTr("Include compatible FRFs"); checked:page.allCompatible; uiScale:page.uiScale; onClicked:page.allCompatible=!page.allCompatible }
        }
        Text { width:parent.width; wrapMode:Text.WordWrap; color:page.result.error?"#ffacb5":"#9bafc0"; font.pixelSize:12*page.uiScale
            text:page.result.error||qsTr("Fit 1–6 seeded modes using a common force reference and calibrated motion responses. Imported FRFs must share a frequency grid and reference DOF. ODS uses measured response amplitude and relative phase. Scalar and rotational DOFs are not animated.") }
        Flickable { visible:page.editGeometry; width:parent.width; height:visible?180*page.uiScale:0; contentWidth:width; contentHeight:points.height; clip:true
            Column { id:points; width:parent.width; spacing:6*page.uiScale
                Repeater { model:page.geometry
                    delegate:Row { id:point; required property var modelData; required property int index; spacing:6*page.uiScale
                        Repeater { model:["node","channel","direction","x","y","z"]
                            delegate:ValueField { required property string modelData; width:115*page.uiScale; label:modelData; value:point.modelData[modelData]; minimum:modelData==="node"||modelData==="channel"?1:-100000; maximum:100000; uiScale:page.uiScale*.75; onEdited:function(value){page.editPoint(point.index,modelData,value)} }
                        }
                        LanXiButton { text:qsTr("Remove"); uiScale:page.uiScale*.8; onClicked:{var rows=page.host.copy(page.geometry);rows.splice(point.index,1);page.geometry=rows} }
                    }
                }
                LanXiButton { text:qsTr("Add node"); enabled:page.geometry.length<32; uiScale:page.uiScale; onClicked:{var rows=page.host.copy(page.geometry);var n=rows.length+1;rows.push({node:n,channel:n,direction:3,x:n,y:0,z:0});page.geometry=rows} }
                Text { text:qsTr("Global XYZ coordinates; signed translation directions ±1=X, ±2=Y, ±3=Z. Wireframe joins nodes in row order."); color:"#9bafc0"; font.pixelSize:12*page.uiScale }
            }
        }
    }
    Grid { id:plots; anchors.top:header.bottom; anchors.topMargin:10*page.uiScale; anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:footer.top; anchors.bottomMargin:8*page.uiScale; columns:2; spacing:10*page.uiScale
        readonly property real cellWidth:Math.max(1,(width-spacing)/2)
        readonly property real cellHeight:Math.max(1,(height-spacing)/2)
        LanXiPlot { width:plots.cellWidth; height:plots.cellHeight; uiScale:page.uiScale; title:qsTr("First response · measured / reconstructed receptance"); values:page.result.measured||[]; comparison:page.result.fitted||[]; axis:qsTr("Retained frequency-line index (exact Hz in export)") }
        LanXiGeometry { width:plots.cellWidth; height:plots.cellHeight; uiScale:page.uiScale; points:page.geometry; shape:page.result.operation==="ods"?page.result.shape||[]:page.modes.length>page.selectedMode?page.modes[page.selectedMode].shape:[]; caption:page.result.operation==="ods"?qsTr("Spectral ODS · normalized displacement"):qsTr("Fitted residue shape · normalized") }
        Rectangle { width:plots.cellWidth; height:plots.cellHeight; color:"#172431"; border.color:"#35495b"
            Flickable { anchors.fill:parent; anchors.margins:12*page.uiScale; contentWidth:width; contentHeight:table.height; clip:true
                Column { id:table; width:parent.width; spacing:6*page.uiScale
                    Text { width:parent.width; text:qsTr("MODE TABLE"); color:"#dbeaf6"; font.pixelSize:15*page.uiScale }
                    Repeater { model:page.modes
                        delegate:LanXiButton { required property var modelData; required property int index; width:table.width; uiScale:page.uiScale; checked:page.selectedMode===index; text:(index+1)+"   "+Number(modelData.frequency).toFixed(3)+" Hz    "+Number(modelData.dampingPercent).toFixed(3)+" % damping"; onClicked:page.selectedMode=index }
                    }
                    Text { width:parent.width; text:page.result.operation==="modal"?"Converged: "+page.result.converged+" | iterations: "+page.result.iterations+"\nResponse fit errors: "+(page.result.relativeErrors||[]).map(function(v){return (v*100).toFixed(3)+"%"}).join(", ")+"\nFRAC: "+(page.result.frac||[]).map(function(v){return Number(v).toFixed(5)}).join(", "):page.result.operation==="ods"?"Frequency: "+Number(page.result.frequency).toFixed(3)+" Hz\nAmplitude unit: "+page.result.unit:qsTr("No structural result yet."); wrapMode:Text.WordWrap; color:"#9bafc0"; font.pixelSize:13*page.uiScale }
                    Text { width:parent.width; text:page.result.method||""; wrapMode:Text.WordWrap; color:"#9bafc0"; font.pixelSize:12*page.uiScale }
                }
            }
        }
        Rectangle { width:plots.cellWidth; height:plots.cellHeight; color:"#172431"; border.color:"#35495b"
            Text { x:12*page.uiScale; y:10*page.uiScale; text:qsTr("AUTO-MAC · fitted mode residue vectors"); color:"#dbeaf6"; font.pixelSize:14*page.uiScale }
            Grid { x:12*page.uiScale; y:42*page.uiScale; columns:Math.max(1,page.modes.length); spacing:3*page.uiScale
                Repeater { model:page.modes.length*page.modes.length
                    delegate:Rectangle { required property int index; readonly property real score:(page.result.mac||[])[Math.floor(index/page.modes.length)][index%page.modes.length]; width:Math.min(64*page.uiScale,(plots.cellWidth-30*page.uiScale)/Math.max(1,page.modes.length)); height:36*page.uiScale; color:Qt.rgba(.1,.3+.4*score,.4+.4*score,1)
                        Text { anchors.centerIn:parent; text:Number(parent.score).toFixed(3); color:"white"; font.pixelSize:12*page.uiScale }
                    }
                }
            }
            Text { anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:parent.bottom; anchors.margins:12*page.uiScale; text:qsTr("Rows and columns follow mode-table order. Too few independent response DOFs can make different modes appear similar."); wrapMode:Text.WordWrap; color:"#9bafc0"; font.pixelSize:12*page.uiScale }
        }
    }
    Text { id:footer; anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:parent.bottom; text:page.result.exportPath||qsTr("Results save automatically with full complex shapes, exact frequencies, fit metrics and reconstructed UFF functions."); wrapMode:Text.WrapAnywhere; color:"#8aa4b8"; font.pixelSize:12*page.uiScale }
}
