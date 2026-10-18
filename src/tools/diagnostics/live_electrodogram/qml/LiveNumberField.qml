pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: field
    property string label: ""
    property string suffix: ""
    property int value: 0
    property int minimum: 0
    property int maximum: 8000
    property int step: 1
    property real uiScale: 1
    readonly property bool valid: input.acceptableInput && /^[0-9]+$/.test(input.text)
                                 && Number.isInteger(Number(input.text))
    signal edited(int value)
    implicitHeight: 66*uiScale
    implicitWidth: 160*uiScale
    opacity: enabled ? 1 : 0.45
    Text {
        anchors.top: parent.top; width: parent.width
        text: field.label; color: "#beb5cd"; elide: Text.ElideRight
        font.pixelSize: Math.round(15*field.uiScale)
    }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 40*field.uiScale; radius: 7*field.uiScale; color: "#151422"
        border.color: !field.valid ? "#ff8d99" : input.activeFocus ? "#cbb2fa" : "#47425b"
        TextInput {
            id: input
            anchors.left: parent.left; anchors.right: units.left
            anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.margins: 9*field.uiScale
            text: String(field.value); color: "#f5f1ff"
            font.pixelSize: Math.round(18*field.uiScale); verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true; clip: true; activeFocusOnTab: true
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: field.minimum; top: field.maximum; locale: "C" }
            Accessible.name: field.label
            onTextEdited: { if (field.valid) field.edited(Number(text)) }
            onEditingFinished: { text = Qt.binding(function() { return String(field.value) }) }
            onActiveFocusChanged: { if (!activeFocus) text = Qt.binding(function() { return String(field.value) }) }
            function increment(direction) {
                field.edited(Math.max(field.minimum,Math.min(field.maximum,field.value+direction*field.step)))
                text = Qt.binding(function() { return String(field.value) })
            }
            Keys.onUpPressed: increment(1)
            Keys.onDownPressed: increment(-1)
        }
        Text {
            id: units
            anchors.right: parent.right; anchors.rightMargin: 9*field.uiScale; anchors.verticalCenter: parent.verticalCenter
            text: field.suffix; color: "#a9a1bc"; font.pixelSize: Math.round(14*field.uiScale)
        }
    }
}
