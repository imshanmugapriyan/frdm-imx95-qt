pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import "../../shell/qml/boot" as Boot
import "../../shell/qml" as Shell
import "../../shared/qml" as Shared
import "../../services/diagnostics/qml" as Diagnostics
import "../../tools" as Tools

Window {
    id: window

    readonly property int designWidth: 1920
    readonly property int designHeight: 1080
    property bool startFullscreen: true

    // Temporary visual sequence until real initialization tasks are available.
    // Disable simulateStartup before binding startupProgress to a controller.
    property bool simulateStartup: true
    property int startupDurationMs: 4000
    property real startupProgress: 0
    // Replace both progress and readiness with backend values during integration.
    property bool startupReady: simulateStartup && startupProgress >= 1
    property bool homeRevealed: false
    property string activeToolId: ""

    width: designWidth
    height: designHeight
    minimumWidth: 640
    minimumHeight: 360
    visibility: startFullscreen ? Window.FullScreen : Window.Windowed
    title: qsTr("Suno Diagnostic Software")
    color: "#151126"
    onClosing: function(close) {
        if (diagnosticSession.busy) {
            close.accepted = false
            if (diagnosticSession.phase !== "stopping") diagnosticSession.finish()
        } else if (home.notesDirty) {
            close.accepted = false
            home.confirmApplicationClose()
        }
    }

    NumberAnimation {
        id: startupAnimation

        target: window
        property: "startupProgress"
        from: 0
        to: 1
        duration: window.startupDurationMs
        easing.type: Easing.Linear
        loops: 1
        running: window.simulateStartup
    }

    // Give the completed bar a brief visible hold before entering home.
    // Readiness is separate from progress so real initialization can gate entry.
    Timer {
        interval: 400
        running: !window.homeRevealed && window.startupReady
                 && isFinite(window.startupProgress) && window.startupProgress >= 1
        repeat: false
        onTriggered: window.homeRevealed = true
    }

    Tools.ToolRegistry { id: toolCatalog }

    Shell.HomeScreen {
        id: home
        objectName: "desktopHomeScreen"

        anchors.fill: parent
        visible: window.homeRevealed
        activeToolId: window.activeToolId
        session: diagnosticSession
        toolRegistry: toolCatalog
        enabled: visible && !splash.visible && !toolNotice.visible
        onEnabledChanged: {
            if (enabled)
                focusTools()
        }
        onToolSelected: function(toolId, toolTitle) {
            var tool = toolCatalog.findTool(toolId)
            if (tool && tool.category === "diagnostic" && tool.entryPoint.length) {
                window.activeToolId = toolId
                diagnosticWorkspace.forceActiveFocus()
                return
            }
            toolNotice.toolTitle = toolTitle
            toolNotice.visible = true
            toolNotice.focusAction()
        }
        onDesktopRequested: {
            if (!diagnosticSession.busy) {
                window.activeToolId = ""
                home.focusTools()
            }
        }
        onExitApproved: window.close()
    }

    Diagnostics.DiagnosticSession { id: diagnosticSession }
    Shell.DiagnosticWorkspace {
        id: diagnosticWorkspace
        parent: home.workspaceHost
        anchors.fill: parent
        visible: window.homeRevealed && window.activeToolId.length > 0
        toolId: window.activeToolId
        session: diagnosticSession
        tools: toolCatalog.entries(true, "")
        toolTitle: home.titleFor(window.activeToolId)
        expanded: home.workspaceExpanded
        onRestoreViewRequested: home.workspaceExpanded = false
        onHomeRequested: {
            if (!diagnosticSession.busy) {
                window.activeToolId = ""
                home.showDesktop()
            }
        }
    }

    Boot.BootScreen {
        id: splash
        objectName: "applicationBootScreen"

        anchors.fill: parent
        progress: window.startupProgress
        opacity: window.homeRevealed ? 0 : 1
        visible: opacity > 0
        enabled: !window.homeRevealed
        // The startup animation already interpolates every frame.
        animationsEnabled: !startupAnimation.running

        Behavior on opacity {
            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
        }
    }

    Shared.ToolNotice {
        id: toolNotice
        anchors.fill: parent
        visible: false
        onDismissed: {
            visible = false
            home.focusTools()
        }
    }

    Shortcut {
        sequence: "F11"
        // Windows preview convenience; the fullscreen Linux shell stays fullscreen.
        enabled: Qt.platform.os === "windows" || !window.startFullscreen
        onActivated: {
            if (window.visibility === Window.FullScreen)
                window.showNormal()
            else
                window.showFullScreen()
        }
    }
}
