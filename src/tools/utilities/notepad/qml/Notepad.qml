pragma ComponentBehavior: Bound
import QtQuick
import "../../../../shared/qml/controls" as Controls

FocusScope {
    id: note
    required property var files
    property bool active: false
    property var notes: []
    property string noteId: ""
    property string version: ""
    property string savedTitle: "Untitled"
    property string savedText: ""
    property string message: ""
    property var pendingAction: null
    readonly property bool dirty: titleInput.text !== savedTitle || editor.text !== savedText
    readonly property bool confirming: confirmation.visible
    signal saved()

    function refresh() { notes=files.listNotes() }
    function focusEditor() { editor.forceActiveFocus() }
    function request(action) {
        if (dirty) { message=""; pendingAction=action; confirmation.visible=true; cancelButton.forceActiveFocus() }
        else action()
    }
    function discard() { titleInput.text=savedTitle; editor.text=savedText }
    function cancelPending() { confirmation.visible=false; pendingAction=null; editor.forceActiveFocus() }
    function finishPending() {
        var action=pendingAction; pendingAction=null; confirmation.visible=false
        if (action) action()
    }
    function blank() {
        noteId=""; version=""; savedTitle="Untitled"; savedText=""
        titleInput.text=savedTitle; editor.text=""; message=""; titleInput.forceActiveFocus(); titleInput.selectAll()
    }
    function open(id) {
        var document=files.loadNote(id)
        if (!document.id) { message=files.error; return }
        noteId=document.id; version=document.version; savedTitle=document.title; savedText=document.text
        titleInput.text=document.title; editor.text=document.text; message=""; editor.forceActiveFocus()
    }
    function save() {
        var document=files.saveNote(noteId,titleInput.text,editor.text,version)
        if (!document.id) { message=files.error; return false }
        noteId=document.id; version=document.version; savedTitle=document.title; savedText=document.text
        titleInput.text=document.title; message=qsTr("Note saved."); refresh(); saved(); return true
    }
    function exportText() {
        var path=files.exportNote(titleInput.text,editor.text)
        message=path.length ? qsTr("Text file saved: %1").arg(path) : files.error
        if (path.length) saved()
    }

    Rectangle {
        enabled: !confirmation.visible
        width: 270; height: parent.height; radius: 12; color: "#131b2d"
        Text { x: 18; y: 18; text: qsTr("SAVED NOTES"); color: "#a5adc6"; font.pixelSize: 14; font.letterSpacing: 1.5 }
        Controls.ActionButton { x: 16; y: 54; width: 238; text: qsTr("New note"); onClicked: note.request(function(){ note.blank() }) }
        ListView {
            id: noteList; x: 10; y: 120; width: 250; height: parent.height-160
            model: note.notes; clip: true; spacing: 6; boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: savedNote; required property var modelData
                width: noteList.width; height: 66; radius: 9
                color: note.noteId===modelData.id ? "#3b3554" : noteMouse.containsMouse ? "#293048" : "transparent"
                activeFocusOnTab: true; border.color: activeFocus ? "#cbb2fa" : "transparent"
                Accessible.role: Accessible.Button; Accessible.name: modelData.title
                function activate() { var id=modelData.id; note.request(function(){ note.open(id) }) }
                Accessible.onPressAction: activate()
                Keys.onReturnPressed: activate()
                Keys.onSpacePressed: activate()
                Text { x: 12; y: 10; width: parent.width-24; text: savedNote.modelData.title
                    textFormat: Text.PlainText; color: "#eee9f8"; font.pixelSize: 18; elide: Text.ElideRight }
                Text { x: 12; y: 37; text: Qt.formatDateTime(new Date(savedNote.modelData.modified),"d MMM, HH:mm")
                    color: "#a8abc2"; font.pixelSize: 13 }
                MouseArea { id: noteMouse; anchors.fill: parent; hoverEnabled: true; onClicked: savedNote.activate() }
            }
            Text { visible: note.notes.length===0; width: parent.width-20; x: 10; y: 8
                text: qsTr("Your saved notes will appear here."); color: "#a8abc2"; wrapMode: Text.WordWrap; font.pixelSize: 17 }
        }
        Text { x: 18; y: parent.height-28; text: qsTr("100 most recent notes"); color: "#8e97b2"; font.pixelSize: 13 }
    }
    Item {
        enabled: !confirmation.visible
        x: 292; width: parent.width-x; height: parent.height
        Rectangle {
            width: parent.width-306; height: 48; radius: 8; color: "#111a2c"; border.color: titleInput.activeFocus ? "#bba2ee" : "#434966"
            TextInput { id: titleInput; anchors.fill: parent; anchors.margins: 12; text: "Untitled"; maximumLength: 120
                color: "#f7f1ff"; font.pixelSize: 20; selectByMouse: true; clip: true; Accessible.name: qsTr("Note title") }
        }
        Row {
            anchors.right: parent.right; spacing: 8
            Controls.ActionButton { text: qsTr("Save"); width: 106; primary: true; enabled: note.dirty; onClicked: note.save() }
            Controls.ActionButton { text: qsTr("Export TXT"); width: 176; onClicked: note.exportText() }
        }
        Rectangle {
            y: 66; width: parent.width; height: parent.height-130; radius: 12; color: "#101a2b"; border.color: "#404962"
            Flickable {
                id: paper; anchors.fill: parent; anchors.margins: 8; clip: true
                contentWidth: width; contentHeight: Math.max(height,editor.height+28)
                boundsBehavior: Flickable.StopAtBounds
                function revealCursor(rect) {
                    if (contentY>rect.y+14) contentY=rect.y+14
                    else if (contentY+height<rect.y+rect.height+14) contentY=rect.y+rect.height+14-height
                }
                TextEdit {
                    id: editor; x: 14; y: 14; width: paper.width-28; height: Math.max(paper.height-28,contentHeight)
                    textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; color: "#f2effa"; font.pixelSize: 21
                    selectByMouse: true; selectByKeyboard: true; selectionColor: "#74539d"
                    Accessible.name: qsTr("Note text")
                    onCursorRectangleChanged: paper.revealCursor(cursorRectangle)
                }
                Text { x: 14; y: 14; visible: editor.length===0; text: qsTr("Write your session notes here…")
                    color: "#727f9e"; font.pixelSize: 21 }
            }
        }
        Text { y: parent.height-50; text: (note.dirty ? qsTr("Unsaved changes") : note.noteId.length ? qsTr("Saved") : qsTr("New note"))+"  ·  "+qsTr("%1 characters").arg(editor.length)
            color: note.dirty ? "#efca88" : "#abb8d0"; font.pixelSize: 15 }
        Text { y: parent.height-24; width: parent.width; text: note.message; textFormat: Text.PlainText
            color: "#c7badc"; font.pixelSize: 14; elide: Text.ElideMiddle }
    }
    Rectangle {
        id: confirmation; anchors.fill: parent; visible: false; color: "#dd101626"; z: 10
        MouseArea { anchors.fill: parent }
        Rectangle {
            anchors.centerIn: parent; width: 620; height: 252; radius: 18; color: "#2c2c45"; border.color: "#8c79aa"
            Text { x: 28; y: 28; text: qsTr("Save your note?"); color: "#fff8ff"; font.pixelSize: 29; font.weight: Font.DemiBold }
            Text { x: 28; y: 82; width: parent.width-56; text: qsTr("This note has unsaved changes.")
                color: "#c9c1d9"; font.pixelSize: 20; wrapMode: Text.WordWrap }
            Text { x: 28; y: 118; width: parent.width-56; text: note.message; textFormat: Text.PlainText
                color: "#efca88"; font.pixelSize: 15; elide: Text.ElideRight }
            Row {
                x: 28; y: 172; spacing: 14
                Controls.ActionButton { text: qsTr("Save"); primary: true; onClicked: if (note.save()) note.finishPending() }
                Controls.ActionButton { text: qsTr("Discard"); onClicked: { note.discard(); note.finishPending() } }
                Controls.ActionButton { id: cancelButton; text: qsTr("Cancel"); onClicked: note.cancelPending() }
            }
        }
    }
    Shortcut { sequence: "Ctrl+S"; enabled: note.active; onActivated: note.save() }
    Shortcut { sequence: "Ctrl+N"; enabled: note.active && !confirmation.visible; onActivated: note.request(function(){ note.blank() }) }
}
