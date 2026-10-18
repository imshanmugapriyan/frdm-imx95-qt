pragma ComponentBehavior: Bound

import QtQuick

// Honest feedback until a diagnostic route or executable has been connected.
FocusScope {
    id: notice

    property string toolTitle: ""
    readonly property real uiScale: Math.max(0.01, Math.min(width / 1920, height / 1080))
    signal dismissed()

    function focusAction() {
        backButton.forceActiveFocus()
    }

    function dismissFromKey(event) {
        if (!event.isAutoRepeat)
            dismissed()
        event.accepted = true
    }

    Keys.onEscapePressed: function(event) {
        dismissed()
        event.accepted = true
    }
    Keys.onTabPressed: function(event) { event.accepted = true }
    Keys.onBacktabPressed: function(event) { event.accepted = true }

    Rectangle { anchors.fill: parent; color: "#b30c0918" }
    MouseArea {
        anchors.fill: parent
        onClicked: notice.dismissed()
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 780 * notice.uiScale
        height: 420 * notice.uiScale
        radius: 24 * notice.uiScale
        color: "#282139"
        border.color: "#645175"
        antialiasing: true
        Accessible.role: Accessible.Dialog
        Accessible.name: notice.toolTitle
        Accessible.description: qsTr("This diagnostic tool is not available yet.")

        MouseArea { anchors.fill: parent }

        Text {
            x: 48 * notice.uiScale
            y: 44 * notice.uiScale
            text: qsTr("COMING SOON")
            color: "#c5a7ff"
            font.pixelSize: Math.round(15 * notice.uiScale)
            font.weight: Font.DemiBold
            font.letterSpacing: 2 * notice.uiScale
        }
        Text {
            id: titleLabel
            x: 48 * notice.uiScale
            y: 86 * notice.uiScale
            width: parent.width - 96 * notice.uiScale
            text: notice.toolTitle
            color: "#fcf8ff"
            font.pixelSize: Math.round(32 * notice.uiScale)
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }
        Text {
            x: titleLabel.x
            anchors.top: titleLabel.bottom
            anchors.topMargin: 22 * notice.uiScale
            width: titleLabel.width
            text: qsTr("This diagnostic tool is not available yet.")
            color: "#c7bdd5"
            font.pixelSize: Math.round(21 * notice.uiScale)
            wrapMode: Text.WordWrap
        }
        Rectangle {
            id: backButton
            objectName: "toolNoticeBackButton"
            x: 48 * notice.uiScale
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 44 * notice.uiScale
            width: 232 * notice.uiScale
            height: 60 * notice.uiScale
            radius: 12 * notice.uiScale
            color: backPointer.pressed ? "#b397e8" : "#d2baff"
            border.width: activeFocus ? 2 : 0
            border.color: "#ffffff"
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Back to home")
            Accessible.focusable: true
            Accessible.onPressAction: notice.dismissed()
            Keys.onReturnPressed: function(event) { notice.dismissFromKey(event) }
            Keys.onEnterPressed: function(event) { notice.dismissFromKey(event) }
            Keys.onSpacePressed: function(event) { notice.dismissFromKey(event) }
            Text {
                anchors.centerIn: parent
                text: qsTr("Back to home")
                color: "#251735"
                font.pixelSize: Math.round(20 * notice.uiScale)
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: backPointer
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: notice.dismissed()
            }
        }
    }
}
