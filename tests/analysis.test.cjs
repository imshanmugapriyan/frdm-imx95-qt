const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const context = vm.createContext({});
vm.runInContext(fs.readFileSync(path.join(__dirname, '../src/services/diagnostics/qml/Analysis.js'), 'utf8')
    .replace(/^\.pragma library\s*/, ''), context);
const a = context;
const near = (actual, expected, tolerance = 1e-9) => assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} != ${expected}`);
let checks = 0;
function test(name, action) { action(); ++checks; process.stdout.write(`PASS ${name}\n`); }
test('DSP midpoint and power-domain EQ coefficient match source formulas', () => {
    const config = a.dspDefaults();
    near(a.dspTargets(config).y, 316.22776601683796);
    near(a.dspTargets(config).spl, 60);
    const result = a.evaluateDsp(config, Math.sqrt(100000), 70);
    near(result.coefficient, 10); assert.equal(result.converged, true);
    assert.equal(a.evaluateDsp(config, 300, 60).converged, false);
});
test('invalid electrical targets and non-finite measurements are rejected', () => {
    assert.throws(() => a.dspTargets({...a.dspDefaults(), thr: 1000}));
    assert.throws(() => a.dspTargets({...a.dspDefaults(), idr: 65}));
    assert.throws(() => a.evaluateDsp(a.dspDefaults(), -1, 60));
    assert.equal(a.evaluateDsp(a.dspDefaults(), 0, 60).converged, false);
    assert.throws(() => a.evaluateDsp(a.dspDefaults(), 300, NaN));
});

test('adaptive PID uses electrical dB error and limits corrections in both directions', () => {
    const first = new a.LevelController(a.dspDefaults()).next(60,10,100);
    near(first.electricalErrorDb,20); near(first.nextLevelDb,75);
    assert.equal(first.correctionLimited,true); assert.equal(first.strategy,'PID');
    near(new a.LevelController(a.dspDefaults()).next(60,1000,100).nextLevelDb,45);
    assert.throws(() => new a.LevelController({...a.dspDefaults(),kp:0}));
});

test('target crossing interpolates inside the bracket without minimum-step oscillation', () => {
    const target=Math.sqrt(100000), controller=new a.LevelController(a.dspDefaults());
    controller.next(60,100,target);
    const decision=controller.next(65,1000,target);
    near(decision.nextLevelDb,62.5); assert.equal(decision.strategy,'BRACKETED_INTERPOLATION');
    const narrow=new a.LevelController({...a.dspDefaults(),minimumCorrection:1});
    narrow.next(60,300,target);
    const move=narrow.next(60.2,333.3333333333,target);
    assert.equal(move.strategy,'BRACKETED_INTERPOLATION');
    assert.ok(move.nextLevelDb>60 && move.nextLevelDb<60.2);
});

test('noise below bracket resolution restarts bounded PID; zero input recovers or reaches a limit', () => {
    const target=Math.sqrt(100000), controller=new a.LevelController(a.dspDefaults());
    controller.next(84.7365,target-1,target);
    const decision=controller.next(84.737,target+3,target);
    assert.equal(decision.strategy,'PID'); assert.ok(decision.appliedCorrectionDb<=-0.01);
    const recovery=new a.LevelController(a.dspDefaults()).next(60,0,100);
    near(recovery.nextLevelDb,75); assert.equal(recovery.strategy,'ZERO_SIGNAL_RECOVERY');
    assert.throws(() => new a.LevelController(a.dspDefaults()).next(110,0,100));
    assert.throws(() => new a.LevelController(a.dspDefaults()).next(30,1000,100));
    assert.throws(() => new a.LevelController(a.dspDefaults()).next(60,NaN,100));
});

test('adaptive control converges on the reference deterministic response', () => {
    const controller=new a.LevelController(a.dspDefaults()), target=Math.sqrt(100000);
    let level=60, converged=false;
    for(let iteration=0;iteration<20;++iteration) {
        const measured=target*Math.pow(10,(level-62.3)/20);
        if (Math.abs(measured-target)<=0.8) { converged=true; break; }
        level=controller.next(level,measured,target).nextLevelDb;
    }
    assert.equal(converged,true); near(level,62.3,0.03);
});
test('source exponential filter initialization and retained range are preserved', () => {
    // alpha=0.5. Source first output is 1.5*x[0]; source range begins at ceil(N/4)-1.
    const result = a.exponentialAverage([2, 4, 6, 8, 10, 12, 14, 16], 1, 1, 1 / Math.log(2));
    near(result.values[0], 3); near(result.values[1], 3.5);
    near(result.mean, (3.5+4.75+6.375+8.1875+10.09375+12.046875+14.0234375)/7);
});
test('RMS preserves channel amplitude, silence and clipping', () => {
    near(a.rms([0.5, -0.5]).linear, 0.5); near(a.rms([0.5, -0.5]).dbfs, -6.020599913279624);
    assert.equal(a.rms([0, 0]).dbfs, -Infinity); near(a.rms([1, -1, 0, 0]).clipping, 50);
    near(a.rms([1e200, -1e200]).linear / 1e200, 1);
    assert.throws(() => a.rms([NaN])); assert.throws(() => a.rms([]));
});
test('FFT locates known tones and uses peak-amplitude dBFS', () => {
    const tone = Array.from({length:1024}, (_, i) => 0.5 * Math.sin(2*Math.PI*64*i/1024));
    const fft = a.spectrum(tone, 16000, 1024, 'Rectangular');
    near(fft.peakHz, 1000); near(fft.values[64], -6.020599913279624, 1e-8);
    near(a.spectrum(tone, 16000, 1024, 'Hamming').peakHz, 1000);
    assert.equal(a.spectrum(Array(1024).fill(0), 16000, 1024, 'Hann').peakHz, null);
});
test('hold depends on measured time and remains reversible after achievement', () => {
    const c = a.micDefaults(), state = a.createGuidance();
    assert.equal(a.updateGuidance(state,c,0,-30,true).progress, 0);
    near(a.updateGuidance(state,c,1000,-30,true).progress, 0.5);
    assert.equal(a.updateGuidance(state,c,2000,-30,true).achieved, true);
    assert.equal(a.updateGuidance(state,c,2100,-50,true).achieved, false);
    assert.throws(() => a.updateGuidance(state,c,2099,-30,true));
});
test('invalid quality and silence reset the hold', () => {
    const c = a.micDefaults(), state = a.createGuidance();
    a.updateGuidance(state,c,0,-30,true); a.updateGuidance(state,c,1900,-30,false);
    assert.equal(a.updateGuidance(state,c,2000,-30,true).progress, 0);
    assert.equal(a.updateGuidance(state,c,2100,-Infinity,true).achieved, false);
});
test('stability and hysteresis follow the source behavior', () => {
    const c = {...a.micDefaults(), stability: 10}, state = a.createGuidance();
    a.updateGuidance(state,c,0,-28,true);
    assert.ok(a.updateGuidance(state,c,1000,-27.95,true).progress > 0);
    assert.equal(a.updateGuidance(state,c,1100,-27.8,true).progress, 0);
    assert.throws(() => a.validateMic({...c, hysteresis: 2}));
});
test('rolling history remains chronological after wrap; missing channels stay missing', () => {
    const ring = new a.RollingRows(3,2);
    for (let i=0;i<5;++i) ring.append([i,null]);
    assert.deepEqual(JSON.parse(JSON.stringify(ring.snapshot())), [[2,null],[3,null],[4,null]]);
    assert.equal(ring.total,5); assert.throws(() => ring.append([1]));
    assert.throws(() => ring.append([NaN,0]));
    const series = a.seriesFromRows(ring.snapshot(),[true,false],1,2);
    assert.deepEqual(JSON.parse(JSON.stringify(series)), [{label:'E1',values:[3,4]}]);
});
context.Analysis = context;
vm.runInContext(fs.readFileSync(path.join(__dirname, '../src/services/diagnostics/qml/SessionModel.js'), 'utf8')
    .replace(/^\.(?:pragma|import).*$/gm, ''), context);
function session(mode, settings={}) {
    const value = new a.Session(); value.availableModes = [mode];
    const request = value.start(mode,settings); value.started(request.ticket); return value;
}
test('capture cannot start without a capability and waits for acknowledgement', () => {
    const s = new a.Session();
    assert.throws(() => s.start('live-electrodogram',{}));
    s.availableModes=['live-electrodogram']; const request=s.start('live-electrodogram',{});
    assert.equal(s.phase,'starting'); assert.equal(s.busy(),true);
    assert.equal(s.started(request.ticket+1),false); assert.equal(s.phase,'starting');
    assert.throws(() => s.start('live-electrodogram',{}));
    s.started(request.ticket); s.stop(); assert.equal(s.busy(),true);
    s.stopped(request.ticket); assert.equal(s.busy(),false);
});
test('late frames from a stopped generation cannot contaminate a new session', () => {
    const s=session('live-electrodogram'); const previous=s.ticket;
    s.stop(); s.stopped(previous); s.start('live-electrodogram',{}); s.started(s.ticket);
    assert.equal(s.acceptRows(previous,{sequence:0,elapsedMs:0,rows:[Array(16).fill(0.1)]}),false);
    assert.equal(s.rows.total,0);
});
test('pausing freezes the view while acquisition ingestion continues', () => {
    const s=session('live-electrodogram');
    s.acceptRows(s.ticket,{sequence:0,elapsedMs:0,rows:[Array(16).fill(0.1)]}); s.pause(true);
    s.acceptRows(s.ticket,{sequence:1,elapsedMs:10,rows:[Array(16).fill(0.2)]});
    assert.equal(s.displayRows.length,1); assert.equal(s.rows.total,2);
    s.pause(false); assert.equal(s.displayRows.length,2);
});
test('batch validation is atomic and sequence gaps remain observable', () => {
    const s=session('live-electrodogram');
    assert.throws(() => s.acceptRows(s.ticket,{sequence:0,elapsedMs:0,rows:[Array(16).fill(1),[NaN]]}));
    assert.equal(s.rows.total,0);
    s.acceptRows(s.ticket,{sequence:0,elapsedMs:0,rows:[Array(16).fill(0)]});
    s.acceptRows(s.ticket,{sequence:3,elapsedMs:1,rows:[Array(16).fill(0)]});
    assert.equal(s.dropped,2);
    assert.throws(() => s.acceptRows(s.ticket,{sequence:3,elapsedMs:2,rows:[Array(16).fill(0)]}));
});
test('DSP results, monitor history and electrode plots update from a decoded observation', () => {
    const s=session('dsp-calibration',a.dspDefaults());
    s.acceptDsp(s.ticket,{sequence:0,band:1,frequencyHz:1000,measuredY:Math.sqrt(100000),measuredSpl:70,
        electrodeRows:[Array(16).fill(0.5)]});
    assert.equal(s.results.length,1); near(s.results[0].coefficient,10);
    assert.equal(s.monitorY.length,1); assert.equal(s.dspRows.length,1);
    assert.throws(() => s.acceptDsp(s.ticket,{sequence:1,band:1,frequencyHz:1000,measuredY:300,measuredSpl:60,electrodeRows:[[NaN]]}));
    assert.equal(s.lastSequence,0);
});

test('DSP control uses applied stimulus instead of measured SPL and isolates per-band history', () => {
    const s=session('dsp-calibration',a.dspDefaults()), target=Math.sqrt(100000);
    s.acceptDsp(s.ticket,{sequence:0,band:1,frequencyHz:1000,measuredY:100,measuredSpl:92,appliedLevelDb:60});
    near(s.observation.decision.appliedLevelDb,60);
    s.acceptDsp(s.ticket,{sequence:1,band:2,frequencyHz:2000,measuredY:1000,measuredSpl:92,appliedLevelDb:65});
    assert.equal(s.observation.decision.strategy,'PID'); assert.equal(s.observation.iteration,1);
    s.acceptDsp(s.ticket,{sequence:2,band:1,frequencyHz:1000,measuredY:1000,measuredSpl:92,appliedLevelDb:65});
    near(s.observation.nextLevelDb,62.5); assert.equal(s.observation.iteration,2);
    s.acceptDsp(s.ticket,{sequence:3,band:1,frequencyHz:1000,measuredY:target,measuredSpl:70,appliedLevelDb:62.5});
    assert.equal(s.observation.nextLevelDb,null); assert.equal(s.results.length,1);
    near(s.results[0].coefficient,10);
    assert.throws(() => s.acceptDsp(s.ticket,{sequence:4,band:1,frequencyHz:1500,measuredY:target,measuredSpl:70}));
});

test('DSP iteration limits and stopping suppress further stimulus requests', () => {
    const s=session('dsp-calibration',{...a.dspDefaults(),maxIterations:1});
    s.acceptDsp(s.ticket,{sequence:0,band:1,frequencyHz:1000,measuredY:0,measuredSpl:60,appliedLevelDb:60});
    assert.equal(s.observation.iterationLimit,true); assert.equal(s.observation.nextLevelDb,null);
    const stopping=session('dsp-calibration',a.dspDefaults()); stopping.stop();
    stopping.acceptDsp(stopping.ticket,{sequence:0,band:1,frequencyHz:1000,measuredY:100,measuredSpl:60,appliedLevelDb:60});
    assert.equal(stopping.observation.nextLevelDb,null); assert.equal(stopping.phase,'stopping');
});
const tone=Array.from({length:1024},(_,i)=>Math.SQRT2*Math.pow(10,-30/20)*Math.sin(2*Math.PI*64*i/1024));
function micBlock(sequence,elapsedMs,channel='Front') {
    return {channel,sequence,elapsedMs,sampleRate:16000,samples:tone};
}
test('microphone analysis measures PCM and never invents SPL calibration', () => {
    const config={...a.micDefaults(),windowMs:20}; const s=session('automatic-mic',config);
    s.acceptMic(s.ticket,micBlock(0,0),0);
    near(s.microphones.Front.stats.dbfs,-30,1e-8); near(s.microphones.Front.spectrum.peakHz,1000);
    assert.equal(s.microphones.Front.stats.spl,null);
    s.acceptMic(s.ticket,{...micBlock(1,100),splOffsetDb:120},100);
    near(s.microphones.Front.stats.spl,90,1e-8);
});
test('target achievement does not stop a microphone session; lost samples reset hold', () => {
    const config={...a.micDefaults(),windowMs:20}; const s=session('automatic-mic',config);
    for(let i=0;i<=20;++i) s.acceptMic(s.ticket,micBlock(i,i*100),i*100);
    assert.equal(s.micQuality().achieved,true); assert.equal(s.phase,'running');
    assert.equal(s.expire(3101),true); assert.equal(s.micQuality().achieved,false);
    s.acceptMic(s.ticket,micBlock(21,3200),3200); assert.equal(s.micQuality().progress,0);
});
test('both-microphone selection requires both streams; AUTO prioritizes Front', () => {
    const config={...a.micDefaults(),microphone:'Both',windowMs:20}; const s=session('automatic-mic',config);
    for(let i=0;i<=20;++i) s.acceptMic(s.ticket,micBlock(i,i*100),i*100);
    assert.equal(s.micQuality().achieved,false); assert.match(s.micQuality().status,/Back/);
    for(let i=0;i<=20;++i) s.acceptMic(s.ticket,micBlock(i,i*100,'Back'),2000);
    assert.equal(s.micQuality().achieved,true);
    s.micConfig.microphone='Auto'; assert.equal(s.selectedMicNames()[0],'Front');
});
test('bad PCM, duplicate blocks and rate/sequence discontinuities are handled', () => {
    const s=session('automatic-mic',{...a.micDefaults(),windowMs:20});
    assert.throws(() => s.acceptMic(s.ticket,{...micBlock(0,0),samples:[2]},0));
    s.acceptMic(s.ticket,micBlock(0,0),0);
    assert.throws(() => s.acceptMic(s.ticket,micBlock(0,100),100));
    s.acceptMic(s.ticket,micBlock(4,200),200);
    assert.equal(s.dropped,1); assert.equal(s.micQuality().progress,0);
    assert.equal(s.microphones.Front.history.length,3);
    assert.equal(s.microphones.Front.history[1],null); // Visible gap, not a continuous RMS curve.
});
test('tone validation blocks a false quality pass', () => {
    const s=session('automatic-mic',{...a.micDefaults(),windowMs:20,validateTone:true,expectedHz:2000});
    for(let i=0;i<=20;++i) s.acceptMic(s.ticket,micBlock(i,i*100),i*100);
    assert.equal(s.micQuality().achieved,false); assert.match(s.microphones.Front.quality,/tone/);
});
test('switching tools preserves completed results from the other tool', () => {
    const s=session('dsp-calibration',a.dspDefaults());
    s.acceptDsp(s.ticket,{sequence:0,band:1,frequencyHz:1000,measuredY:Math.sqrt(100000),measuredSpl:60});
    s.stopped(s.ticket); s.availableModes=['automatic-mic'];
    s.start('automatic-mic',a.micDefaults()); assert.equal(s.results.length,1);
});
process.stdout.write(`${checks} calculation/session checks passed; no Qt runtime used.\n`);
