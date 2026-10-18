pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Item {
    id: page
    required property var host
    property real uiScale: 1
    property string filePath: ""
    property string exportKind: "frf"
    property int responseNode: 1
    property int responseDirection: 0
    property int referenceNode: 1
    property int referenceDirection: 0
    readonly property var importState: host.status.interchange || ({})
    readonly property var selected: importState.selected || ({})
    Column {
        id: setup; width:parent.width; spacing:12*page.uiScale
        Text { text:qsTr("UFF function interchange"); color:"#e6f4fc"; font.pixelSize:24*page.uiScale }
        Text { width:parent.width; text:qsTr("Import ASCII UFF 58 time histories, spectra and complex FRFs. Original units and node/direction metadata are retained. Other datasets are listed as skipped; geometry transforms and binary 58b are not interpreted."); color:"#9bafc0"; wrapMode:Text.WordWrap; font.pixelSize:13*page.uiScale }
        Flow { width:parent.width; spacing:10*page.uiScale; enabled:page.host.idle
            LanXiText { width:Math.max(300,setup.width-200*page.uiScale); label:qsTr("Local file path (.uff / .unv)"); value:page.filePath; uiScale:page.uiScale; onEdited:function(value){page.filePath=value} }
            LanXiButton { text:qsTr("Import file"); primary:true; uiScale:page.uiScale; onClicked:page.host.backend.importUff(page.filePath) }
            LanXiSelect { width:360*page.uiScale; label:qsTr("Imported function"); value:page.selected.index===undefined?-1:page.selected.index; options:page.host.options(page.importState.functions,"index","name"); uiScale:page.uiScale; onSelected:function(value){page.host.backend.selectUff(value)} }
            LanXiButton { text:qsTr("Export selected function"); uiScale:page.uiScale; enabled:page.selected.index!==undefined; onClicked:page.host.backend.exportUff({source:"import",index:page.selected.index}) }
        }
        Text { width:parent.width; text:page.importState.error || (page.selected.samples===undefined?qsTr("No file imported."):page.selected.samples+" values | "+page.selected.start+" to "+page.selected.end+" "+page.selected.xUnit+" | response "+page.selected.responseNode+":"+page.selected.responseDirection+" / reference "+page.selected.referenceNode+":"+page.selected.referenceDirection+" | skipped datasets: "+(page.importState.skipped||[]).join(", ")); color:page.importState.error?"#ffacb5":"#80d4ba"; wrapMode:Text.WrapAnywhere; font.pixelSize:13*page.uiScale }
        Rectangle { width:parent.width; height:1; color:"#35495b" }
        Text { text:qsTr("Export captured / replayed channel"); color:"#dbeaf6"; font.pixelSize:18*page.uiScale }
        Flow { width:parent.width; spacing:12*page.uiScale; enabled:page.host.idle
            LanXiSelect { width:180*page.uiScale; label:qsTr("Function"); value:page.exportKind; options:["frf","spectrum","psd"]; uiScale:page.uiScale; onSelected:function(value){page.exportKind=value} }
            ValueField { width:135*page.uiScale; label:qsTr("Response node"); value:page.responseNode; minimum:1; maximum:999999; uiScale:page.uiScale*.8; onEdited:function(value){page.responseNode=Math.round(value)} }
            LanXiSelect { width:170*page.uiScale; label:qsTr("Response direction"); value:page.responseDirection; options:[{value:0,label:"Scalar"},{value:1,label:"+X"},{value:2,label:"+Y"},{value:3,label:"+Z"},{value:-1,label:"-X"},{value:-2,label:"-Y"},{value:-3,label:"-Z"}]; uiScale:page.uiScale; onSelected:function(value){page.responseDirection=value} }
            ValueField { width:135*page.uiScale; label:qsTr("Reference node"); value:page.referenceNode; minimum:1; maximum:999999; uiScale:page.uiScale*.8; onEdited:function(value){page.referenceNode=Math.round(value)} }
            LanXiSelect { width:170*page.uiScale; label:qsTr("Reference direction"); value:page.referenceDirection; options:[0,1,2,3,-1,-2,-3]; uiScale:page.uiScale; onSelected:function(value){page.referenceDirection=value} }
            LanXiButton { text:qsTr("Export channel function"); uiScale:page.uiScale; onClicked:page.host.backend.exportUff({source:"capture",kind:page.exportKind,responseNode:page.responseNode,responseDirection:page.responseDirection,referenceNode:page.referenceNode,referenceDirection:page.referenceDirection}) }
        }
    }
    Row { anchors.top:setup.bottom; anchors.topMargin:14*page.uiScale; anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:footer.top; anchors.bottomMargin:10*page.uiScale; spacing:10*page.uiScale
        LanXiPlot { width:(parent.width-parent.spacing)/2; height:parent.height; uiScale:page.uiScale; title:page.selected.complex?qsTr("Imported magnitude"):qsTr("Imported values"); values:page.selected.amplitude||[]; xStep:page.selected.previewStride||1; axis:qsTr("Data point index (decimated preview)") }
        LanXiPlot { width:(parent.width-parent.spacing)/2; height:parent.height; uiScale:page.uiScale; title:qsTr("Imported phase · degrees"); values:page.selected.complex?page.selected.phase||[]:[]; xStep:page.selected.previewStride||1; axis:qsTr("Data point index"); accent:"#6bcaff" }
    }
    Text { id:footer; anchors.left:parent.left; anchors.right:parent.right; anchors.bottom:parent.bottom; text:page.importState.exportPath||qsTr("Full-recording time-data export is available under Time data processing → Entire recording."); color:"#9bafc0"; wrapMode:Text.WrapAnywhere; font.pixelSize:12*page.uiScale }
}
