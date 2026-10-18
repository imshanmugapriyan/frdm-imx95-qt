pragma ComponentBehavior: Bound
import QtQuick
import "../../shared/qml/controls"

// Each registered tool is created on first use and retained for the application
// lifetime. Hiding a tool must not discard its settings or stop acquisition.
Item {
    id: host
    required property var session
    property var tools: []
    property string toolId: ""
    property real uiScale: 1

    Repeater {
        model: host.tools
        delegate: Loader {
            id: toolLoader
            required property var modelData
            property bool opened: false
            property string loadError: ""
            property var errorProbe: null
            property bool detailsVisible: false
            anchors.fill: parent
            visible: modelData.toolId === host.toolId
            focus: visible

            function ensureLoaded() {
                if (!modelData || modelData.toolId !== host.toolId || (opened && status !== Loader.Error)) return
                if (status === Loader.Error) source = ""
                clearLoadError()
                opened = true
                setSource(modelData.entryPoint, {session:host.session, uiScale:host.uiScale})
                // Local QRC loads may finish before a status-change handler runs.
                if (status === Loader.Error) diagnoseLoadError()
            }
            function clearLoadError() {
                if (errorProbe) { errorProbe.destroy(); errorProbe=null }
                loadError=""; detailsVisible=false
            }
            function updateLoadError() {
                if (status !== Loader.Error || !errorProbe || errorProbe.status === Component.Loading) return
                var detail=errorProbe.errorString()
                loadError=detail.length ? detail : qsTr("Qt could not create the tool instance. See Application Output for the creation error.")
                console.warn("Tool load failed: " + modelData.entryPoint + "\n" + loadError)
            }
            function diagnoseLoadError() {
                if (status !== Loader.Error || errorProbe) return
                loadError=qsTr("Collecting the component error…")
                // Inspect the component definition only. Never instantiate a second
                // tool, which could repeat startup behavior or acquire hardware.
                errorProbe=Qt.createComponent(modelData.entryPoint,Component.PreferSynchronous,toolLoader)
                if (errorProbe) {
                    errorProbe.statusChanged.connect(updateLoadError)
                    updateLoadError()
                } else loadError=qsTr("No component was returned. Check the registered tool URL in Application Output.")
            }
            Component.onCompleted: ensureLoaded()
            onVisibleChanged: if (visible) ensureLoaded()
            onStatusChanged: if (status === Loader.Error) diagnoseLoadError()
            onLoaded: {
                item.uiScale = Qt.binding(function() { return host.uiScale })
                if (visible) item.forceActiveFocus()
            }

            Column {
                id: errorPanel
                anchors.centerIn: parent
                visible: toolLoader.status === Loader.Error
                width: Math.min(parent.width-80*host.uiScale,1100*host.uiScale)
                spacing: 18*host.uiScale
                Text {
                    width: parent.width; text: qsTr("Unable to open %1").arg(toolLoader.modelData.title)
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    color: "#e7c6aa"; font.pixelSize: Math.round(24*host.uiScale)
                }
                Text {
                    width: parent.width; text: qsTr("The tool page failed to load. Open the error details to identify the cause.")
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    color: "#c9bad9"; font.pixelSize: Math.round(18*host.uiScale)
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter; spacing: 12*host.uiScale
                    ActionButton { text: qsTr("Retry"); uiScale: host.uiScale; enabled: !host.session.busy
                        onClicked: toolLoader.ensureLoaded() }
                    ActionButton { text: toolLoader.detailsVisible ? qsTr("Hide details") : qsTr("Show details"); uiScale: host.uiScale
                        onClicked: toolLoader.detailsVisible=!toolLoader.detailsVisible }
                }
                Rectangle {
                    width: parent.width; height: Math.min(320*host.uiScale,host.height*0.45)
                    visible: toolLoader.detailsVisible; color: "#211a2f"; radius: 10*host.uiScale; border.color: "#4c3b5d"
                    Flickable {
                        anchors.fill: parent; anchors.margins: 16*host.uiScale
                        contentWidth: width; contentHeight: errorText.height
                        clip: true; boundsBehavior: Flickable.StopAtBounds
                        TextEdit {
                            id: errorText
                            width: parent.width; readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText
                            text: toolLoader.modelData.entryPoint + "\n\n" + toolLoader.loadError
                            wrapMode: TextEdit.WrapAnywhere; color: "#e7d9f5"; font.pixelSize: Math.round(16*host.uiScale)
                        }
                    }
                }
            }
        }
    }
}
