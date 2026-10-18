pragma ComponentBehavior: Bound
import QtQuick
import "../../tools/utilities/notepad/qml" as Notes
import "../../tools/utilities/calculator/qml" as ArithmeticTool
import "../../tools/utilities/results/qml" as Results
import "../../tools/utilities/settings/qml" as Settings

Item {
    id: utilities
    required property var files
    required property var preferences
    property var systemInfo: ({})
    property string currentId: ""
    property bool minimized: false
    property var opened: ({})
    readonly property bool windowVisible: currentId.length>0 && !minimized
    readonly property bool notesDirty: notepad.dirty
    readonly property bool confirming: notepad.confirming
    signal preferencesRequested(var values)
    signal documentsChanged()
    signal exitApproved()
    function isRunning(id) { return opened[id]===true }
    function openTool(id) {
        if (confirming) return
        if (["notepad","calculator","results","settings"].indexOf(id)<0) return
        var next=Object.assign({},opened); next[id]=true; opened=next
        currentId=id; minimized=false
        if (id === "notepad") { notepad.refresh(); notepad.focusEditor() }
        else if (id === "calculator") calculator.focusInput()
        else if (id === "results") { results.refresh(); results.focusList() }
        else frame.forceActiveFocus()
    }
    function finishClose(id) {
        var next=Object.assign({},opened); delete next[id]; opened=next
        if (currentId===id) { currentId=""; minimized=false }
    }
    function closeCurrent() {
        var id=currentId
        if (id === "notepad") notepad.request(function(){ utilities.finishClose(id) })
        else finishClose(id)
    }
    function minimize() { if (!confirming) minimized=true }
    function confirmExit() {
        if (notepad.dirty) { openTool("notepad"); notepad.request(function(){ utilities.exitApproved() }) }
        else exitApproved()
    }
    function handleEscape() {
        if (currentId === "notepad" && notepad.confirming) notepad.cancelPending()
        else closeCurrent()
    }
    function preferenceStatus(message) { settings.message=message }

    DesktopWindow {
        id: frame
        visible: utilities.windowVisible
        title: utilities.currentId === "notepad" ? qsTr("Notepad")+(notepad.dirty ? "  •" : "")
             : utilities.currentId === "calculator" ? qsTr("Calculator")
             : utilities.currentId === "results" ? qsTr("Results") : qsTr("Desktop Settings")
        onCloseRequested: utilities.closeCurrent()
        onMinimizeRequested: utilities.minimize()
    }
    Notes.Notepad {
        id: notepad; parent: frame.contentItem; anchors.fill: parent
        visible: utilities.currentId === "notepad"; active: visible && utilities.windowVisible && utilities.visible
        files: utilities.files; onSaved: utilities.documentsChanged()
    }
    ArithmeticTool.Calculator { id: calculator; parent: frame.contentItem; anchors.fill: parent; visible: utilities.currentId === "calculator" }
    Results.ResultsBrowser { id: results; parent: frame.contentItem; anchors.fill: parent
        visible: utilities.currentId === "results"; files: utilities.files }
    Settings.DesktopSettings {
        id: settings; parent: frame.contentItem; anchors.fill: parent
        visible: utilities.currentId === "settings"; files: utilities.files
        preferences: utilities.preferences; systemInfo: utilities.systemInfo
        onPreferencesRequested: function(values) { utilities.preferencesRequested(values) }
    }
}
