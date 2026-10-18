import QtQuick

Rectangle {
    id: bootScreen

    implicitWidth: 1920
    implicitHeight: 1080
    width: implicitWidth
    height: implicitHeight
    color: "#291650"
    clip: true

    // Bind to actual startup progress: 0.0 = starting, 1.0 = ready.
    // A negative or non-finite value means the amount of work is unknown.
    property real progress: 0.0
    property bool animationsEnabled: true

    readonly property bool indeterminate: !isFinite(progress) || progress < 0
    readonly property real normalizedProgress: isFinite(progress)
        ? Math.max(0, Math.min(1, progress)) : 0
    readonly property real contentScale: Math.max(0, Math.min(width / 1920,
                                                            height / 1080))

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("assets/BootScreen.png")
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        smooth: true
        mipmap: true
    }

    // Use the same aspect-preserving geometry as the image so the bar stays
    // below its baked-in "System booting" label, even when the window resizes.
    Item {
        anchors.centerIn: parent
        width: 1920 * bootScreen.contentScale
        height: 1080 * bootScreen.contentScale

        Rectangle {
            id: progressTrack
            objectName: "bootProgressTrack"

            x: 688 * bootScreen.contentScale
            y: 628 * bootScreen.contentScale
            width: 544 * bootScreen.contentScale
            height: 8 * bootScreen.contentScale
            radius: height / 2
            color: "#47ffffff"
            antialiasing: true

            Accessible.role: Accessible.ProgressBar
            Accessible.name: qsTr("System booting")
            Accessible.description: bootScreen.indeterminate
                ? qsTr("Startup in progress")
                : qsTr("%1 percent complete").arg(
                      Math.round(bootScreen.normalizedProgress * 100))

            Rectangle {
                id: progressFill
                objectName: "bootProgressFill"

                // Animate the fraction, not the geometry, so resizing does
                // not make the fill temporarily exceed its track.
                property real displayedProgress: bootScreen.normalizedProgress

                width: progressTrack.width * displayedProgress
                height: progressTrack.height
                radius: height / 2
                color: "#351c75"
                antialiasing: true
                visible: !bootScreen.indeterminate && displayedProgress > 0

                Behavior on displayedProgress {
                    enabled: bootScreen.animationsEnabled
                             && bootScreen.visible && !bootScreen.indeterminate
                    NumberAnimation {
                        duration: 180
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Rectangle {
                id: waitingIndicator
                objectName: "bootWaitingIndicator"

                property real phase: 0

                x: phase * (progressTrack.width - width)
                width: progressTrack.width * 0.25
                height: progressTrack.height
                radius: height / 2
                color: "#351c75"
                antialiasing: true
                visible: bootScreen.indeterminate

                SequentialAnimation on phase {
                    running: bootScreen.visible && bootScreen.indeterminate
                             && bootScreen.animationsEnabled
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: 0
                        to: 1
                        duration: 1000
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        from: 1
                        to: 0
                        duration: 1000
                        easing.type: Easing.InOutSine
                    }
                }
            }
        }
    }
}
