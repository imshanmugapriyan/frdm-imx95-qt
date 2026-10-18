pragma ComponentBehavior: Bound
import QtQuick
import "../../shared/qml/controls"

FocusScope {
    id: workspace
    required property var session
    property var tools: []
    property string toolId: ""
    property string toolTitle: ""
    property bool expanded: false
    readonly property real uiScale: Math.max(0.01,Math.min(width/1920,height/1080))
    signal homeRequested()
    signal restoreViewRequested()
    clip: true
    Keys.onEscapePressed: function(event) {
        if (!session.busy) homeRequested()
        event.accepted=true
    }
    Rectangle { anchors.fill: parent; color: "#171222" }
    // Keep navigation compact; the active tool uses the rest of the viewport.
    Item {
        id: toolHeader
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 60*workspace.uiScale
        Text {
            anchors.left: parent.left; anchors.leftMargin: 20*workspace.uiScale
            anchors.right: navigation.left; anchors.rightMargin: 16*workspace.uiScale
            anchors.verticalCenter: parent.verticalCenter
            text: workspace.toolTitle
            color: "#e8dbf7"; font.pixelSize: Math.round(24*workspace.uiScale); font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        ActionButton {
            id: navigation
            anchors.right: parent.right; anchors.rightMargin: 20*workspace.uiScale
            anchors.verticalCenter: parent.verticalCenter
            text: workspace.expanded ? qsTr("Exit full screen") : qsTr("Desktop")
            height: 40*workspace.uiScale; uiScale: workspace.uiScale
            enabled: workspace.expanded || !workspace.session.busy
            onClicked: {
                if (workspace.expanded) workspace.restoreViewRequested()
                else workspace.homeRequested()
            }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: "#46354f" }
    }
    DiagnosticToolHost {
        id: toolArea
        session: workspace.session; tools: workspace.tools
        toolId: workspace.toolId; uiScale: workspace.uiScale
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20*workspace.uiScale; anchors.rightMargin: 20*workspace.uiScale
        anchors.top: toolHeader.bottom; anchors.topMargin: 12*workspace.uiScale
        anchors.bottom: feedback.top; anchors.bottomMargin: 8*workspace.uiScale
    }
    Text {
        id: feedback
        anchors.left: toolArea.left; anchors.right: toolArea.right
        anchors.bottom: parent.bottom; anchors.bottomMargin: 10*workspace.uiScale
        height: text.length ? 26*workspace.uiScale : 0
        text: workspace.session.feedback
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle; color: "#c9bad9"; font.pixelSize: Math.round(16*workspace.uiScale)
    }
}
