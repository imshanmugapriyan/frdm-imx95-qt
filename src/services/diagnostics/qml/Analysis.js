.pragma library

// Transport-independent calculations. Reference mapping is in docs/JAVA_MIGRATION.md.
function finite(value) { return typeof value === "number" && isFinite(value) }
function requireValue(condition, message) { if (!condition) throw new Error(message) }
function clamp(value, minimum, maximum) { return Math.max(minimum, Math.min(maximum, value)) }
function copy(value) { return JSON.parse(JSON.stringify(value)) }

function dspDefaults() {
    return { mcl: 1000, thr: 100, idr: 60, maxError: 0.8, stimulus: "Sine tone",
             averagingSeconds: 1, samplesPerFrame: 32, referenceSampleRate: 16000,
             kp: 0.8, ki: 0.05, kd: 0, minimumCorrection: 0.01, maximumCorrection: 15,
             integralLimit: 10, minimumLevel: 30, maximumLevel: 110, maxIterations: 30 }
}
function micDefaults() {
    return { target: -30, tolerance: 2, windowMs: 500, holdSeconds: 2,
             stability: 0.4, hysteresis: 0.1, fineMultiplier: 4, microphone: "Auto",
             fftSize: 1024, window: "Hamming", validateTone: false, expectedHz: 1000,
             frequencyTolerance: 20, maxClipping: 0 }
}
function validateDsp(config) {
    requireValue(finite(config.mcl) && finite(config.thr) && config.thr > 0
                 && config.mcl > config.thr && config.mcl <= 5000 && config.thr <= 1000,
                 "MCL must exceed THR; supported limits are MCL 5000 and THR 1000.")
    requireValue([20,30,40,50,60,70,80].indexOf(config.idr) >= 0, "Choose a supported IDR.")
    requireValue(finite(config.maxError) && config.maxError >= 0, "Maximum Y error cannot be negative.")
    requireValue(finite(config.averagingSeconds) && config.averagingSeconds > 0
                 && finite(config.referenceSampleRate) && config.referenceSampleRate > 0
                 && Number.isInteger(config.samplesPerFrame) && config.samplesPerFrame > 0,
                 "Averaging requires positive time, sample rate and samples per frame.")
    requireValue(["Sine tone","1/3-octave noise"].indexOf(config.stimulus) >= 0, "Unknown calibration stimulus.")
    validateController(config)
    return config
}
function validateController(config) {
    requireValue(finite(config.kp) && config.kp > 0 && finite(config.ki) && config.ki >= 0
                 && finite(config.kd) && config.kd >= 0, "PID gains must be finite; P must be positive.")
    requireValue(finite(config.minimumCorrection) && config.minimumCorrection > 0
                 && finite(config.maximumCorrection) && config.maximumCorrection >= config.minimumCorrection
                 && finite(config.integralLimit) && config.integralLimit >= 0,
                 "Check correction and integral limits.")
    requireValue(finite(config.minimumLevel) && finite(config.maximumLevel)
                 && config.maximumLevel > config.minimumLevel, "Maximum stimulus level must exceed minimum.")
    requireValue(Number.isInteger(config.maxIterations) && config.maxIterations >= 0,
                 "Maximum iterations must be an integer; zero means unlimited.")
}
function dspTargets(config) {
    validateDsp(config)
    return { y: Math.sqrt(config.mcl * config.thr), spl: 30 + config.idr / 2 }
}
function evaluateDsp(config, measuredY, measuredSpl) {
    var target = dspTargets(config)
    requireValue(finite(measuredY) && measuredY >= 0 && finite(measuredSpl),
                 "A finite, non-negative Y measurement and calibrated SPL are required.")
    var error = Math.abs(measuredY - target.y)
    var coefficient = Math.pow(10, (measuredSpl - target.spl) / 10)
    requireValue(finite(coefficient) && coefficient > 0, "Calibrated SPL produces an invalid coefficient.")
    return { measuredY: measuredY, measuredSpl: measuredSpl, targetY: target.y,
             targetSpl: target.spl, error: error, percentError: error / target.y * 100,
             converged: error <= config.maxError,
             coefficient: coefficient }
}

// Java AdaptivePidLevelController behavior; a caller must test convergence first.
// Applied stimulus setting is distinct from measured microphone SPL.
function LevelController(config) {
    validateController(config)
    this.config = copy(config); this.integral = 0; this.previous = null
    this.below = null; this.above = null; this.sequence = 0
}
LevelController.prototype.next = function(applied, measured, target) {
    var c = this.config, epsilon = 1e-9
    requireValue(finite(applied) && applied >= c.minimumLevel && applied <= c.maximumLevel,
                 "Applied stimulus is outside the configured limits.")
    requireValue(finite(measured) && measured >= 0 && finite(target) && target > 0,
                 "Invalid electrical measurement or target.")
    ++this.sequence
    var decision = { appliedLevelDb: applied, measuredY: measured, electricalErrorDb: null,
        proportionalTermDb: 0, integralTermDb: 0, derivativeTermDb: 0,
        requestedCorrectionDb: 0, nextLevelDb: applied, strategy: "PID",
        correctionLimited: false, levelLimited: false, lowerBracketDb: null, upperBracketDb: null }
    if (measured <= 1e-12) {
        this.previous = null; this.integral = 0
        decision.strategy = "ZERO_SIGNAL_RECOVERY"
        decision.requestedCorrectionDb = c.maximumCorrection
        decision.nextLevelDb = clamp(applied+c.maximumCorrection,c.minimumLevel,c.maximumLevel)
        decision.correctionLimited = true; decision.levelLimited = decision.nextLevelDb >= c.maximumLevel
    } else {
        var error = 20*Math.log10(target/measured)
        decision.electricalErrorDb = error
        var observation = { level: applied, error: error, sequence: this.sequence }
        if (error > 0 && (!this.below || applied > this.below.level)) this.below = observation
        if (error < 0 && (!this.above || applied < this.above.level)) this.above = observation
        if (this.below && this.above && this.below.level >= this.above.level) {
            if (this.below.sequence > this.above.sequence) this.above = null; else this.below = null
        }
        if (this.below && this.above
            && this.above.level-this.below.level <= Math.max(0.001,c.minimumCorrection*0.1)) {
            if (error > 0) { this.below=observation; this.above=null }
            else if (error < 0) { this.above=observation; this.below=null }
        }
        if (this.below && this.above) {
            this.integral = 0; this.previous = error
            var lower=this.below.level, upper=this.above.level, width=upper-lower
            var candidate=lower+this.below.error/(this.below.error-this.above.error)*width
            if (!finite(candidate)) candidate=lower+width/2
            candidate=clamp(candidate,lower+width*0.1,upper-width*0.1)
            decision.strategy="BRACKETED_INTERPOLATION"
            decision.lowerBracketDb=lower; decision.upperBracketDb=upper
            decision.requestedCorrectionDb=candidate-applied
            decision.correctionLimited=Math.abs(decision.requestedCorrectionDb)>c.maximumCorrection
            decision.nextLevelDb=clamp(clamp(applied+clamp(decision.requestedCorrectionDb,
                -c.maximumCorrection,c.maximumCorrection),lower+epsilon,upper-epsilon),c.minimumLevel,c.maximumLevel)
            decision.levelLimited=decision.nextLevelDb<=c.minimumLevel || decision.nextLevelDb>=c.maximumLevel
        } else {
            var candidateIntegral=clamp(this.integral+error,-c.integralLimit,c.integralLimit)
            var p=c.kp*error, i=c.ki*candidateIntegral, d=c.kd*(this.previous===null ? 0 : error-this.previous)
            var raw=p+i+d
            if (!finite(raw) || raw*error<=0) { raw=p; i=0; d=0 }
            requireValue(finite(raw),"PID correction overflow.")
            var correction=(error<0 ? -1 : 1)*clamp(Math.abs(raw),c.minimumCorrection,c.maximumCorrection)
            decision.proportionalTermDb=p; decision.integralTermDb=i; decision.derivativeTermDb=d
            decision.requestedCorrectionDb=raw
            decision.correctionLimited=Math.abs(correction-raw)>epsilon
            decision.nextLevelDb=clamp(applied+correction,c.minimumLevel,c.maximumLevel)
            decision.levelLimited=Math.abs(decision.nextLevelDb-(applied+correction))>epsilon
            if (!decision.correctionLimited && !decision.levelLimited) this.integral=candidateIntegral
            this.previous=error
        }
    }
    requireValue(Math.abs(decision.nextLevelDb-applied)>epsilon,
                 "Cannot reach the target within the configured stimulus limits.")
    decision.appliedCorrectionDb=decision.nextLevelDb-applied
    return decision
}
function exponentialAverage(values, samplesPerFrame, sampleRate, timeConstant) {
    requireValue(values.length > 0 && Number.isInteger(samplesPerFrame) && samplesPerFrame > 0
                 && finite(sampleRate) && sampleRate > 0 && finite(timeConstant)
                 && timeConstant > 0, "Averaging requires samples and positive timing.")
    var alpha = Math.exp(-samplesPerFrame / sampleRate / timeConstant)
    var filtered = []
    for (var i = 0; i < values.length; ++i) {
        requireValue(finite(values[i]), "Invalid electrical measurement.")
        // Preserve the Java/MATLAB initial condition, including its first-sample overshoot.
        filtered.push((1 - alpha) * values[i] + (i === 0 ? values[i] : alpha * filtered[i - 1]))
    }
    // The source starts at ceil(N/4)-1. Despite its name this is NOT the final quarter.
    var start = Math.max(0, Math.ceil(values.length / 4) - 1)
    var total = 0
    for (var j = start; j < filtered.length; ++j) total += filtered[j]
    return { values: filtered, mean: total / (filtered.length - start) }
}
function validateMic(config) {
    requireValue(finite(config.target) && config.target <= 0, "Target RMS must be at or below 0 dBFS.")
    requireValue(finite(config.tolerance) && config.tolerance > 0, "Tolerance must be positive.")
    requireValue(finite(config.windowMs) && config.windowMs >= 20 && config.windowMs <= 60000,
                 "RMS window must be between 20 and 60000 ms.")
    requireValue(finite(config.holdSeconds) && config.holdSeconds > 0, "Hold time must be positive.")
    requireValue(finite(config.stability) && config.stability >= 0
                 && finite(config.hysteresis) && config.hysteresis >= 0
                 && config.hysteresis < config.tolerance, "Check stability and hysteresis limits.")
    requireValue(finite(config.fineMultiplier) && config.fineMultiplier > 1, "Fine multiplier must exceed 1.")
    requireValue(Number.isInteger(config.fftSize) && config.fftSize >= 128 && config.fftSize <= 65536
                 && (config.fftSize & (config.fftSize - 1)) === 0, "FFT size must be a power of two, 128–65536.")
    requireValue(["Hamming", "Hann", "Rectangular"].indexOf(config.window) >= 0, "Unsupported FFT window.")
    requireValue(["Auto", "Front", "Back", "Both"].indexOf(config.microphone) >= 0, "Unknown microphone selection.")
    requireValue(finite(config.expectedHz) && config.expectedHz > 0
                 && finite(config.frequencyTolerance) && config.frequencyTolerance >= 0,
                 "Check expected frequency and tolerance.")
    requireValue(finite(config.maxClipping) && config.maxClipping >= 0 && config.maxClipping <= 100,
                 "Maximum clipping must be between 0 and 100 percent.")
    return config
}
function rms(samples) {
    requireValue(samples.length > 0, "No samples.")
    var scale = 0, sum = 1, clipped = 0
    for (var i = 0; i < samples.length; ++i) {
        var absolute = Math.abs(samples[i])
        requireValue(finite(samples[i]), "Non-finite audio sample.")
        if (absolute >= 1) ++clipped
        if (absolute === 0) continue
        if (scale < absolute) { sum = 1 + sum * Math.pow(scale / absolute, 2); scale = absolute }
        else sum += Math.pow(absolute / scale, 2)
    }
    var value = scale * Math.sqrt(sum / samples.length)
    return { linear: value, dbfs: value === 0 ? -Infinity : 20 * Math.log10(value),
             clipping: 100 * clipped / samples.length }
}
function spectrum(samples, sampleRate, size, windowName) {
    requireValue(finite(sampleRate) && sampleRate > 0 && samples.length >= size
                 && size >= 128 && size <= 65536 && (size & (size - 1)) === 0,
                 "Not enough samples or invalid FFT parameters.")
    requireValue(["Hamming", "Hann", "Rectangular"].indexOf(windowName) >= 0, "Invalid FFT window.")
    var real = [], imaginary = [], gain = 0
    for (var i = 0; i < size; ++i) {
        var sample = samples[samples.length - size + i]
        requireValue(finite(sample), "Non-finite audio sample.")
        var phase = 2 * Math.PI * i / (size - 1)
        var w = windowName === "Hamming" ? 0.54 - 0.46 * Math.cos(phase)
              : windowName === "Hann" ? 0.5 - 0.5 * Math.cos(phase) : 1
        real[i] = sample * w; imaginary[i] = 0; gain += w
    }
    for (var n = 1, reversed = 0; n < size; ++n) {
        var bit = size >> 1
        for (; reversed & bit; bit >>= 1) reversed ^= bit
        reversed ^= bit
        if (n < reversed) { var swap = real[n]; real[n] = real[reversed]; real[reversed] = swap }
    }
    for (var length = 2; length <= size; length *= 2) {
        var angle = -2 * Math.PI / length
        for (var start = 0; start < size; start += length) {
            for (var k = 0; k < length / 2; ++k) {
                var even = start + k, odd = even + length / 2
                var cosine = Math.cos(angle * k), sine = Math.sin(angle * k)
                var r = real[odd] * cosine - imaginary[odd] * sine
                var im = real[odd] * sine + imaginary[odd] * cosine
                real[odd] = real[even] - r; imaginary[odd] = imaginary[even] - im
                real[even] += r; imaginary[even] += im
            }
        }
    }
    var bins = [], peakIndex = 0, peakMagnitude = 0
    for (var b = 0; b <= size / 2; ++b) {
        var magnitude = Math.sqrt(real[b] * real[b] + imaginary[b] * imaginary[b])
                        * (b === 0 || b === size / 2 ? 1 : 2) / gain
        bins.push(20 * Math.log10(Math.max(1e-8, magnitude)))
        // DC is not a test-tone frequency.
        if (b > 0 && magnitude > peakMagnitude) { peakMagnitude = magnitude; peakIndex = b }
    }
    return { values: bins, binHz: sampleRate / size,
             peakHz: peakMagnitude > 1e-8 ? peakIndex * sampleRate / size : null }
}
function createGuidance() { return { observations: [], lastTime: -1, holdStart: null, latched: false } }
function updateGuidance(state, config, elapsedMs, dbfs, qualityValid) {
    validateMic(config)
    requireValue(finite(elapsedMs) && elapsedMs >= state.lastTime && elapsedMs >= 0,
                 "Measurement time must be monotonic.")
    state.lastTime = elapsedMs
    if (!finite(dbfs) || !qualityValid) {
        state.observations = []; state.holdStart = null; state.latched = false
        return { status: "Invalid measurement", progress: 0, achieved: false }
    }
    state.observations.push({ time: elapsedMs, value: dbfs })
    state.observations = state.observations.filter(function(o) { return o.time >= elapsedMs - config.windowMs })
    var minimum = dbfs, maximum = dbfs
    state.observations.forEach(function(o) { minimum = Math.min(minimum, o.value); maximum = Math.max(maximum, o.value) })
    var error = dbfs - config.target
    if (maximum - minimum > config.stability) {
        state.holdStart = null; state.latched = false
        return { status: "Unstable — hold position", progress: 0, achieved: false }
    }
    if (Math.abs(error) <= config.tolerance + (state.latched ? config.hysteresis : 0)) {
        if (!state.latched) { state.latched = true; state.holdStart = elapsedMs }
        var progress = clamp((elapsedMs - state.holdStart) / (config.holdSeconds * 1000), 0, 1)
        return { status: progress >= 1 ? "Target achieved" : "Within target — hold position",
                 progress: progress, achieved: progress >= 1 }
    }
    state.holdStart = null; state.latched = false
    var fine = Math.abs(error) <= config.tolerance * config.fineMultiplier
    return { status: (error < 0 ? "Below target" : "Above target") + (fine ? " — fine adjustment" : " — adjust level"),
             progress: 0, achieved: false }
}

function RollingRows(capacity, columns) {
    requireValue(Number.isInteger(capacity) && capacity > 0 && Number.isInteger(columns) && columns > 0,
                 "Invalid buffer dimensions.")
    this.capacity = capacity; this.columns = columns; this.rows = []; this.head = 0; this.total = 0
}
RollingRows.prototype.append = function(row) {
    requireValue(Array.isArray(row) && row.length === this.columns, "Incorrect channel count.")
    row.forEach(function(v) { requireValue(v === null || finite(v), "Invalid channel value.") })
    if (this.rows.length < this.capacity) this.rows.push(row.slice())
    else { this.rows[this.head] = row.slice(); this.head = (this.head + 1) % this.capacity }
    ++this.total
}
RollingRows.prototype.snapshot = function() {
    var result = []
    for (var i = 0; i < this.rows.length; ++i) result.push(this.rows[(this.head + i) % this.rows.length].slice())
    return result
}
function seriesFromRows(rows, enabled, start, count) {
    var result = []
    for (var channel = enabled.length - 1; channel >= 0; --channel) {
        if (!enabled[channel]) continue
        var values = []
        for (var i = start; i < Math.min(rows.length, start + count); ++i) values.push(rows[i][channel])
        result.push({ label: "E" + (channel + 1), values: values })
    }
    return result
}
