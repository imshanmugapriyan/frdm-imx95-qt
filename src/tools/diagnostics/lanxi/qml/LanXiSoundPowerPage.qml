pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Item {
    id:page
    required property var host
    property real uiScale:1
    property string weighting:"Z"
    property real environmentalCorrection:0
    property real minimumSeparation:6
    property var positions:[{area:1,measuredDb:null,backgroundDb:null}]
    readonly property var result:host.status.acousticResult||({})
    property bool restored:false
    function workspaceSettings(){return {weighting:weighting,environmentalCorrection:environmentalCorrection,minimumSeparation:minimumSeparation,positions:positions}}
    Connections { target:page.host.backend
        function onChanged(){var saved=page.host.status.workspace;if(page.restored||!saved||saved.schema!==1)return;page.restored=true;var p=saved.soundPower;if(!p)return;page.weighting=p.weighting;page.environmentalCorrection=p.environmentalCorrection;page.minimumSeparation=p.minimumSeparation;page.positions=p.positions}
    }
    readonly property var levels:host.plot.analysis||({})
    readonly property bool canUseLevel:host.idle && host.plot.unit==="Pa" && levels.weighting===weighting && levels.leq!==undefined && levels.leq!==null && isFinite(levels.leq)
    function edit(index,key,value){var rows=host.copy(positions);rows[index][key]=value;if(key!=="area")rows[index][key+"Source"]="manual";positions=rows}
    function take(index,key){if(!canUseLevel)return;var rows=host.copy(positions);rows[index][key]=levels.leq;rows[index][key+"Source"]={runId:host.status.runId,device:host.device,channel:host.channel,seconds:levels.seconds,weighting:weighting};positions=rows}
    Column { id:heading; width:parent.width; spacing:10*page.uiScale
        Text { text:qsTr("Surface sound power"); color:"#e6f4fc"; font.pixelSize:24*page.uiScale }
        Text { width:parent.width; wrapMode:Text.WordWrap; text:qsTr("Engineering worksheet for an enveloping measurement surface. Supply representative source-on and background Leq readings at every position using the same weighting. Assign each position its surface area. Fixture, environment and standards conformity require separate qualification."); color:"#9bafc0"; font.pixelSize:13*page.uiScale }
        Flow { width:parent.width; spacing:12*page.uiScale; enabled:page.host.idle
            LanXiSelect { width:130*page.uiScale; label:qsTr("Weighting"); value:page.weighting; options:["Z","A","C"]; uiScale:page.uiScale; onSelected:function(value){page.weighting=value;page.positions=[{area:1,measuredDb:null,backgroundDb:null}]} }
            ValueField { width:200*page.uiScale; label:qsTr("Environment K2 (dB)"); value:page.environmentalCorrection; minimum:0; maximum:30; uiScale:page.uiScale*.8; onEdited:function(value){page.environmentalCorrection=value} }
            ValueField { width:200*page.uiScale; label:qsTr("Minimum separation (dB)"); value:page.minimumSeparation; minimum:6; maximum:30; uiScale:page.uiScale*.8; onEdited:function(value){page.minimumSeparation=value} }
            LanXiButton { text:qsTr("Add position"); uiScale:page.uiScale; enabled:page.positions.length<256; onClicked:{var rows=page.host.copy(page.positions);rows.push({area:1,measuredDb:null,backgroundDb:null});page.positions=rows} }
            LanXiButton { text:qsTr("Calculate and save"); primary:true; uiScale:page.uiScale; onClicked:page.host.backend.soundPower({positions:page.positions,weighting:page.weighting,environmentCorrection:page.environmentalCorrection,minimumSeparation:page.minimumSeparation}) }
        }
        Text { width:parent.width; wrapMode:Text.WordWrap; text:page.result.error||qsTr("Leq buttons copy the selected stopped/replayed Pa channel. Readings are required; displayed zeros in empty fields are not treated as measurements. Changing weighting clears the worksheet."); color:page.result.error?"#ffacb5":"#9bafc0"; font.pixelSize:12*page.uiScale }
    }
    Flickable { id:rowsView; anchors.top:heading.bottom; anchors.topMargin:14*page.uiScale; anchors.left:parent.left; anchors.right:summary.left; anchors.rightMargin:14*page.uiScale; anchors.bottom:parent.bottom; contentWidth:width; contentHeight:rows.height; clip:true
        Column { id:rows; width:parent.width; spacing:12*page.uiScale; enabled:page.host.idle
            Repeater { model:page.positions
                delegate:Rectangle { id:position; required property var modelData; required property int index; width:rows.width; height:controls.height+24*page.uiScale; color:"#172431"; border.color:"#35495b"
                    Column { id:controls; x:12*page.uiScale; y:12*page.uiScale; width:parent.width-24*page.uiScale; spacing:8*page.uiScale
                        Text { text:"Position "+(position.index+1)+(position.modelData.measuredDb===null||position.modelData.backgroundDb===null?" · readings required":" · ready"); color:"#dbeaf6"; font.pixelSize:15*page.uiScale }
                        Flow { width:parent.width; spacing:8*page.uiScale
                            ValueField { width:140*page.uiScale; label:qsTr("Area (m²)"); value:position.modelData.area; minimum:.000001; maximum:1e8; uiScale:page.uiScale*.8; onEdited:function(value){page.edit(position.index,"area",value)} }
                            ValueField { width:150*page.uiScale; label:qsTr("Source-on (dB)"); value:position.modelData.measuredDb===null?0:position.modelData.measuredDb; minimum:-100; maximum:250; uiScale:page.uiScale*.8; onEdited:function(value){page.edit(position.index,"measuredDb",value)} }
                            ValueField { width:150*page.uiScale; label:qsTr("Background (dB)"); value:position.modelData.backgroundDb===null?0:position.modelData.backgroundDb; minimum:-100; maximum:250; uiScale:page.uiScale*.8; onEdited:function(value){page.edit(position.index,"backgroundDb",value)} }
                        }
                        Flow { width:parent.width; spacing:8*page.uiScale
                            LanXiButton { text:qsTr("Source-on ← Leq"); enabled:page.canUseLevel; uiScale:page.uiScale*.85; onClicked:page.take(position.index,"measuredDb") }
                            LanXiButton { text:qsTr("Background ← Leq"); enabled:page.canUseLevel; uiScale:page.uiScale*.85; onClicked:page.take(position.index,"backgroundDb") }
                            LanXiButton { text:qsTr("Remove"); enabled:page.positions.length>1; uiScale:page.uiScale*.85; onClicked:{var next=page.host.copy(page.positions);next.splice(position.index,1);page.positions=next} }
                        }
                    }
                }
            }
        }
    }
    Rectangle { id:summary; anchors.top:rowsView.top; anchors.right:parent.right; anchors.bottom:parent.bottom; width:Math.min(360*page.uiScale,parent.width*.36); color:"#172431"; border.color:"#35495b"
        Flickable { anchors.fill:parent; anchors.margins:18*page.uiScale; contentWidth:width; contentHeight:report.height; clip:true
            Text { id:report; width:parent.width; color:"#dbeaf6"; font.pixelSize:18*page.uiScale; wrapMode:Text.WrapAnywhere; textFormat:Text.PlainText
                text:page.result.powerDb===undefined?qsTr("SOUND POWER\n\nComplete the position readings to calculate."):Number(page.result.powerDb).toFixed(2)+" dB("+page.result.weighting+") re 1 pW\n\nSurface area: "+Number(page.result.area).toPrecision(5)+" m²\nSurface SPL: "+Number(page.result.surfaceDb).toFixed(2)+" dB\nBackground separation: "+Number(page.result.separationDb).toFixed(2)+" dB\nK1: "+Number(page.result.k1).toFixed(3)+" dB\nK2: "+Number(page.result.k2).toFixed(3)+" dB\n\n"+page.result.qualification+"\n\n"+page.result.exportPath }
        }
    }
}
