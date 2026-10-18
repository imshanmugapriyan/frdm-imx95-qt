pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: view
    property var points: []
    property var shape: []
    property real uiScale: 1
    property bool animated: true
    property real phase: 0
    property real yaw: -0.4
    property real magnification: 0.15
    property string caption: qsTr("Local-reference FRF shape · normalized")
    color: "#142330"; border.color: "#35495b"; radius: 4 * uiScale
    onPointsChanged: canvas.requestPaint()
    onShapeChanged: canvas.requestPaint()
    onPhaseChanged: canvas.requestPaint()
    onYawChanged: canvas.requestPaint()
    onMagnificationChanged: canvas.requestPaint()
    Text { x: 12 * view.uiScale; y: 10 * view.uiScale; text: view.caption; color: "#d7e8f3"; font.pixelSize: 14 * view.uiScale }
    Canvas {
        id: canvas; anchors.fill: parent; anchors.margins: 32 * view.uiScale
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx=getContext("2d");ctx.reset();ctx.clearRect(0,0,width,height)
            if(!view.points.length)return
            var extent=1
            for(var i=0;i<view.points.length;++i)extent=Math.max(extent,Math.abs(view.points[i].x),Math.abs(view.points[i].y),Math.abs(view.points[i].z))
            var scale=Math.min(width,height)*0.36/extent
            function project(p,moving) {
                var px=p.x,py=p.y,z=p.z
                if(moving)for(var k=0;k<view.shape.length;++k) {
                    var dof=view.shape[k],match=dof.node!==undefined ? dof.node===(p.node===undefined?p.channel:p.node) : dof.channel===p.channel
                    if(!match)continue
                    var direction=dof.direction===undefined?3:dof.direction
                    var delta=extent*view.magnification*dof.amplitude*Math.cos(view.phase+dof.phase*Math.PI/180)*(direction<0?-1:1)
                    if(Math.abs(direction)===1)px+=delta
                    else if(Math.abs(direction)===2)py+=delta
                    else if(Math.abs(direction)===3)z+=delta
                }
                var x=px*Math.cos(view.yaw)-py*Math.sin(view.yaw), depth=px*Math.sin(view.yaw)+py*Math.cos(view.yaw)
                return {x:width/2+x*scale,y:height*.62-(z+depth*.45)*scale}
            }
            function draw(moving,color) {
                ctx.strokeStyle=color;ctx.lineWidth=moving?2:1;ctx.beginPath()
                for(var j=0;j<view.points.length;++j){var p=project(view.points[j],moving);if(j===0)ctx.moveTo(p.x,p.y);else ctx.lineTo(p.x,p.y)}ctx.stroke()
                for(var n=0;n<view.points.length;++n){var q=project(view.points[n],moving);ctx.fillStyle=color;ctx.beginPath();ctx.arc(q.x,q.y,moving?4:2,0,2*Math.PI);ctx.fill()
                    if(moving){ctx.font=Math.round(12*view.uiScale)+"px sans-serif";ctx.fillText("Node "+(view.points[n].node===undefined?view.points[n].channel:view.points[n].node),q.x+8,q.y-8)}}
            }
            draw(false,"#536b7e");draw(true,"#5ad4db")
        }
    }
    MouseArea { anchors.fill: canvas; property real startX: 0; property real startYaw: 0
        onPressed: function(mouse) {startX=mouse.x;startYaw=view.yaw}
        onPositionChanged: function(mouse) {if(pressed)view.yaw=startYaw+(mouse.x-startX)/200}
        onWheel: function(wheel) {view.magnification=Math.max(.01,Math.min(1,view.magnification+wheel.angleDelta.y/1200))} }
    Timer { interval: 40; repeat: true; running: view.visible && view.animated && view.shape.length>0; onTriggered: view.phase=(view.phase+0.12)%(2*Math.PI) }
    LanXiButton { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 8 * view.uiScale; text: view.animated ? qsTr("Pause") : qsTr("Animate"); uiScale: view.uiScale; onClicked: view.animated=!view.animated }
    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 10 * view.uiScale; text: qsTr("Drag to rotate · wheel scales motion"); color: "#8aa4b8"; font.pixelSize: 11 * view.uiScale }
}
