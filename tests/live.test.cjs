const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
const livePath = 'src/tools/diagnostics/live_electrodogram/qml/';
const read = relative => fs.readFileSync(path.join(root, relative), 'utf8').replace(/^\uFEFF/, '');
function load(relative, globals={}) {
    const context = vm.createContext(globals);
    vm.runInContext(read(relative).replace(/^\.(pragma|import).*$/gm, ''), context);
    return context;
}
const live = load(livePath+'LiveModel.js');
const analysis = load('src/services/diagnostics/qml/Analysis.js');
const core = load('src/services/diagnostics/qml/SessionModel.js', {Analysis:analysis});
const plain = value => JSON.parse(JSON.stringify(value));
let checks = 0;
function test(name, action) { action(); ++checks; process.stdout.write(`PASS ${name}\n`); }
function session(settings=live.captureSettings(live.defaults())) {
    const model = new core.Session(); model.availableModes=['live-electrodogram','dsp-calibration'];
    model.start('live-electrodogram',settings); model.started(model.ticket); return model;
}
const rows = count => Array.from({length:count}, (_, i) => Array(16).fill(i/Math.max(1,count)));

test('all defaults and choice lists match LiveWaveformPanel settings', () => {
    assert.deepEqual(plain(live.defaults()), {
        mcl:4095,thl:1,pulseWidth:40,gain:1,pps:500,tolerance:20,ifg:3,ipg:8,
        biPolar:false,baseToApex:false,plotSource:'STIM_EVALUE',selectionMode:'all',
        electrodes:Array(16).fill(true),resistances:Array(16).fill(2000)
    });
    assert.deepEqual(plain(live.pulseWidths),[8,10,20,40,60,80,100,200]);
    assert.deepEqual(plain(live.gains),[0.25,0.5,0.75,1,1.5,2,3]);
    assert.deepEqual(plain(live.rates),[250,500,1000,2000]);
    assert.deepEqual(plain(live.tolerances),[10,20,30,40,50]);
});
test('invalid reference parameters are rejected, including fractional and non-finite values', () => {
    for (const [key,value] of [['mcl',4096],['mcl',0],['thl',1.5],['gain',4],['pulseWidth',30],
        ['pps',750],['tolerance',0],['ifg',-1],['ipg',100],['biPolar',1],['baseToApex',null],['plotSource','ADC'],['selectionMode','none']]) {
        assert.throws(() => live.validate({...live.defaults(),[key]:value}),key);
    }
    assert.throws(() => live.validate({...live.defaults(),mcl:10,thl:11}));
    assert.equal(live.validate({...live.defaults(),mcl:1,thl:1}),true);
    for (const value of [0,20001,2.5,NaN,Infinity]) {
        const s=live.defaults(); s.resistances[15]=value; assert.throws(() => live.validate(s));
    }
    assert.throws(() => live.validate({...live.defaults(),electrodes:[true]}));
    assert.throws(() => live.validate({...live.defaults(),resistances:[2000]}));
});
test('Enable All preserves a saved custom mask', () => {
    const s=live.defaults(); s.electrodes[0]=false;
    assert.equal(live.enabled(s)[0],true);
    s.selectionMode='custom'; assert.equal(live.enabled(s)[0],false);
    const result=live.enabled(s); result[1]=false; assert.equal(s.electrodes[1],true);
});
test('settings edits use an isolated draft and cancelling discards resistance and mask changes', () => {
    const source=read(livePath+'LiveSettingsPanel.qml');
    const panel=vm.createContext({Live:live,scroll:{contentY:42}});
    vm.runInContext(source.slice(source.indexOf('    function begin'),source.indexOf('    Keys.onEscapePressed')),panel);
    const saved=live.defaults(); panel.begin(saved);
    panel.changeResistance(0,3100); panel.changeElectrode(1,false); panel.change('mcl',2000);
    assert.equal(saved.resistances[0],2000); assert.equal(saved.electrodes[1],true); assert.equal(saved.mcl,4095);
    panel.begin(saved); // Cancel and reopen: all fields restore from committed settings.
    assert.deepEqual(plain(panel.draft),plain(saved)); assert.equal(panel.scroll.contentY,0);
});
test('capture request is an immutable configuration without display-only channel filtering', () => {
    const settings=live.defaults(); settings.selectionMode='custom'; settings.electrodes.fill(false);
    const request=live.captureSettings(settings); const s=session(request);
    assert.equal(request.profile,'live-electrodogram-v1'); assert.equal(request.electrodes,undefined);
    assert.equal(request.selectionMode,undefined); assert.equal(request.resistances.length,16);
    settings.resistances[0]=1234; request.resistances[1]=3456;
    assert.equal(s.liveConfig.resistances[0],2000); assert.equal(s.liveConfig.resistances[1],2000);
});
test('plot order is EL16 at the top and EL1 at the bottom; identity colors survive filtering', () => {
    const data=[Array.from({length:16},(_,i)=>i/16)]; const mask=Array(16).fill(true);
    const all=live.series(data,mask,0,1000);
    assert.equal(all[0].label,'EL16'); assert.equal(all[15].label,'EL1');
    assert.equal(all[0].values[0],15/16); assert.equal(all[15].values[0],0);
    mask[15]=false; mask[12]=false;
    const filtered=live.series(data,mask,0,1000);
    for (const series of filtered) assert.equal(series.color,all.find(item=>item.label===series.label).color);
    assert.notEqual(all[0].color,all[1].color); assert.equal(all[0].color,all[2].color);
});
test('plot preparation preserves nulls and signed data without inventing E-value conversion', () => {
    const data=rows(3); data[1][0]=null; data[2][0]=-0.75;
    const s=live.series(data,Array(16).fill(true),1,2);
    assert.deepEqual(plain(s[15].values),[null,-0.75]);
    assert.equal(live.series(data,Array(16).fill(false),0,1000).length,0);
    assert.equal(live.series([],Array(16).fill(true),0,1000).length,16);
});
test('zoom follows reference increments and clamps to 10–8000 points', () => {
    assert.equal(live.zoom(1000,-1),800); assert.equal(live.zoom(1000,1),1200);
    assert.equal(live.zoom(100,-1),50); assert.equal(live.zoom(50,-1),10);
    assert.equal(live.zoom(7990,1),8000); assert.equal(live.points(1234),1230);
});
test('history stays on the same accepted rows as new data arrives', () => {
    const first=live.windowFor(3000,3000,1000,false,500);
    const next=live.windowFor(3500,3500,1000,false,first.start);
    assert.equal(first.start,500); assert.equal(next.start,500); assert.equal(next.offset,500);
    assert.equal(live.windowFor(3500,3500,1000,true,0).start,2500);
});
test('ring-buffer rollover clamps expired history and retains unexpired row identity', () => {
    const retained=live.windowFor(8500,8000,1000,false,1000);
    assert.deepEqual(plain(retained),{oldest:500,latest:7500,start:1000,offset:500,count:1000});
    assert.equal(live.windowFor(9500,8000,1000,false,1000).start,1500);
    const empty=live.windowFor(0,0,1000,true,0); assert.equal(empty.count,0); assert.equal(empty.start,0);
});
test('pause keeps both samples and x-axis origin frozen when the incoming ring wraps', () => {
    const s=session(); s.acceptRows(s.ticket,{sequence:0,elapsedMs:1,rows:rows(8000)});
    s.pause(true); const before=s.displayRows;
    s.acceptRows(s.ticket,{sequence:8000,elapsedMs:2,rows:rows(500)});
    assert.equal(s.displayRows,before); assert.equal(s.displayTotal,8000); assert.equal(s.rows.total,8500);
    s.pause(false); assert.equal(s.displayTotal,8500); assert.equal(s.displayRows.length,8000);
    assert.equal(live.windowFor(s.displayTotal,s.displayRows.length,1000,true,0).start,7500);
});
test('wrong-source batches are rejected atomically before data and counters change', () => {
    const s=session();
    assert.throws(() => s.acceptRows(s.ticket,{sequence:0,elapsedMs:1,plotSource:'ASK_EVALUE',rows:rows(2)}),/source/);
    assert.equal(s.rows.total,0); assert.equal(s.lastSequence,-1); assert.equal(s.liveLastElapsed,-1);
    assert.equal(s.acceptRows(s.ticket,{sequence:0,elapsedMs:1,plotSource:'STIM_EVALUE',rows:rows(2)}),true);
    assert.equal(s.acceptRows(s.ticket,{sequence:2,elapsedMs:2,rows:rows(1)}),true); // Source is implicit in accepted ticket.
});
test('live snapshot metadata survives a different tool and resets for a new live run', () => {
    const s=session(); const generation=s.liveGeneration;
    s.acceptRows(s.ticket,{sequence:0,elapsedMs:10,rows:rows(2)}); s.stopped(s.ticket);
    s.start('dsp-calibration',analysis.dspDefaults());
    assert.equal(s.liveGeneration,generation); assert.equal(s.displayTotal,2); assert.equal(s.liveLastElapsed,10);
    assert.equal(s.liveConfig.plotSource,'STIM_EVALUE'); s.stopped(s.ticket);
    s.start('live-electrodogram',live.captureSettings({...live.defaults(),plotSource:'ASK_EVALUE'}));
    assert.ok(s.liveGeneration>generation); assert.equal(s.displayTotal,0); assert.equal(s.liveLastElapsed,-1);
});

function viewContext() {
    const context=vm.createContext({Live:live,qsTr:s=>s,settings:live.defaults(),settingsOpen:false,
        following:true,historyStart:0,visiblePoints:1000,discardedGeneration:-1,
        session:{busy:false,liveGeneration:4,feedback:'',start:(mode,request)=>{context.request={mode,request}}}});
    Object.defineProperty(context,'viewport',{get:()=>live.windowFor(3000,3000,context.visiblePoints,context.following,context.historyStart)});
    const source=read(livePath+'LiveElectrodogram.qml');
    vm.runInContext(source.slice(source.indexOf('    function changeSource'),source.indexOf('    Keys.onEscapePressed')),context);
    return context;
}
test('source change invalidates the prior plot and is locked during capture or draft editing', () => {
    const view=viewContext(); view.changeSource('ASK_EVALUE');
    assert.equal(view.discardedGeneration,4); assert.equal(view.settings.plotSource,'ASK_EVALUE');
    view.session.busy=true; view.changeSource('STIM_EVALUE'); assert.equal(view.settings.plotSource,'ASK_EVALUE');
    view.session.busy=false; view.settingsOpen=true; view.changeSource('STIM_EVALUE'); assert.equal(view.settings.plotSource,'ASK_EVALUE');
});
test('active capture allows visibility edits but refuses a changed acquisition request', () => {
    const view=viewContext(); view.session.busy=true;
    const changed=live.defaults(); changed.gain=2; view.applySettings(changed);
    assert.equal(view.settings.gain,1); assert.match(view.session.feedback,/Stop capture/);
    const display=live.defaults(); display.selectionMode='custom'; display.electrodes[2]=false;
    view.applySettings(display); assert.equal(view.settings.electrodes[2],false);
});
test('history controls resume following only at the newest window; start uses saved settings', () => {
    const view=viewContext(); view.seek(500); assert.equal(view.following,false); assert.equal(view.viewport.start,500);
    view.setPoints(1500); assert.equal(view.viewport.start,500);
    view.seek(9000); assert.equal(view.following,true); assert.equal(view.viewport.start,1500);
    view.startCapture(); assert.equal(view.request.mode,'live-electrodogram'); assert.equal(view.request.request.pps,500);
});

function historyKeyHandler(view) {
    const source=read(livePath+'LiveElectrodogram.qml');
    assert.doesNotMatch(source,/Keys\.on(?:Home|End)Pressed\b/,
        'Home and End have no dedicated Qt Quick Keys signals');
    const binding='Keys.onPressed:';
    const start=source.indexOf(binding);
    const end=source.indexOf('\n                Rectangle {',start);
    assert.ok(start>=0 && end>start,'history must handle Home/End through Keys.onPressed');
    return vm.runInNewContext('('+source.slice(start+binding.length,end)+')',
        {view,Qt:{Key_Home:0x01000010,Key_End:0x01000011}});
}

test('Home seeks the oldest retained history and End resumes following', () => {
    const view=viewContext(); const press=historyKeyHandler(view);
    view.seek(500);
    const home={key:0x01000010,accepted:false}; press(home);
    assert.equal(home.accepted,true); assert.equal(view.following,false);
    assert.equal(view.viewport.start,view.viewport.oldest);
    const end={key:0x01000011,accepted:false}; press(end);
    assert.equal(end.accepted,true); assert.equal(view.following,true);
    assert.equal(view.viewport.start,view.viewport.latest);
});

test('unhandled history keys propagate without changing the selected window', () => {
    const view=viewContext(); view.seek(500);
    const event={key:0x01000000,accepted:true}; // Escape remains available to the workspace.
    historyKeyHandler(view)(event);
    assert.equal(event.accepted,false); assert.equal(view.following,false);
    assert.equal(view.viewport.start,500);
});
process.stdout.write(`${checks} live graph/settings checks passed; no Qt runtime used.\n`);
