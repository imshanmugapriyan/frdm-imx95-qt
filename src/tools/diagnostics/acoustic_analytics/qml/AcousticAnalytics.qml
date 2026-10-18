pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

// Public diagnostic entry point. Subpages and their state belong to this tool.
Item {
    id: tool
    required property var session
    property real uiScale: 1
    property string acousticPage: "DSP Calibration"

    Row {
        id: acousticTabs
        spacing: 12*tool.uiScale
        Repeater {
            model: ["DSP Calibration","Automatic Mic Calibration"]
            delegate: ActionButton {
                required property string modelData
                text: modelData; checked: tool.acousticPage === modelData; uiScale: tool.uiScale
                enabled: !tool.session.busy || checked
                onClicked: tool.acousticPage=modelData
            }
        }
    }
    DspCalibration {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.top: acousticTabs.bottom; anchors.topMargin: 12*tool.uiScale
        visible: tool.acousticPage === "DSP Calibration"; uiScale: tool.uiScale; session: tool.session
    }
    AutomaticMicCalibration {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.top: acousticTabs.bottom; anchors.topMargin: 12*tool.uiScale
        visible: tool.acousticPage === "Automatic Mic Calibration"; uiScale: tool.uiScale; session: tool.session
    }
}
