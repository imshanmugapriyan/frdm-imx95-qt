pragma ComponentBehavior: Bound
import QtQuick
import "../../shared/qml/controls" as Controls

Rectangle {
    id: frame
    property string title: ""
    property bool maximized: false
    property point restoredPosition: Qt.point(330,135)
    property alias contentItem: body
    signal closeRequested()
    signal minimizeRequested()
    color: "#f51b2236"; border.color: "#706c8c"; radius: maximized ? 0 : 22
    x: maximized ? 0 : restoredPosition.x
    y: maximized ? 62 : restoredPosition.y
    width: maximized ? parent.width : Math.min(1260,parent.width-64)
    height: maximized ? parent.height-190 : Math.min(760,parent.height-240)
    MouseArea { anchors.fill: parent; onClicked: frame.forceActiveFocus() }
    // Drag only the title bar. Stored coordinates stay within the desktop work area.
    Rectangle {
        x: 1; y: 1; width: parent.width-2; height: 64
        color: "#172033"; radius: frame.radius
        Text { x: 26; anchors.verticalCenter: parent.verticalCenter; text: frame.title
            color: "#f6f2ff"; font.pixelSize: 22; font.weight: Font.DemiBold }
        MouseArea {
            width: parent.width-350; height: parent.height
            property point startPoint
            property point startPosition
            onPressed: function(mouse) { startPoint=mapToItem(frame.parent,mouse.x,mouse.y); startPosition=frame.restoredPosition }
            onPositionChanged: function(mouse) {
                if (!pressed || frame.maximized) return
                var point=mapToItem(frame.parent,mouse.x,mouse.y)
                frame.restoredPosition=Qt.point(Math.max(12,Math.min(frame.parent.width-frame.width-12,startPosition.x+point.x-startPoint.x)),
                    Math.max(62,Math.min(frame.parent.height-frame.height-122,startPosition.y+point.y-startPoint.y)))
            }
            onDoubleClicked: frame.maximized=!frame.maximized
        }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 6
            Controls.ActionButton { width: 92; text: qsTr("Hide"); onClicked: frame.minimizeRequested() }
            Controls.ActionButton { width: 104; text: frame.maximized ? qsTr("Restore") : qsTr("Expand"); onClicked: frame.maximized=!frame.maximized }
            Controls.ActionButton { width: 92; text: qsTr("Close"); onClicked: frame.closeRequested() }
        }
    }
    Item { id: body; x: 20; y: 84; width: parent.width-40; height: parent.height-104; clip: true }
}
