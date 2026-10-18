pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls" as Controls

Item {
    id: settings
    required property var files
    required property var preferences
    property var systemInfo: ({available:false,platform:""})
    property string message: ""
    signal preferencesRequested(var values)
    function change(key,value) {
        var next={wallpaper:preferences.wallpaper,clock24:preferences.clock24}
        next[key]=value; preferencesRequested(next)
    }
    Text { x: 28; y: 24; text: qsTr("Make this workspace yours"); color: "#f4eeff"; font.pixelSize: 30; font.weight: Font.DemiBold }
    Controls.ChoiceField {
        x: 28; y: 102; label: qsTr("Wallpaper"); options: ["Dusk","Midnight"]
        value: settings.preferences.wallpaper === "dusk" ? "Dusk" : "Midnight"
        onSelected: function(value) { settings.change("wallpaper",value.toLowerCase()) }
    }
    Controls.ChoiceField {
        x: 28; y: 226; label: qsTr("Clock format"); options: ["24-hour","12-hour"]
        value: settings.preferences.clock24 ? "24-hour" : "12-hour"
        onSelected: function(value) { settings.change("clock24",value === "24-hour") }
    }
    Controls.ActionButton {
        x: 28; y: 356; text: qsTr("Restore desktop defaults")
        onClicked: settings.preferencesRequested({wallpaper:"dusk",clock24:true})
    }
    Text { x: 28; y: 430; width: 500; wrapMode: Text.WordWrap; text: settings.message
        textFormat: Text.PlainText; color: "#cbbedf"; font.pixelSize: 18 }
    Rectangle {
        x: 620; y: 24; width: parent.width-x-28; height: parent.height-48; radius: 18; color: "#111c2f"; border.color: "#3b4965"
        Column {
            x: 28; y: 28; width: parent.width-56; spacing: 24
            Text { text: "Suno"; color: "#ffffff"; font.pixelSize: 44; font.weight: Font.Light }
            Text { text: qsTr("Diagnostic desktop"); color: "#c9bfdb"; font.pixelSize: 23 }
            Text { width: parent.width; wrapMode: Text.WordWrap
                text: qsTr("System: %1").arg(settings.systemInfo.platform || qsTr("Unavailable"))
                color: "#aebdd5"; font.pixelSize: 18 }
            Text { text: qsTr("Designed for 1920 × 1080"); color: "#aebdd5"; font.pixelSize: 18 }
            Text { width: parent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
                text: qsTr("Results and note exports\n%1").arg(settings.files.outputDirectory)
                color: "#aebdd5"; font.pixelSize: 17 }
            Text { width: parent.width; wrapMode: Text.WordWrap
                text: qsTr("Your diagnostic workspaces and supporting tools, together in one desktop.")
                color: "#c9bfdb"; font.pixelSize: 20 }
        }
    }
}
