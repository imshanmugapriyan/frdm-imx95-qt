pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls" as Controls

Item {
    id: browser
    required property var files
    property var entries: []
    property string selectedName: ""
    property string preview: ""
    property string message: ""
    function refresh() { entries=files.listResults() }
    function focusList() { resultsList.forceActiveFocus() }
    function open(name) {
        var result=files.readResult(name)
        if (!result.name) { message=files.error; return }
        selectedName=name; preview=result.text
        message=result.truncated ? qsTr("Preview shows the first 128 KiB. The original file is unchanged.") : qsTr("Read-only preview")
    }
    Text { width: parent.width-155; text: files.outputDirectory; textFormat: Text.PlainText; elide: Text.ElideMiddle
        color: "#abb8d0"; font.pixelSize: 18; y: 12 }
    Controls.ActionButton { anchors.right: parent.right; text: qsTr("Refresh"); onClicked: browser.refresh() }
    Rectangle {
        y: 66; width: 384; height: parent.height-y; radius: 12; color: "#111a2b"
        ListView {
            id: resultsList; anchors.fill: parent; anchors.margins: 10; model: browser.entries; spacing: 4; clip: true
            activeFocusOnTab: true; keyNavigationEnabled: true
            delegate: Rectangle {
                id: resultRow; required property var modelData; required property int index
                width: resultsList.width; height: 76; radius: 8
                focus: ListView.isCurrentItem
                color: browser.selectedName===modelData.name ? "#303d59" : resultMouse.containsMouse ? "#242d44" : "transparent"
                activeFocusOnTab: true; border.color: activeFocus ? "#8abaff" : "transparent"
                Accessible.role: Accessible.Button; Accessible.name: modelData.name
                Accessible.onPressAction: browser.open(modelData.name)
                Keys.onReturnPressed: browser.open(modelData.name)
                Keys.onSpacePressed: browser.open(modelData.name)
                Text { x: 10; y: 12; width: parent.width-20; text: resultRow.modelData.name; textFormat: Text.PlainText
                    elide: Text.ElideMiddle; color: "#e9eefa"; font.pixelSize: 17 }
                Text { x: 10; y: 44; text: (resultRow.modelData.bytes/1024).toFixed(1)+" KiB  ·  "+Qt.formatDateTime(new Date(resultRow.modelData.modified),"d MMM, HH:mm")
                    color: "#9ba8c2"; font.pixelSize: 14 }
                MouseArea { id: resultMouse; anchors.fill: parent; hoverEnabled: true
                    onClicked: { resultsList.currentIndex=resultRow.index; resultsList.forceActiveFocus(); browser.open(resultRow.modelData.name) } }
            }
            Text { visible: browser.entries.length===0; x: 12; y: 22; width: parent.width-24
                text: qsTr("No results yet.\n\nExport a diagnostic result or a Notepad text file to see it here.")
                wrapMode: Text.WordWrap; color: "#a7b5cc"; font.pixelSize: 19 }
        }
    }
    Item {
        x: 408; y: 66; width: parent.width-x; height: parent.height-y
        Text { width: parent.width; text: browser.selectedName.length ? browser.selectedName : qsTr("Select a result to preview")
            textFormat: Text.PlainText; elide: Text.ElideMiddle; color: "#f1ecfb"; font.pixelSize: 20 }
        Rectangle {
            y: 44; width: parent.width; height: parent.height-84; radius: 12; color: "#111a2b"; border.color: "#414d67"
            Flickable {
                id: previewScroll; anchors.fill: parent; anchors.margins: 16; contentWidth: width
                contentHeight: previewText.height; clip: true; boundsBehavior: Flickable.StopAtBounds
                TextEdit { id: previewText; width: previewScroll.width; text: browser.preview
                    textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; selectByKeyboard: true
                    wrapMode: TextEdit.WrapAnywhere; font.family: "monospace"; font.pixelSize: 16; color: "#d1daeb" }
            }
        }
        Text { y: parent.height-26; width: parent.width; text: browser.message; textFormat: Text.PlainText
            font.pixelSize: 14; color: "#baaacb"; elide: Text.ElideRight }
    }
}
