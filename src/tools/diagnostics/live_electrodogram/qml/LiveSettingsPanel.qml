pragma ComponentBehavior: Bound
import QtQuick
import "LiveModel.js" as Live
import "../../../../shared/qml/controls"

Rectangle {
    id: panel
    property real uiScale: 1
    property bool acquisitionBusy: false
    property var draft: Live.defaults()
    property int tab: 0
    readonly property string validationError: Live.error(draft)
    readonly property bool fieldsValid: {
        if (!mcl.valid || !thl.valid || !ifg.valid || !ipg.valid) return false
        for (var i=0; i<resistanceFields.count; ++i) {
            var cell=resistanceFields.itemAt(i)
            if (cell && !cell.valid) return false
        }
        return true
    }
    signal applied(var settings)
    signal cancelled()
    color: "#211e32"; border.color: "#4b425f"; radius: 12*uiScale
    function begin(settings) { draft=Live.copy(settings); scroll.contentY=0 }
    function change(key, value) {
        var next=Live.copy(draft); next[key]=value; draft=next
    }
    function changeElectrode(index, value) {
        var values=draft.electrodes.slice(); values[index]=value; change("electrodes",values)
    }
    function changeResistance(index, value) {
        var values=draft.resistances.slice(); values[index]=value; change("resistances",values)
    }
    Keys.onEscapePressed: function(event) { cancelled(); event.accepted=true }

    Column {
        id: heading
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.margins: 20*panel.uiScale; spacing: 12*panel.uiScale
        Text { text: qsTr("Waveform settings"); color: "#f3edff"; font.pixelSize: Math.round(23*panel.uiScale); font.weight: Font.DemiBold }
        Row {
            spacing: 8*panel.uiScale
            ActionButton { text: qsTr("Signal"); checked: panel.tab===0; width: 210*panel.uiScale; height: 40*panel.uiScale; uiScale: panel.uiScale
                onClicked: { panel.tab=0; scroll.contentY=0 } }
            ActionButton { text: qsTr("Electrodes"); checked: panel.tab===1; width: 210*panel.uiScale; height: 40*panel.uiScale; uiScale: panel.uiScale
                onClicked: { panel.tab=1; scroll.contentY=0 } }
        }
    }
    Flickable {
        id: scroll
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: heading.bottom; anchors.bottom: footer.top
        anchors.leftMargin: 20*panel.uiScale; anchors.rightMargin: 20*panel.uiScale
        anchors.topMargin: 18*panel.uiScale; anchors.bottomMargin: 12*panel.uiScale
        contentWidth: width; contentHeight: panel.tab===0 ? signalSettings.height : electrodeSettings.height
        clip: true; boundsBehavior: Flickable.StopAtBounds
        Column {
            id: signalSettings
            width: scroll.width; visible: panel.tab===0; spacing: 16*panel.uiScale
            Text {
                width: parent.width; wrapMode: Text.WordWrap; color: "#bcb2cd"; font.pixelSize: Math.round(15*panel.uiScale)
                text: panel.acquisitionBusy ? qsTr("Stop capture to edit signal settings. Electrode visibility can still be changed.")
                    : qsTr("Requested at the next capture. Hardware support must be confirmed before acquisition starts.")
            }
            Column {
                width: parent.width; spacing: 16*panel.uiScale; enabled: !panel.acquisitionBusy
                Row {
                    spacing: 12*panel.uiScale
                    LiveNumberField { id: mcl; label: qsTr("MCL"); value: panel.draft.mcl; minimum: 1; maximum: 4095
                        width: (scroll.width-12*panel.uiScale)/2; uiScale: panel.uiScale; onEdited: function(value) { panel.change("mcl",value) } }
                    LiveNumberField { id: thl; label: qsTr("THL"); value: panel.draft.thl; minimum: 1; maximum: 4095
                        width: mcl.width; uiScale: panel.uiScale; onEdited: function(value) { panel.change("thl",value) } }
                }
                LiveChoiceField { label: qsTr("Pulse Width"); width: parent.width; options: Live.pulseWidths; value: panel.draft.pulseWidth
                    uiScale: panel.uiScale; onSelected: function(value) { panel.change("pulseWidth",value) } }
                LiveChoiceField { label: qsTr("Gain"); width: parent.width; options: Live.gains; value: panel.draft.gain
                    uiScale: panel.uiScale; onSelected: function(value) { panel.change("gain",value) } }
                LiveChoiceField { label: qsTr("PPS"); width: parent.width; options: Live.rates; value: panel.draft.pps
                    uiScale: panel.uiScale; onSelected: function(value) { panel.change("pps",value) } }
                LiveChoiceField { label: qsTr("Tolerance"); width: parent.width; columns: 5; options: Live.tolerances; value: panel.draft.tolerance
                    uiScale: panel.uiScale; onSelected: function(value) { panel.change("tolerance",value) } }
                Row {
                    spacing: 12*panel.uiScale
                    LiveNumberField { id: ifg; label: qsTr("IFG"); value: panel.draft.ifg; minimum: 0; maximum: 99
                        width: mcl.width; uiScale: panel.uiScale; onEdited: function(value) { panel.change("ifg",value) } }
                    LiveNumberField { id: ipg; label: qsTr("IPG"); value: panel.draft.ipg; minimum: 0; maximum: 99
                        width: mcl.width; uiScale: panel.uiScale; onEdited: function(value) { panel.change("ipg",value) } }
                }
                Row {
                    spacing: 12*panel.uiScale
                    ActionButton { text: qsTr("BiPolar") + (panel.draft.biPolar ? qsTr(" · On") : qsTr(" · Off")); width: mcl.width
                        height: 40*panel.uiScale; checked: panel.draft.biPolar; uiScale: panel.uiScale
                        onClicked: panel.change("biPolar",!panel.draft.biPolar) }
                    ActionButton { text: qsTr("BaseToApex") + (panel.draft.baseToApex ? qsTr(" · On") : qsTr(" · Off")); width: mcl.width
                        height: 40*panel.uiScale; checked: panel.draft.baseToApex; uiScale: panel.uiScale
                        onClicked: panel.change("baseToApex",!panel.draft.baseToApex) }
                }
            }
            Text {
                width: parent.width; wrapMode: Text.WordWrap; color: "#a59ab9"; font.pixelSize: Math.round(14*panel.uiScale)
                text: qsTr("Reference profile: Electrodogram · CIS\nPulse Width, IFG and IPG retain the reference values; their device behavior is pending integration.")
            }
        }
        Column {
            id: electrodeSettings
            width: scroll.width; visible: panel.tab===1; spacing: 18*panel.uiScale
            Text { width: parent.width; wrapMode: Text.WordWrap; color: "#bcb2cd"; font.pixelSize: Math.round(15*panel.uiScale)
                text: qsTr("Choose visible electrodes and their reference resistance. Hidden electrodes remain in captured data.") }
            Row {
                spacing: 8*panel.uiScale
                ActionButton { text: qsTr("Enable all"); checked: panel.draft.selectionMode==="all"; uiScale: panel.uiScale; height: 40*panel.uiScale
                    onClicked: panel.change("selectionMode","all") }
                ActionButton { text: qsTr("Custom"); checked: panel.draft.selectionMode==="custom"; uiScale: panel.uiScale; height: 40*panel.uiScale
                    onClicked: panel.change("selectionMode","custom") }
            }
            Grid {
                width: parent.width; columns: 4; spacing: 10*panel.uiScale
                Repeater {
                    id: resistanceFields
                    model: 16
                    delegate: Column {
                        id: electrode
                        required property int index
                        readonly property bool valid: resistance.valid
                        width: (scroll.width-30*panel.uiScale)/4; spacing: 8*panel.uiScale
                        ActionButton {
                            width: parent.width; height: 36*panel.uiScale; text: "EL"+(electrode.index+1); uiScale: panel.uiScale
                            checked: panel.draft.selectionMode==="all" || panel.draft.electrodes[electrode.index]
                            enabled: panel.draft.selectionMode==="custom"
                            onClicked: panel.changeElectrode(electrode.index,!panel.draft.electrodes[electrode.index])
                        }
                        LiveNumberField {
                            id: resistance
                            width: parent.width; uiScale: panel.uiScale; label: qsTr("Resistance"); suffix: "Ω"
                            value: panel.draft.resistances[electrode.index]; minimum: 1; maximum: 20000; step: 100
                            enabled: !panel.acquisitionBusy
                            onEdited: function(value) { panel.changeResistance(electrode.index,value) }
                        }
                        Rectangle { width: parent.width; height: 2*panel.uiScale; color: Live.channelColor(electrode.index) }
                    }
                }
            }
            Text { width: parent.width; wrapMode: Text.WordWrap; color: "#a59ab9"; font.pixelSize: Math.round(14*panel.uiScale)
                text: qsTr("Enable all preserves your custom selection. Resistance range: 1–20000 Ω; default: 2000 Ω.") }
        }
    }
    Rectangle {
        // Scroll position remains visible without adding a Qt Quick Controls dependency.
        anchors.right: parent.right; anchors.rightMargin: 5*panel.uiScale
        y: scroll.y + scroll.visibleArea.yPosition*scroll.height
        width: 3*panel.uiScale; height: Math.max(16*panel.uiScale,scroll.visibleArea.heightRatio*scroll.height)
        radius: width/2; color: "#8f7aa9"; visible: scroll.contentHeight>scroll.height
    }
    Column {
        id: footer
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 20*panel.uiScale; spacing: 12*panel.uiScale
        Text { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: Math.round(14*panel.uiScale)
            color: panel.validationError.length || !panel.fieldsValid ? "#ff9aa5" : "#a59ab9"
            text: !panel.fieldsValid ? qsTr("Enter valid whole numbers in the highlighted fields.") : panel.validationError.length ? panel.validationError : qsTr("Changes apply only after Save.") }
        Row {
            spacing: 10*panel.uiScale
            ActionButton { text: qsTr("Save"); primary: true; width: 210*panel.uiScale; uiScale: panel.uiScale
                enabled: !panel.validationError.length && panel.fieldsValid
                onClicked: panel.applied(Live.copy(panel.draft)) }
            ActionButton { text: qsTr("Cancel"); width: 210*panel.uiScale; uiScale: panel.uiScale; onClicked: panel.cancelled() }
        }
    }
}
