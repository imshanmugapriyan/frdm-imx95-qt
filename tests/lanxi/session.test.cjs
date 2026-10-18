const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const analysis = vm.createContext({});
vm.runInContext(read('src/services/diagnostics/qml/Analysis.js').replace(/^\.pragma library\s*/, ''), analysis);
const core = vm.createContext({Analysis: analysis});
vm.runInContext(read('src/services/diagnostics/qml/SessionModel.js').replace(/^\.(pragma|import).*$/gm, ''), core);
const source = read('src/services/diagnostics/qml/DiagnosticSession.qml');
function makeSession() {
    const calls = [];
    const context = vm.createContext({
        model: new core.Session(), backendModes: {m7:[], lanxi:[]}, revision:0, feedback:'',
        commandStartedAt:0, recording:false, recordingError:'', qsTr: text => text, Analysis:analysis,
        files:{monotonicMs:()=>100,finishRecording:()=>true},
        lanxiBackend:{busy:false,start:(ticket,settings)=>calls.push(['lanxi',ticket,settings]),stop:()=>calls.push(['lanxi-stop'])},
        startRequested:(ticket,mode)=>calls.push(['m7',ticket,mode]),stopRequested:ticket=>calls.push(['m7-stop',ticket])
    });
    context.session=context;
    Object.defineProperties(context,{
        busy:{get:()=>context.model.busy()||context.lanxiBackend.busy},
        phase:{get:()=>context.model.phase},mode:{get:()=>context.model.mode}
    });
    vm.runInContext(source.slice(source.indexOf('    function available'),source.lastIndexOf('}')),context);
    return {s:context,calls};
}
let count=0;
function test(name,run){run();++count;console.log('PASS '+name);}
test('LAN-XI and M7 capability reports do not erase each other',()=>{
    const {s}=makeSession();s.setAvailableModes(['live-electrodogram']);s.setBackendModes('lanxi',['lanxi-measurement']);
    assert(s.available('live-electrodogram')&&s.available('lanxi-measurement'));
    s.setAvailableModes([]);assert(s.available('lanxi-measurement'));
    s.setAvailableModes(['automatic-mic']);s.setBackendModes('lanxi',[]);assert(s.available('automatic-mic'));
});
test('paired LAN-XI start routes to the native backend and freezes its settings',()=>{
    const {s,calls}=makeSession();s.setBackendModes('lanxi',['lanxi-measurement']);
    const settings={devices:[{reference:1},{reference:2}]};s.start('lanxi-measurement',settings);settings.devices[0].reference=4;
    assert.equal(calls.length,1);assert.equal(calls[0][0],'lanxi');assert.equal(calls[0][2].devices[0].reference,1);assert(s.busy);
});
test('inspection and replay ownership prevent M7 capture',()=>{
    const {s,calls}=makeSession();s.setAvailableModes(['live-electrodogram']);s.lanxiBackend.busy=true;s.start('live-electrodogram',{});
    assert.equal(calls.length,0);assert.match(s.feedback,/LAN-XI/);assert.equal(s.model.phase,'idle');
});
test('a pending paired stop retains ownership until the matching acknowledgement',()=>{
    const {s,calls}=makeSession();s.setBackendModes('lanxi',['lanxi-measurement']);s.start('lanxi-measurement',{});
    const ticket=s.model.ticket;s.lanxiBackend.busy=true;s.started(ticket);s.finish();
    assert.equal(s.phase,'stopping');assert.equal(calls.at(-1)[0],'lanxi-stop');
    s.stopped(ticket-1);assert(s.busy);s.lanxiBackend.busy=false;s.stopped(ticket);assert(!s.busy);
});
test('capability loss does not prematurely release a paired recovery',()=>{
    const {s}=makeSession();s.setBackendModes('lanxi',['lanxi-measurement']);s.start('lanxi-measurement',{});
    s.lanxiBackend.busy=true;s.setBackendModes('lanxi',[]);assert(s.busy);assert.equal(s.phase,'starting');
});
test('LAN-XI capture preserves existing microphone and DSP results',()=>{
    const {s}=makeSession();s.model.microphones={Front:{total:123}};s.model.results=[{band:2}];
    s.setBackendModes('lanxi',['lanxi-measurement']);s.start('lanxi-measurement',{});
    assert.equal(s.model.microphones.Front.total,123);assert.equal(s.model.results[0].band,2);
});
console.log(`${count} LAN-XI shared-session checks passed.`);
