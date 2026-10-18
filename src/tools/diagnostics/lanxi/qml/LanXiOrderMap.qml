pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: panel
    property var frames: []
    property var rpm: []
    property real orderStep: 1
    property real uiScale: 1
    readonly property real minimumRpm: rpm.length ? Math.min.apply(Math,rpm) : 0
    readonly property real maximumRpm: rpm.length ? Math.max.apply(Math,rpm) : 1
    color:"#172431"; border.color:"#35495b"; radius:4*uiScale
    onFramesChanged: map.requestPaint()
    onRpmChanged: map.requestPaint()
    Text { x:12*panel.uiScale; y:10*panel.uiScale; text:qsTr("Order map · energy averaged in RPM bins"); color:"#d7e8f3"; font.pixelSize:14*panel.uiScale }
    Canvas {
        id:map; anchors.fill:parent; anchors.leftMargin:64*panel.uiScale; anchors.rightMargin:16*panel.uiScale; anchors.topMargin:38*panel.uiScale; anchors.bottomMargin:38*panel.uiScale
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        onPaint:{
            var ctx=getContext("2d");ctx.reset();ctx.clearRect(0,0,width,height)
            if(!panel.frames.length||panel.frames.length!==panel.rpm.length)return
            var bins=64,columns=panel.frames[0].length,levels=[],counts=[],peak=1e-24
            for(var r=0;r<bins;++r){levels.push(new Array(columns).fill(0));counts.push(0)}
            var span=panel.maximumRpm-panel.minimumRpm
            for(var frame=0;frame<panel.frames.length;++frame){var row=span>0?Math.min(bins-1,Math.floor((panel.rpm[frame]-panel.minimumRpm)/span*bins)):0;++counts[row]
                for(var k=0;k<columns;++k){var v=panel.frames[frame][k];if(v!==null&&isFinite(v))levels[row][k]+=v*v}}
            for(var y=0;y<bins;++y)if(counts[y])for(var x=0;x<columns;++x){levels[y][x]/=counts[y];peak=Math.max(peak,levels[y][x])}
            for(var yy=0;yy<bins;++yy)if(counts[yy])for(var xx=0;xx<columns;++xx){var db=levels[yy][xx]>0?10*Math.log(levels[yy][xx]/peak)/Math.LN10:-80;var intensity=Math.max(0,Math.min(1,(db+80)/80));ctx.fillStyle=Qt.hsla(.68-.65*intensity,.8,.12+.48*intensity,1);ctx.fillRect(xx*width/columns,(bins-1-yy)*height/bins,Math.ceil(width/columns),Math.ceil(height/bins))}
        }
    }
    Text { anchors.left:parent.left; anchors.top:map.top; anchors.leftMargin:5*panel.uiScale; text:panel.maximumRpm.toFixed(0); color:"#9bafc0"; font.pixelSize:11*panel.uiScale }
    Text { anchors.left:parent.left; anchors.bottom:map.bottom; anchors.leftMargin:5*panel.uiScale; text:panel.minimumRpm.toFixed(0)+"\nRPM"; color:"#9bafc0"; font.pixelSize:11*panel.uiScale }
    Text { anchors.left:map.left; anchors.bottom:parent.bottom; anchors.bottomMargin:10*panel.uiScale; text:qsTr("Order 0 – %1").arg(panel.frames.length?((panel.frames[0].length-1)*panel.orderStep).toFixed(1):"—"); color:"#9bafc0"; font.pixelSize:12*panel.uiScale }
    Text { anchors.right:map.right; anchors.bottom:parent.bottom; anchors.bottomMargin:10*panel.uiScale; text:qsTr("Colour: −80 to 0 dB re map peak"); color:"#9bafc0"; font.pixelSize:11*panel.uiScale }
}
