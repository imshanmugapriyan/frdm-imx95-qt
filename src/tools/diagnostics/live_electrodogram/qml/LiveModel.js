.pragma library

// Live-tab presentation and reference settings. No legacy packet decoding or
// ADC-to-current conversion belongs here; input rows are already normalized.
var capacity = 8000
var pulseWidths = [8, 10, 20, 40, 60, 80, 100, 200]
var gains = [0.25, 0.5, 0.75, 1, 1.5, 2, 3]
var rates = [250, 500, 1000, 2000]
var tolerances = [10, 20, 30, 40, 50]

function copy(value) { return JSON.parse(JSON.stringify(value)) }
function defaults() {
    return { mcl:4095, thl:1, pulseWidth:40, gain:1, pps:500, tolerance:20,
        ifg:3, ipg:8, biPolar:false, baseToApex:false, plotSource:"STIM_EVALUE",
        selectionMode:"all", electrodes:Array(16).fill(true), resistances:Array(16).fill(2000) }
}
function requireValue(condition, message) { if (!condition) throw new Error(message) }
function integer(value, low, high) { return Number.isInteger(value) && value >= low && value <= high }
function validate(settings) {
    requireValue(settings && integer(settings.mcl,1,4095) && integer(settings.thl,1,settings.mcl),
                 "Use 1–4095 for MCL and THL, with THL ≤ MCL.")
    requireValue(pulseWidths.indexOf(settings.pulseWidth) >= 0 && gains.indexOf(settings.gain) >= 0
                 && rates.indexOf(settings.pps) >= 0 && tolerances.indexOf(settings.tolerance) >= 0,
                 "Choose a supported pulse width, gain, PPS and tolerance.")
    requireValue(integer(settings.ifg,0,99) && integer(settings.ipg,0,99), "IFG and IPG must be 0–99.")
    requireValue(typeof settings.biPolar === "boolean" && typeof settings.baseToApex === "boolean",
                 "Invalid signal path selection.")
    requireValue(["STIM_EVALUE","ASK_EVALUE"].indexOf(settings.plotSource) >= 0, "Choose a plot source.")
    requireValue(["all","custom"].indexOf(settings.selectionMode) >= 0, "Choose an electrode selection mode.")
    requireValue(Array.isArray(settings.electrodes) && settings.electrodes.length === 16
        && settings.electrodes.every(function(v) { return typeof v === "boolean" }), "Expected 16 electrode selections.")
    requireValue(Array.isArray(settings.resistances) && settings.resistances.length === 16
        && settings.resistances.every(function(v) { return integer(v,1,20000) }), "Resistance must be 1–20000 Ω for each electrode.")
    return true
}
function error(settings) { try { validate(settings); return "" } catch (e) { return e.message } }
function enabled(settings) { return settings.selectionMode === "all" ? Array(16).fill(true) : settings.electrodes.slice() }
function channelColor(channel) { return channel % 2 === 0 ? "#67caff" : "#ff8d99" }
function sourceLabel(source) { return source === "ASK_EVALUE" ? "ASK E-Value" : "STIM E-Value" }
function captureSettings(settings) {
    validate(settings)
    var result = copy(settings)
    // Graph visibility never disables acquisition or removes recorded channels.
    delete result.selectionMode
    delete result.electrodes
    result.profile = "live-electrodogram-v1"
    return result
}
function points(value) { return Math.max(10, Math.min(capacity, Math.round(value / 10) * 10)) }
function zoom(value, direction) { return points(value + direction * Math.max(50, Math.floor(value / 50) * 10)) }

// Absolute accepted-row ordinals keep a history view stationary as data arrives.
// When the ring overwrites that view, clamp it to the oldest surviving row.
function windowFor(total, length, count, following, requestedStart) {
    var oldest = Math.max(0,total-length)
    var latest = oldest + Math.max(0,length-count)
    var start = following ? latest : Math.max(oldest,Math.min(latest,requestedStart))
    return { oldest:oldest, latest:latest, start:start, offset:start-oldest, count:Math.min(count,length) }
}
function series(rows, mask, offset, count) {
    var result = []
    // QPainter lays lanes out from top to bottom: EL16 first, EL1 last.
    for (var channel=15; channel>=0; --channel) {
        if (!mask[channel]) continue
        var values = []
        for (var i=offset; i<Math.min(rows.length,offset+count); ++i) values.push(rows[i][channel])
        result.push({ label:"EL"+(channel+1), color:channelColor(channel), values:values })
    }
    return result
}
