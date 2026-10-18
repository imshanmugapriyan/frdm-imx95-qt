pragma ComponentBehavior: Bound
import QtQuick
import FRDM_IMX95_QT_DSW
import "../../shared/qml/controls" as Controls

FocusScope {
    id: desktop
    property var session
    required property var toolRegistry
    property string activeToolId: ""
    property bool workspaceExpanded: false
    property var preferences: ({wallpaper:"dusk",clock24:true})
    property var storage: ({available:false,platform:""})
    property date currentTime: new Date()
    property string feedback: ""
    property alias searchText: search.text
    property alias workspaceHost: workspaceLayer
    readonly property bool notesDirty: utilities.notesDirty
    readonly property real uiScale: Math.max(0.01,Math.min(width/1920,height/1080))
    readonly property bool captureBusy: session ? session.busy : false
    readonly property string acquisitionStatus: {
        if (!session) return qsTr("Acquisition disconnected")
        session.revision
        if (session.busy) return qsTr("Session: %1").arg(session.phase)
        return session.model.availableModes.length ? qsTr("Acquisition available") : qsTr("Acquisition disconnected")
    }
    signal toolSelected(string toolId,string toolTitle)
    signal desktopRequested()
    signal exitApproved()
    clip: true

    function entries(diagnosticsOnly) {
        return toolRegistry.entries(diagnosticsOnly, searchText)
    }
    function titleFor(id) {
        var tool=toolRegistry.findTool(id)
        return tool ? tool.title : qsTr("Desktop")
    }
    function launch(tool) {
        if (utilities.confirming) return
        if (tool.category === "utility") { utilities.openTool(tool.toolId); return }
        if (captureBusy && activeToolId!==tool.toolId) return
        utilities.minimize(); toolSelected(tool.toolId,tool.title)
    }
    function showDesktop() {
        if (captureBusy || utilities.confirming) return
        utilities.minimize(); workspaceExpanded=false; desktopRequested(); focusTools()
    }
    function focusTools() { if (!activeToolId.length && !utilities.windowVisible) toolsGrid.forceActiveFocus() }
    function restoreFocus() {
        if (utilities.windowVisible) return
        if (activeToolId.length && workspaceLayer.children.length) workspaceLayer.children[0].forceActiveFocus()
        else focusTools()
    }
    function confirmApplicationClose() { utilities.confirmExit() }
    function refreshStorage() { storage=desktopFiles.storageInfo() }
    function setPreferences(values) {
        if (desktopFiles.savePreferences(values)) {
            preferences=values; utilities.preferenceStatus(qsTr("Desktop preferences saved."))
        } else utilities.preferenceStatus(desktopFiles.error)
    }
    function searchTools() {
        if (captureBusy || utilities.confirming) return
        showDesktop(); search.forceActiveFocus(); search.selectAll()
    }
    onSearchTextChanged: if (toolsGrid) toolsGrid.currentIndex=0
    onActiveToolIdChanged: { if (!activeToolId.length) workspaceExpanded=false; if (desktopFiles) refreshStorage() }
    Component.onCompleted: {
        preferences=desktopFiles.loadPreferences(); feedback=desktopFiles.error; refreshStorage()
    }
    Keys.onEscapePressed: function(event) {
        if (utilities.windowVisible) utilities.handleEscape()
        else if (activeToolId.length && !captureBusy) showDesktop()
        else if (searchText.length) searchText=""
        event.accepted=true
    }

    DesktopFiles { id: desktopFiles }
    Timer { interval: 1000; running: desktop.visible; repeat: true; onTriggered: desktop.currentTime=new Date() }
    Timer { interval: 30000; running: desktop.visible; repeat: true; onTriggered: desktop.refreshStorage() }
    DesktopWallpaper { anchors.fill: parent; wallpaperStyle: desktop.preferences.wallpaper }
    Rectangle { anchors.fill: parent; color: "#2010172d" }

    Item {
        id: stage
        anchors.left: parent.left; anchors.top: parent.top
        width: desktop.width/desktop.uiScale; height: desktop.height/desktop.uiScale
        scale: desktop.uiScale; transformOrigin: Item.TopLeft
        // Fill the viewport while keeping text and controls proportionally scaled.
        Item {
            id: launcherArea
            visible: !desktop.activeToolId.length
            anchors.left: parent.left; anchors.right: parent.right
            anchors.leftMargin: 64; anchors.rightMargin: 64
            anchors.top: topBar.bottom; anchors.topMargin: 50
            anchors.bottom: dock.top; anchors.bottomMargin: 48
            Rectangle {
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                anchors.right: statusWidgets.left; anchors.rightMargin: 28
                radius: 28
                color: "#db1d253e"; border.color: "#706b88"
                Text { x: 36; y: 28; text: qsTr("Workspace"); color: "#f8f3ff"; font.pixelSize: 29; font.weight: Font.DemiBold }
                Text { anchors.right: parent.right; anchors.rightMargin: 38; y: 35
                    text: qsTr("%1 diagnostic tools").arg(toolsGrid.count); color: "#acb5d1"; font.pixelSize: 18 }
                Rectangle {
                    x: 36; y: 88; width: parent.width-72; height: 58; radius: 12
                    color: "#30405b"; border.color: search.activeFocus ? "#cbb2fa" : "#53607b"
                    Item {
                        x: 18; y: 17; width: 24; height: 24
                        Rectangle { width: 15; height: 15; radius: 8; color: "transparent"; border.color: "#b1bdd6"; border.width: 2 }
                        Rectangle { x: 15; y: 13; width: 10; height: 2; rotation: 45; color: "#b1bdd6" }
                    }
                    TextInput {
                        id: search; x: 54; y: 14; width: parent.width-78; height: 32
                        color: "#f4f5ff"; font.pixelSize: 20; selectByMouse: true; clip: true
                        Accessible.name: qsTr("Search diagnostic tools")
                        onAccepted: if (toolsGrid.count>0) desktop.launch(toolsGrid.model[Math.max(0,toolsGrid.currentIndex)])
                    }
                    Text { x: 54; y: 17; width: parent.width-78; visible: !search.length; text: qsTr("Search diagnostic tools…")
                        color: "#b0b9d1"; font.pixelSize: 19; elide: Text.ElideRight }
                }
                Text { x: 36; y: 170; text: qsTr("Select a diagnostic tool to begin."); color: "#b8c1d9"; font.pixelSize: 18 }
                GridView {
                    id: toolsGrid; objectName: "diagnosticToolsGrid"
                    x: 36; y: 210; width: parent.width-72; height: parent.height-y-70
                    cellWidth: width/Math.max(1,Math.floor(width/280)); cellHeight: 176
                    model: desktop.entries(true); clip: true; focus: true
                    keyNavigationEnabled: true; keyNavigationWraps: true; activeFocusOnTab: true
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: LauncherTile {
                        required property var modelData; required property int index
                        width: toolsGrid.cellWidth-12; height: toolsGrid.cellHeight-6
                        title: modelData.title; iconKind: modelData.iconKind; accent: modelData.accent
                        focus: GridView.isCurrentItem
                        onActivated: { toolsGrid.currentIndex=index; desktop.launch(modelData) }
                    }
                    Text { visible: toolsGrid.count===0; anchors.centerIn: parent; text: qsTr("No matching diagnostic tools"); color: "#c6c9dd"; font.pixelSize: 22 }
                }
                Rectangle { x: 36; y: parent.height-48; width: parent.width-72; height: 1; color: "#50516c" }
                Text { x: 36; y: parent.height-32; text: qsTr("Ctrl+Space to search diagnostic tools")
                    color: "#abb4cd"; font.pixelSize: 14 }
            }
            Column {
                id: statusWidgets
                anchors.top: parent.top; anchors.right: parent.right
                width: 324; spacing: 20
                Rectangle {
                    width: parent.width; height: 214; radius: 24; color: "#d51b2741"; border.color: "#656c8a"
                    Text { x: 24; y: 25; text: Qt.formatDate(desktop.currentTime,"dddd, d MMM yyyy"); color: "#b8c1d9"; font.pixelSize: 16 }
                    Text { x: 20; y: 62; text: Qt.formatTime(desktop.currentTime,desktop.preferences.clock24 ? "HH:mm" : "h:mm AP")
                        color: "#f7f2ff"; font.pixelSize: desktop.preferences.clock24 ? 72 : 53; font.weight: Font.Light }
                    Text { x: 24; y: 160; text: qsTr("Local time")
                        color: "#c5c7dd"; font.pixelSize: 19 }
                }
                Rectangle {
                    width: parent.width; height: 196; radius: 24; color: "#d51b2741"; border.color: "#656c8a"
                    Text { x: 24; y: 25; text: qsTr("DIAGNOSTICS"); color: "#aebad4"; font.pixelSize: 14; font.letterSpacing: 1.5 }
                    Rectangle { x: 24; y: 75; width: 9; height: 9; radius: 5; color: desktop.captureBusy ? "#89e0c0" : "#dab987" }
                    Text { x: 44; y: 68; width: parent.width-64; text: desktop.acquisitionStatus; color: "#eee8fa"; font.pixelSize: 19; wrapMode: Text.WordWrap }
                    Text { x: 24; y: 132; width: parent.width-48; text: qsTr("Prepare settings and review\nsaved results at any time.")
                        color: "#acb8d3"; font.pixelSize: 16; lineHeight: 1.3 }
                }
                Rectangle {
                    width: parent.width; height: 210; radius: 24; color: "#d51b2741"; border.color: "#656c8a"
                    Text { x: 24; y: 24; text: qsTr("RESULTS STORAGE"); color: "#aebad4"; font.pixelSize: 14; font.letterSpacing: 1.5 }
                    Text { x: 24; y: 60; text: desktop.storage.available ? qsTr("%1 GB free").arg((desktop.storage.freeBytes/1073741824).toFixed(1)) : qsTr("Unavailable")
                        color: "#eee8fa"; font.pixelSize: 29; font.weight: Font.Medium }
                    Rectangle {
                        x: 24; y: 110; width: 276; height: 7; radius: 4; color: "#3f4964"
                        Rectangle { width: parent.width*(desktop.storage.available ? Math.max(0,Math.min(1,desktop.storage.freeBytes/desktop.storage.totalBytes)) : 0)
                            height: parent.height; radius: 4; color: "#8fbfd9" }
                    }
                    Controls.ActionButton { x: 24; y: 142; width: 276; height: 44; text: qsTr("Browse results"); onClicked: utilities.openTool("results") }
                }
            }
        }

        Item {
            id: workspaceLayer; x: 0; y: desktop.workspaceExpanded ? 0 : 58
            width: parent.width; height: desktop.workspaceExpanded ? parent.height : dock.y-y-10
            visible: desktop.activeToolId.length>0; z: desktop.workspaceExpanded ? 50 : 2
        }
        UtilityHost {
            id: utilities; anchors.fill: parent; z: 60
            files: desktopFiles; preferences: desktop.preferences; systemInfo: desktop.storage
            onPreferencesRequested: function(values) { desktop.setPreferences(values) }
            onDocumentsChanged: desktop.refreshStorage()
            onExitApproved: desktop.exitApproved()
            onWindowVisibleChanged: if (!windowVisible) desktop.restoreFocus()
        }
        Rectangle {
            id: topBar; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 54; color: "#ec151f35"; z: 40; visible: !desktop.workspaceExpanded
            Text { x: 28; anchors.verticalCenter: parent.verticalCenter; text: "Suno"; color: "#fbf8ff"; font.pixelSize: 24; font.weight: Font.DemiBold }
            Rectangle { x: 113; y: 15; width: 1; height: 24; color: "#52607a" }
            Controls.ActionButton { x: 130; y: 8; height: 38; width: 94; text: qsTr("Tools"); enabled: !desktop.captureBusy && !utilities.confirming; onClicked: desktop.showDesktop() }
            Text { x: 250; anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0,topBarActions.x-x-24); elide: Text.ElideRight
                text: desktop.titleFor(utilities.windowVisible ? utilities.currentId : desktop.activeToolId)
                color: "#bdc8df"; font.pixelSize: 17 }
            Row {
                id: topBarActions; anchors.right: parent.right; anchors.rightMargin: 28
                anchors.verticalCenter: parent.verticalCenter; spacing: 24
                Controls.ActionButton { height: 38; width: 182; text: qsTr("Full workspace"); visible: desktop.activeToolId.length>0
                    onClicked: desktop.workspaceExpanded=true }
                Text { anchors.verticalCenter: parent.verticalCenter; text: desktop.captureBusy ? qsTr("Acquisition active") : qsTr("Diagnostic desktop")
                    color: desktop.captureBusy ? "#95dec6" : "#a9b9d5"; font.pixelSize: 16 }
                Text { anchors.verticalCenter: parent.verticalCenter
                    text: Qt.formatDateTime(desktop.currentTime,desktop.preferences.clock24 ? "ddd d MMM  HH:mm" : "ddd d MMM  h:mm AP")
                    color: "#e5e6f4"; font.pixelSize: 16 }
            }
        }
        Rectangle {
            id: dock; anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 28
            width: dockRow.implicitWidth+36; height: 108; radius: 26; visible: !desktop.workspaceExpanded
            color: "#db29354e"; border.color: "#747b97"; z: 40
            Row {
                id: dockRow; x: 18; y: 11; spacing: 10
                LauncherTile { title: qsTr("Desktop"); iconKind: "desktop"; accent: "#c7b6f5"; compact: true
                    selected: !desktop.activeToolId.length && !utilities.windowVisible
                    enabled: !desktop.captureBusy && !utilities.confirming; onActivated: desktop.showDesktop() }
                Rectangle { width: 1; height: 56; y: 14; color: "#75809b" }
                Repeater {
                    model: desktop.entries(false)
                    delegate: LauncherTile {
                        required property var modelData; compact: true
                        title: modelData.title; iconKind: modelData.iconKind; accent: modelData.accent
                        running: modelData.category === "utility" ? utilities.isRunning(modelData.toolId) : desktop.activeToolId===modelData.toolId
                        selected: utilities.windowVisible ? utilities.currentId===modelData.toolId : desktop.activeToolId===modelData.toolId
                        enabled: !utilities.confirming && (modelData.category === "utility" || !desktop.captureBusy || desktop.activeToolId===modelData.toolId)
                        onActivated: desktop.launch(modelData)
                    }
                }
            }
        }
        Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.leftMargin: 64; anchors.rightMargin: 64; anchors.bottomMargin: 8
            visible: !desktop.activeToolId.length && desktop.feedback.length>0
            text: desktop.feedback; color: "#e7c6aa"; font.pixelSize: 14; elide: Text.ElideRight }
    }
    Shortcut { sequence: "Ctrl+Space"; enabled: desktop.visible && !desktop.captureBusy && !utilities.confirming; onActivated: desktop.searchTools() }
    Shortcut { sequence: "F10"; enabled: desktop.visible && desktop.activeToolId.length>0; onActivated: desktop.workspaceExpanded=!desktop.workspaceExpanded }
}
