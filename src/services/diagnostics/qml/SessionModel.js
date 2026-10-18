.pragma library
.import "Analysis.js" as Analysis

// Domain session state only. No UART, FTDI, FlexIO or inter-core packet parsing.
function Session() {
    this.availableModes = []
    this.ticket = 0; this.phase = "idle"; this.mode = ""; this.message = "Acquisition is not connected."
    this.config = {}; this.rows = new Analysis.RollingRows(8000, 16); this.displayRows = []
    this.displayTotal = 0; this.liveGeneration = 0; this.liveConfig = {}; this.liveLastElapsed = -1
    this.paused = false; this.results = []; this.observation = null; this.microphones = {}
    this.monitorY = []; this.monitorSpl = []; this.dspRows = []; this.micConfig = Analysis.micDefaults()
    this.dspConfig = Analysis.dspDefaults(); this.dspBands = {}
    this.lastSequence = -1; this.lastElapsed = -1; this.dropped = 0; this.events = []
}
Session.prototype.busy = function() { return ["starting","running","stopping"].indexOf(this.phase) >= 0 }
Session.prototype.log = function(message) {
    this.message = message
    this.events.push(message)
    if (this.events.length > 200) this.events.shift()
}
Session.prototype.start = function(mode, config) {
    Analysis.requireValue(!this.busy(), "Finish the current session first.")
    Analysis.requireValue(this.availableModes.indexOf(mode) >= 0, "Acquisition is not connected.")
    if (mode === "dsp-calibration") Analysis.validateDsp(config)
    else if (mode === "automatic-mic") Analysis.validateMic(config)
    else Analysis.requireValue(mode === "live-electrodogram" || mode === "lanxi-measurement", "Unknown diagnostic mode.")
    this.config = Analysis.copy(config); this.mode = mode; this.phase = "starting"; ++this.ticket
    if (mode === "live-electrodogram") {
        this.rows = new Analysis.RollingRows(8000,16); this.displayRows = []; this.paused = false
        this.displayTotal = 0; this.liveGeneration = this.ticket; this.liveConfig = Analysis.copy(config); this.liveLastElapsed = -1
    } else if (mode === "dsp-calibration") {
        this.results = []; this.observation = null; this.monitorY = []; this.monitorSpl = []; this.dspRows = []
        this.dspConfig = Analysis.copy(config); this.dspBands = {}
    } else if (mode === "automatic-mic") { this.microphones = {}; this.micConfig = Analysis.copy(config) }
    this.lastSequence = -1; this.lastElapsed = -1; this.dropped = 0; this.events = []
    this.log("Waiting for acquisition to start.")
    return { ticket: this.ticket, mode: mode, settings: Analysis.copy(config) }
}
Session.prototype.started = function(ticket) {
    if (ticket !== this.ticket || this.phase !== "starting") return false
    this.phase = "running"; this.log("Acquisition running."); return true
}
Session.prototype.stop = function() {
    Analysis.requireValue(this.phase === "running" || this.phase === "starting", "No active session.")
    this.phase = "stopping"; this.log("Finishing acquisition and saving evidence."); return this.ticket
}
Session.prototype.stopped = function(ticket) {
    if (ticket !== this.ticket || !this.busy()) return false
    this.phase = "idle"; this.log("Session finished."); return true
}
Session.prototype.failed = function(ticket, reason) {
    if (ticket !== this.ticket || !this.busy()) return false
    this.phase = "error"; this.log(reason); return true
}
Session.prototype.accepts = function(ticket, mode) {
    return ticket === this.ticket && this.mode === mode && ["running","stopping"].indexOf(this.phase) >= 0
}
Session.prototype.acceptRows = function(ticket, batch) {
    if (!this.accepts(ticket,"live-electrodogram")) return false
    Analysis.requireValue(Number.isSafeInteger(batch.sequence) && batch.sequence > this.lastSequence
        && Analysis.finite(batch.elapsedMs) && batch.elapsedMs >= this.lastElapsed,
        "Out-of-order electrodogram data.")
    Analysis.requireValue(Array.isArray(batch.rows) && batch.rows.length > 0 && batch.rows.length <= 8000,
        "Invalid electrodogram batch size.")
    // Optional source identity must agree with the configuration accepted at start.
    Analysis.requireValue(batch.plotSource === undefined || batch.plotSource === this.liveConfig.plotSource,
        "Electrodogram source does not match the active capture.")
    // Validate the whole batch before making any change or recording it.
    batch.rows.forEach(function(row) {
        Analysis.requireValue(Array.isArray(row) && row.length === 16, "Expected 16 mapped electrode values.")
        row.forEach(function(v) { Analysis.requireValue(v === null || Analysis.finite(v), "Invalid electrode value.") })
    })
    Analysis.requireValue(Number.isSafeInteger(batch.sequence+batch.rows.length-1), "Sequence overflow.")
    if (this.lastSequence >= 0) this.dropped += batch.sequence - this.lastSequence - 1
    for (var i=0; i<batch.rows.length; ++i) this.rows.append(batch.rows[i])
    this.lastSequence = batch.sequence+batch.rows.length-1; this.lastElapsed = batch.elapsedMs
    this.liveLastElapsed = batch.elapsedMs
    if (!this.paused) { this.displayRows = this.rows.snapshot(); this.displayTotal = this.rows.total }
    return true
}
Session.prototype.pause = function(value) {
    this.paused = value
    if (!value) { this.displayRows = this.rows.snapshot(); this.displayTotal = this.rows.total }
}
Session.prototype.acceptDsp = function(ticket, observation) {
    if (!this.accepts(ticket,"dsp-calibration")) return false
    Analysis.requireValue(Number.isSafeInteger(observation.sequence) && observation.sequence > this.lastSequence,
        "Out-of-order calibration observation.")
    Analysis.requireValue(Number.isInteger(observation.band) && observation.band >= 1 && observation.band <= 16
        && Analysis.finite(observation.frequencyHz) && observation.frequencyHz > 0, "Invalid band or frequency.")
    // The adapter supplies decoded electrical Y values and measured/calibrated SPL.
    // Raw ADC codes and uncalibrated PCM are never accepted as those quantities.
    var measured = observation.measuredY
    if (Array.isArray(observation.yValues)) {
        measured = Analysis.exponentialAverage(observation.yValues, this.config.samplesPerFrame,
                    this.config.referenceSampleRate, this.config.averagingSeconds).mean
    }
    var result = Analysis.evaluateDsp(this.config, measured, observation.measuredSpl)
    if (observation.electrodeRows !== undefined) {
        Analysis.requireValue(Array.isArray(observation.electrodeRows) && observation.electrodeRows.length <= 1000,
            "Invalid calibration electrodogram size.")
        observation.electrodeRows.forEach(function(row) {
            Analysis.requireValue(Array.isArray(row) && row.length === 16, "Expected 16 mapped electrodes.")
            row.forEach(function(v) { Analysis.requireValue(v === null || Analysis.finite(v), "Invalid electrode value.") })
        })
    }
    var bandState = this.dspBands[observation.band]
    if (bandState) Analysis.requireValue(bandState.frequencyHz === observation.frequencyHz,
        "Band frequency changed during calibration. Start a new session for a new band plan.")
    else bandState = { frequencyHz: observation.frequencyHz, iterations: 0,
                      controller: new Analysis.LevelController(this.config) }
    var applied = observation.appliedLevelDb
    if (applied !== undefined) Analysis.requireValue(Analysis.finite(applied)
        && applied >= this.config.minimumLevel && applied <= this.config.maximumLevel,
        "Applied stimulus is outside the configured limits.")
    result.iteration = bandState.iterations+1
    result.appliedLevelDb = applied === undefined ? null : applied
    result.decision = null; result.nextLevelDb = null
    result.iterationLimit = !result.converged && this.config.maxIterations > 0
        && result.iteration >= this.config.maxIterations
    if (!result.converged && !result.iterationLimit && applied !== undefined && this.phase === "running") {
        result.decision = bandState.controller.next(applied,measured,result.targetY)
        result.nextLevelDb = result.decision.nextLevelDb
    }
    bandState.iterations = result.iteration; this.dspBands[observation.band] = bandState
    result.band = observation.band; result.frequencyHz = observation.frequencyHz
    this.lastSequence = observation.sequence; this.observation = result
    this.monitorY.push(measured); this.monitorY = this.monitorY.slice(-240)
    this.monitorSpl.push(observation.measuredSpl); this.monitorSpl = this.monitorSpl.slice(-240)
    if (observation.electrodeRows) this.dspRows = observation.electrodeRows.map(function(row) { return row.slice() })
    if (result.converged) {
        this.results = this.results.filter(function(row) { return row.band !== result.band })
        this.results.push(result); this.results.sort(function(a,b) { return a.band-b.band })
        this.log("Band " + result.band + " converged.")
    } else if (result.iterationLimit) this.log("Band " + result.band + " reached the iteration limit without convergence.")
    else if (result.decision) this.log("Band " + result.band + ", iteration " + result.iteration
        + ": " + result.decision.strategy + ", next stimulus " + result.nextLevelDb.toFixed(3) + " dB.")
    return true
}
Session.prototype.acceptMic = function(ticket, block, receivedAtMs) {
    if (!this.accepts(ticket,"automatic-mic")) return false
    Analysis.requireValue(block.channel === "Front" || block.channel === "Back", "Unknown microphone channel.")
    Analysis.requireValue(Number.isSafeInteger(block.sequence) && block.sequence >= 0
        && Analysis.finite(block.elapsedMs) && block.elapsedMs >= 0
        && Analysis.finite(receivedAtMs) && receivedAtMs >= 0, "Invalid microphone timing.")
    Analysis.requireValue(Number.isInteger(block.sampleRate) && block.sampleRate >= 1000 && block.sampleRate <= 192000,
        "Unsupported microphone sample rate.")
    Analysis.requireValue(Array.isArray(block.samples) && block.samples.length > 0 && block.samples.length <= 65536,
        "Invalid microphone block size.")
    block.samples.forEach(function(v) { Analysis.requireValue(Analysis.finite(v) && Math.abs(v) <= 1,
        "Microphone samples must already be normalized PCM in [-1, 1].") })
    var prior = this.microphones[block.channel]
    if (prior) Analysis.requireValue(block.sequence > prior.sequence && block.elapsedMs >= prior.elapsedMs,
        "Out-of-order microphone data.")
    var gap = prior && (prior.sampleRate !== block.sampleRate || block.sequence !== prior.sequence+1 || prior.stale
        || block.elapsedMs-prior.elapsedMs > Math.max(1000,this.config.windowMs*2))
    var channel = !prior || gap ? { samples: [], total: prior ? prior.total : 0,
        history: prior ? prior.history.slice() : [], guidance: Analysis.createGuidance() } : prior
    if (gap) { ++this.dropped; channel.history.push(null) }
    channel.sampleRate = block.sampleRate; channel.sequence = block.sequence; channel.elapsedMs = block.elapsedMs
    channel.receivedAtMs = receivedAtMs; channel.stale = false; channel.total += block.samples.length
    var count = Math.ceil(block.sampleRate*this.config.windowMs/1000)
    var capacity = Math.max(count,this.config.fftSize)
    channel.samples = channel.samples.concat(block.samples).slice(-capacity)
    channel.stats = null; channel.spectrum = null; channel.quality = "Collecting samples"
    if (channel.samples.length >= count && channel.samples.length >= this.config.fftSize) {
        channel.stats = Analysis.rms(channel.samples.slice(-count))
        channel.spectrum = Analysis.spectrum(channel.samples,block.sampleRate,this.config.fftSize,this.config.window)
        var valid = Analysis.finite(channel.stats.dbfs) && channel.stats.clipping <= this.config.maxClipping
        channel.quality = valid ? "Valid" : "Silent or clipped signal"
        if (this.config.validateTone && (this.config.expectedHz >= block.sampleRate/2
            || channel.spectrum.peakHz === null
            || Math.abs(channel.spectrum.peakHz-this.config.expectedHz)>this.config.frequencyTolerance)) {
            valid = false; channel.quality = "Test tone outside tolerance"
        }
        channel.hold = Analysis.updateGuidance(channel.guidance,this.config,block.elapsedMs,channel.stats.dbfs,valid)
        channel.stats.spl = Analysis.finite(block.splOffsetDb) ? channel.stats.dbfs+block.splOffsetDb : null
        channel.history.push(Analysis.finite(channel.stats.dbfs) ? channel.stats.dbfs : null)
    } else channel.hold = {status:"Collecting samples",progress:0,achieved:false}
    channel.history = channel.history.slice(-240)
    this.microphones[block.channel] = channel
    return true
}
Session.prototype.expire = function(nowMs) {
    if (!this.accepts(this.ticket,"automatic-mic")) return false
    var changed = false, timeout = Math.max(1000,this.config.windowMs*2)
    Object.keys(this.microphones).forEach(function(name) {
        var channel=this.microphones[name]
        if (!channel.stale && nowMs-channel.receivedAtMs > timeout) {
            channel.stale=true; channel.guidance=Analysis.createGuidance()
            channel.hold={status:"No new samples",progress:0,achieved:false}; channel.quality="No new samples"
            changed=true
        }
    },this)
    return changed
}
Session.prototype.selectedMicNames = function() {
    var choice = this.micConfig.microphone
    if (choice === "Both") return ["Front","Back"]
    if (choice === "Front" || choice === "Back") return [choice]
    // Source AUTO prioritizes Front; Back is selected only when Front is absent.
    return [this.microphones.Front ? "Front" : "Back"]
}
Session.prototype.micQuality = function() {
    var names=this.selectedMicNames(), progress=1, achieved=true
    for (var i=0;i<names.length;++i) {
        var channel=this.microphones[names[i]]
        if (!channel || !channel.hold) return {status:"Waiting for " + names[i] + " microphone",progress:0,achieved:false}
        progress=Math.min(progress,channel.hold.progress); achieved=achieved && channel.hold.achieved && !channel.stale
        if (!channel.hold.achieved) return {status:names[i] + ": " + channel.hold.status,progress:progress,achieved:false}
    }
    return {status:"Target achieved — select Finish when ready",progress:progress,achieved:achieved}
}
