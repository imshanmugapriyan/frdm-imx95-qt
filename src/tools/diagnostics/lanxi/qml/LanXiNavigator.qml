pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: nav
    required property var host
    property real uiScale: 1
    color: "#172431"; border.color: "#35495b"
    Flickable {
        anchors.fill: parent; anchors.margins: 10 * nav.uiScale; clip: true
        contentWidth: width; contentHeight: tree.height; boundsBehavior: Flickable.StopAtBounds
        Column {
            id: tree; width: parent.width; spacing: 6 * nav.uiScale
            Text { width: parent.width; text: qsTr("PROJECT BROWSER"); color: "#8aa4b8"; font.pixelSize: 12 * nav.uiScale; font.letterSpacing: 1 }
            Text { width: parent.width; text: nav.host.config.name || qsTr("Measurement"); color: "#e6f4fc"; font.pixelSize: 18 * nav.uiScale; elide: Text.ElideRight }
            Repeater {
                model: [{page:"Setup",label:"01  Hardware setup"},{page:"Calibrate",label:"02  Transducer calibration"},{page:"Stimulus",label:"03  Signal generator"},{page:"Analysis",label:"04  Acquisition processing"},{page:"Measure",label:"05  Measurement displays"},{page:"Processing",label:"06  Time data processing"},{page:"Orders",label:"07  Order analysis"},{page:"Structural",label:"08  Isolated-mode fit"},{page:"Modal / ODS",label:"09  Modal / ODS"},{page:"Limits",label:"10  Acceptance limits"},{page:"Interchange",label:"11  UFF interchange"},{page:"Sound power",label:"12  Surface sound power"},{page:"Review",label:"13  Data and reporting"}]
                delegate: LanXiButton {
                    required property var modelData
                    width: tree.width; text: modelData.label; uiScale: nav.uiScale; checked: nav.host.page === modelData.page
                    onClicked: {nav.host.page = modelData.page; if(modelData.page === "Review" && nav.host.idle) nav.host.backend.refreshRuns()}
                }
            }
            Item { width: 1; height: 12 * nav.uiScale }
            Text { text: qsTr("HARDWARE / CHANNELS"); color: "#8aa4b8"; font.pixelSize: 12 * nav.uiScale; font.letterSpacing: 1 }
            Repeater {
                model: nav.host.configured ? nav.host.config.devices : []
                delegate: Column {
                    id: moduleNode
                    required property int index
                    required property var modelData
                    width: tree.width; spacing: 3 * nav.uiScale
                    Text { width: parent.width; text: (moduleNode.index === 0 ? "3676" : "3677") + "  ·  " + (moduleNode.modelData.serial || qsTr("not inspected")); color: "#77cde7"; font.pixelSize: 14 * nav.uiScale; elide: Text.ElideRight }
                    Repeater { model: moduleNode.modelData.channels
                        delegate: LanXiButton {
                            required property int index
                            required property var modelData
                            width: tree.width; uiScale: nav.uiScale
                            text: (modelData.enabled ? "●  " : "○  ") + (index + 1) + "  " + modelData.name + (moduleNode.modelData.reference === index + 1 ? "  [R]" : "")
                            checked: nav.host.device === moduleNode.index && nav.host.channel === index + 1
                            onClicked: nav.host.selectChannel(moduleNode.index, index + 1)
                        }
                    }
                }
            }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Two independent clock domains. Use a reference or tachometer on the selected module."); color: "#9bafc0"; font.pixelSize: 12 * nav.uiScale }
        }
    }
}
