pragma ComponentBehavior: Bound
import QtQuick
import "Arithmetic.js" as Arithmetic
import "../../../../shared/qml/controls" as Controls

Item {
    id: calculator
    property var history: []
    property string message: ""
    property bool justCalculated: false
    function focusInput() { input.forceActiveFocus() }
    function evaluate() {
        try {
            var expression=input.text, result=Arithmetic.calculate(expression)
            history=[{expression:expression,result:result}].concat(history).slice(0,12)
            input.text=result; justCalculated=true; message=""; input.forceActiveFocus(); input.selectAll()
        } catch(error) { message=error.message }
    }
    function enter(key) {
        message=""
        if (key === "C") input.clear()
        else if (key === "Back") {
            if (input.selectionStart!==input.selectionEnd) input.remove(input.selectionStart,input.selectionEnd)
            else if (input.cursorPosition>0) input.remove(input.cursorPosition-1,input.cursorPosition)
        } else {
            if (justCalculated) {
                if ("0123456789.(".indexOf(key)>=0) input.clear()
                else { input.deselect(); input.cursorPosition=input.length }
            }
            var position=input.selectionStart
            if (input.selectionStart!==input.selectionEnd) input.remove(input.selectionStart,input.selectionEnd)
            input.insert(position,key)
        }
        justCalculated=false; input.forceActiveFocus()
    }
    Item {
        x: 28; y: 8; width: 620; height: parent.height-16
        Rectangle {
            width: parent.width; height: 100; radius: 15; color: "#101a2b"; border.color: "#485972"
            TextInput { id: input; anchors.fill: parent; anchors.margins: 22; maximumLength: 256
                text: ""; color: "#edf9f6"; font.pixelSize: 34; selectByMouse: true; clip: true
                horizontalAlignment: TextInput.AlignRight; verticalAlignment: TextInput.AlignVCenter
                Accessible.name: qsTr("Calculation"); onAccepted: calculator.evaluate(); onTextEdited: calculator.justCalculated=false }
            Text { anchors.right: parent.right; anchors.rightMargin: 22; anchors.verticalCenter: parent.verticalCenter
                visible: !input.length; text: "0"; color: "#6e8199"; font.pixelSize: 34 }
        }
        Text { y: 112; width: parent.width; text: calculator.message; color: "#efbbad"; font.pixelSize: 18; elide: Text.ElideRight }
        Grid {
            y: 154; columns: 4; spacing: 12
            Repeater {
                model: ["C","(",")","Back","7","8","9","/","4","5","6","*","1","2","3","-","0",".","%","+"]
                delegate: Controls.ActionButton { required property string modelData
                    width: 146; height: 70
                    text: modelData === "*" ? "×" : modelData === "/" ? "÷" : modelData === "Back" ? "⌫" : modelData
                    onClicked: calculator.enter(modelData) }
            }
        }
        Controls.ActionButton { y: 564; width: parent.width; height: 64; text: "="; primary: true; onClicked: calculator.evaluate() }
    }
    Item {
        x: 700; y: 20; width: parent.width-x-20; height: parent.height-40
        Text { text: qsTr("Recent calculations"); color: "#f1eff9"; font.pixelSize: 26; font.weight: Font.DemiBold }
        Text { y: 44; width: parent.width; text: qsTr("Percent means divide by 100.\nExample: 200 × 10% = 20."); color: "#aebad0"; font.pixelSize: 17 }
        ListView {
            id: historyList; y: 112; width: parent.width; height: parent.height-y
            model: calculator.history; spacing: 10; clip: true
            delegate: Column {
                required property var modelData; width: historyList.width; spacing: 5
                Text { width: parent.width; text: parent.modelData.expression; elide: Text.ElideRight; color: "#b3bdd5"; font.pixelSize: 18 }
                Text { text: "= "+parent.modelData.result; color: "#9fe0cc"; font.pixelSize: 27 }
            }
        }
    }
}
