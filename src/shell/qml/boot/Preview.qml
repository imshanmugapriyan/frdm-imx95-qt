import QtQuick
import QtQuick.Window

// Standalone visual review. Play one fill sequence; keys allow reviewing
// individual values without changing the application's startup settings.
Window {
    id: previewWindow

    width: Math.round(Math.min(1920, Screen.desktopAvailableWidth * 0.9,
                              Screen.desktopAvailableHeight * 0.9 * 16 / 9))
    height: Math.round(width * 9 / 16)
    minimumWidth: 480
    minimumHeight: 270
    visible: true
    title: qsTr("Boot screen preview - R: replay, P: 25%, I: waiting, 0: empty, 1: full, F11: fullscreen")

    BootScreen {
        id: splash
        anchors.fill: parent
        progress: 0
        animationsEnabled: !previewAnimation.running
    }

    NumberAnimation {
        id: previewAnimation

        target: splash
        property: "progress"
        from: 0
        to: 1
        duration: 4000
        easing.type: Easing.Linear
        loops: 1
        running: true
    }

    Shortcut {
        sequence: "R"
        onActivated: previewAnimation.restart()
    }

    Shortcut {
        sequence: "P"
        onActivated: {
            previewAnimation.stop()
            splash.progress = 0.25
        }
    }

    Shortcut {
        sequence: "I"
        onActivated: {
            previewAnimation.stop()
            splash.progress = -1
        }
    }

    Shortcut {
        sequence: "0"
        onActivated: {
            previewAnimation.stop()
            splash.progress = 0
        }
    }

    Shortcut {
        sequence: "1"
        onActivated: {
            previewAnimation.stop()
            splash.progress = 1
        }
    }

    Shortcut {
        sequence: "F11"
        onActivated: {
            if (previewWindow.visibility === Window.FullScreen)
                previewWindow.showNormal()
            else
                previewWindow.showFullScreen()
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: previewWindow.visibility === Window.FullScreen
        onActivated: previewWindow.showNormal()
    }
}
