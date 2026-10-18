import QtQuick

// The desktop launcher and dock share this registry. IDs are stable routing keys.
ListModel {
    // URLs are resolved here so callers never depend on their own directory.
    function describe(tool) {
        return {toolId:tool.toolId,title:tool.title,summary:tool.summary,
            category:tool.category,iconKind:tool.iconKind,accent:tool.accent,
            entryPoint:tool.entryPoint.length ? Qt.resolvedUrl(tool.entryPoint).toString() : ""}
    }
    function entries(diagnosticsOnly, searchText) {
        var result=[], query=diagnosticsOnly ? searchText.trim().toLowerCase() : ""
        for (var i=0;i<count;++i) {
            var tool=get(i)
            if (diagnosticsOnly && tool.category!=="diagnostic") continue
            if (query && (tool.title+" "+tool.summary).toLowerCase().indexOf(query)<0) continue
            result.push(describe(tool))
        }
        return result
    }
    function findTool(toolId) {
        for (var i=0;i<count;++i) if (get(i).toolId===toolId) return describe(get(i))
        return null
    }
    ListElement {
        toolId: "live-electrodogram"
        title: qsTr("Live Electrodogram")
        summary: qsTr("View live electrode activity")
        category: "diagnostic"
        iconKind: "electrodogram"
        accent: "#be9bff"
        entryPoint: "diagnostics/live_electrodogram/qml/LiveElectrodogram.qml"
    }
    ListElement {
        toolId: "acoustic-analytics"
        title: qsTr("Acoustic Analytics Studio")
        summary: qsTr("DSP and microphone calibration")
        category: "diagnostic"
        iconKind: "acoustic"
        accent: "#74caff"
        entryPoint: "diagnostics/acoustic_analytics/qml/AcousticAnalytics.qml"
    }
    ListElement {
        toolId: "lanxi-measurement"
        title: qsTr("LAN-XI Measurement Studio")
        summary: qsTr("Microphone and amplifier measurements, stimulus and frequency response with Types 3676 and 3677")
        category: "diagnostic"
        iconKind: "lanxi"
        accent: "#80d4ba"
        entryPoint: "diagnostics/lanxi/qml/LanXiStudio.qml"
    }
    ListElement {
        toolId: "notepad"; title: qsTr("Notepad"); summary: qsTr("Write and save session notes")
        category: "utility"; iconKind: "notepad"; accent: "#e9c480"
        entryPoint: ""
    }
    ListElement {
        toolId: "calculator"; title: qsTr("Calculator"); summary: qsTr("Quick arithmetic and percentages")
        category: "utility"; iconKind: "calculator"; accent: "#80d4ba"
        entryPoint: ""
    }
    ListElement {
        toolId: "results"; title: qsTr("Results"); summary: qsTr("Browse exported CSV and text files")
        category: "utility"; iconKind: "results"; accent: "#8abaff"
        entryPoint: ""
    }
    ListElement {
        toolId: "settings"; title: qsTr("Desktop Settings"); summary: qsTr("Appearance, clock and system information")
        category: "utility"; iconKind: "settings"; accent: "#b6b8d7"
        entryPoint: ""
    }
}
