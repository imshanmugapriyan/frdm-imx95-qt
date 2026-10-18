pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls"

Column {
    id: panel
    required property var host
    property real uiScale: 1
    property string networkPrefix: "192.168.1"
    property string sensorName: ""
    spacing: 18 * uiScale
    Text { text: qsTr("Network and transducers"); color: "#eee5fa"; font.pixelSize: Math.round(26 * panel.uiScale) }
    Flow {
        width: parent.width; spacing: 16 * panel.uiScale; enabled: panel.host.idle
        LanXiText { label: qsTr("Discovery network (/24 prefix)"); value: panel.networkPrefix; uiScale: panel.uiScale
            onEdited: function(value) { panel.networkPrefix=value } }
        LanXiButton { text: qsTr("Find modules"); uiScale: panel.uiScale; onClicked: panel.host.backend.discover(panel.networkPrefix) }
        LanXiButton { text: qsTr("Detect TEDS on selected module"); uiScale: panel.uiScale; enabled: panel.host.backend.ready
            onClicked: panel.host.backend.detectTransducers(panel.host.device) }
    }
    Flow {
        width: parent.width; spacing: 12 * panel.uiScale; enabled: panel.host.idle
        Repeater { model: panel.host.status.discovered || []
            delegate: LanXiButton {
                required property var modelData
                text: modelData.type+" · "+modelData.serial+" · "+modelData.url; uiScale: panel.uiScale
                onClicked: panel.host.edit(["devices",modelData.type === "3676" ? 0 : 1,"url"],modelData.url)
            }
        }
    }
    Repeater {
        model: (panel.host.status.transducers || []).filter(function(t) { return t.device===panel.host.device && t.channel===panel.host.channel })
        delegate: Column {
            id: transducer
            required property var modelData
            width: panel.width; spacing: 10 * panel.uiScale
            Text { width: parent.width; wrapMode: Text.WordWrap; textFormat: Text.PlainText; color: "#c9bad9"; font.pixelSize: Math.round(17 * panel.uiScale)
                text: transducer.modelData.available ? qsTr("TEDS · serial %1 · sensitivity %2 V/%3 · CCLD required: %4 · 200 V required: %5")
                    .arg(transducer.modelData.serialNumber || "unknown").arg(transducer.modelData.sensitivity || "unknown").arg(transducer.modelData.unit || "unknown")
                    .arg(transducer.modelData.requiresCcld ? "yes" : "no").arg(transducer.modelData.requires200V ? "yes" : "no") : qsTr("No TEDS transducer detected on this channel.") }
            LanXiButton { text: qsTr("Apply TEDS sensitivity and unit"); uiScale: panel.uiScale
                enabled: panel.host.idle && transducer.modelData.available && transducer.modelData.sensitivity > 0
                    && (transducer.modelData.unit === "V" || (transducer.modelData.unit === "Pa" && panel.host.config.workflow !== "amplifier"))
                onClicked: panel.host.applySensor({unit:transducer.modelData.unit,sensitivity:transducer.modelData.sensitivity,sensorSerial:String(transducer.modelData.serialNumber || "")}) }
        }
    }
    Text { text: qsTr("Sensor library"); color: "#eee5fa"; font.pixelSize: Math.round(26 * panel.uiScale) }
    Flow {
        width: parent.width; spacing: 16 * panel.uiScale; enabled: panel.host.idle
        LanXiText { label: qsTr("Sensor library name"); value: panel.sensorName; uiScale: panel.uiScale; onEdited: function(value) { panel.sensorName=value } }
        LanXiText { label: qsTr("Sensor serial number"); value: panel.host.currentChannel.sensorSerial || ""; uiScale: panel.uiScale
            onEdited: function(value) { panel.host.editChannel("sensorSerial",value) } }
        LanXiButton { text: qsTr("Save selected sensor"); uiScale: panel.uiScale; onClicked: panel.host.backend.saveSensor(panel.sensorName,panel.host.currentChannel) }
        LanXiSelect { label: qsTr("Apply library sensor"); uiScale: panel.uiScale; value: ""
            options: panel.host.options(panel.host.status.sensors,"id","name")
            onSelected: function(value) {
                var rows=panel.host.status.sensors || []
                for(var i=0;i<rows.length;++i)if(rows[i].id===value) {
                    if(panel.host.config.workflow!=="amplifier" || rows[i].channel.unit==="V")panel.host.applySensor(rows[i].channel)
                }
            } }
    }
    Text { width: parent.width; wrapMode: Text.WordWrap; color: "#baaecd"; font.pixelSize: Math.round(17 * panel.uiScale)
        text: qsTr("Applying a sensor updates its sensitivity, unit and calibration metadata. Review CCLD and polarization separately against the connected transducer. TEDS with unsupported engineering units remains visible but cannot be applied.") }
}
