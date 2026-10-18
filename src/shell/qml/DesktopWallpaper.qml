import QtQuick
import QtQuick.Window

Canvas {
    id: wallpaper
    property string wallpaperStyle: "dusk"
    readonly property real density: Screen.devicePixelRatio
    canvasSize: Qt.size(Math.ceil(width*density), Math.ceil(height*density))
    onWallpaperStyleChanged: requestPaint()
    onCanvasSizeChanged: requestPaint()
    onAvailableChanged: if (available) requestPaint()
    // Original static landscape. Repaint only on size/style changes.
    onPaint: {
        var p=getContext("2d")
        p.reset()
        p.scale(canvasSize.width/1920,canvasSize.height/1080)
        var midnight=wallpaperStyle === "midnight"
        var sky=p.createLinearGradient(0,0,0,1080)
        sky.addColorStop(0,midnight ? "#101c36" : "#282949")
        sky.addColorStop(0.5,midnight ? "#304167" : "#73627e")
        sky.addColorStop(0.69,midnight ? "#5e6587" : "#d68d8e")
        sky.addColorStop(1,"#17263f")
        p.fillStyle=sky; p.fillRect(0,0,1920,1080)
        var glow=p.createRadialGradient(430,600,20,430,600,700)
        glow.addColorStop(0,midnight ? "rgba(48,76,103,0.5)" : "rgba(233,171,147,0.44)")
        glow.addColorStop(1,"rgba(0,0,0,0)")
        p.fillStyle=glow; p.fillRect(0,0,1920,1080)
        function ridge(points,color) {
            p.beginPath(); p.moveTo(0,1080)
            for(var i=0;i<points.length;++i) p.lineTo(points[i][0],points[i][1])
            p.lineTo(1920,1080); p.closePath(); p.fillStyle=color; p.fill()
        }
        ridge([[0,650],[160,520],[270,595],[480,410],[650,600],[770,530],
               [910,650],[1090,455],[1230,555],[1450,350],[1690,560],[1820,510],[1920,600]],"#404563")
        ridge([[0,765],[200,665],[340,710],[490,595],[650,770],[830,615],
               [980,740],[1190,590],[1330,675],[1510,540],[1710,695],[1920,640]],"#2b3554")
        p.fillStyle="rgba(191,193,222,0.45)"
        p.beginPath(); p.moveTo(1383,431); p.lineTo(1450,350); p.lineTo(1549,437)
        p.lineTo(1480,417); p.lineTo(1454,395); p.lineTo(1427,425); p.closePath(); p.fill()
        var lake=p.createLinearGradient(0,760,0,1080)
        lake.addColorStop(0,midnight ? "#2b3b59" : "#655773"); lake.addColorStop(1,"#131e34")
        p.fillStyle=lake; p.fillRect(0,800,1920,280)
        p.strokeStyle="rgba(169,154,186,0.12)"; p.lineWidth=1
        for(var line=0;line<20;++line) {
            var y=810+line*14, inset=60+(line%5)*70
            p.beginPath(); p.moveTo(inset,y); p.lineTo(1920-inset,y); p.stroke()
        }
        ridge([[0,870],[120,849],[245,920],[380,946],[550,1065],[700,1080],[1920,1080]],"#10192b")
        ridge([[0,1080],[1390,1080],[1520,996],[1680,968],[1815,868],[1920,841]],"#101a2e")
    }
}
