const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {pathToFileURL} = require('node:url');
const root = path.resolve(__dirname, '..');
const registryPath = path.join(root, 'src/tools/ToolRegistry.qml');
const source = fs.readFileSync(registryPath, 'utf8');
const records = [...source.matchAll(/ListElement\s*\{([^}]+)\}/g)].map(match =>
    Object.fromEntries([...match[1].matchAll(/(\w+)\s*:\s*(?:qsTr\()?\s*("(?:\\.|[^"\\])*")\)?/g)]
        .map(role => [role[1], JSON.parse(role[2])])));
const context = vm.createContext({
    count: records.length, get: index => records[index],
    Qt: {resolvedUrl: relative => new URL(relative, pathToFileURL(registryPath)).href}
});
// Exercise the actual registry functions, independently of Qt's ListModel.
vm.runInContext(source.slice(source.indexOf('    function '), source.indexOf('    ListElement')), context);
const ids = entries => Array.from(entries, entry => entry.toolId);
let checks = 0;
function test(name, action) { action(); ++checks; process.stdout.write(`PASS ${name}\n`); }

test('registry IDs are unique and diagnostic entries have an entry point', () => {
    assert.equal(new Set(records.map(tool => tool.toolId)).size, records.length);
    for (const tool of records) {
        assert.match(tool.toolId, /^[a-z][a-z0-9-]+$/);
        for (const role of ['title','summary','category','iconKind','accent']) assert.ok(tool[role]);
        if (tool.category === 'diagnostic') assert.ok(tool.entryPoint.endsWith('.qml'));
        else assert.equal(tool.category, 'utility');
    }
});
test('launcher searches only diagnostics and ignores case and surrounding spaces', () => {
    assert.deepEqual(ids(context.entries(true, '')), records.filter(tool => tool.category === 'diagnostic').map(tool => tool.toolId));
    const microphoneTools = context.entries(true, ' MICROPHONE ');
    assert.ok(ids(microphoneTools).includes('acoustic-analytics'));
    assert.ok(microphoneTools.every(tool => tool.category === 'diagnostic' && (tool.title+' '+tool.summary).toLowerCase().includes('microphone')));
    assert.equal(context.entries(true, 'notepad').length, 0);
});
test('dock includes utilities regardless of the launcher search', () => {
    assert.deepEqual(ids(context.entries(false, 'no matches')), records.map(tool => tool.toolId));
});
test('entry URLs resolve relative to the registry, not the shell or current directory', () => {
    for (const tool of records.filter(tool => tool.category === 'diagnostic')) {
        const entry = context.findTool(tool.toolId);
        assert.equal(entry.entryPoint, pathToFileURL(path.join(path.dirname(registryPath), tool.entryPoint)).href);
        assert.ok(fs.existsSync(new URL(entry.entryPoint)));
    }
    assert.equal(context.findTool('unknown-tool'), null);
});
test('a future diagnostic is found and routed by metadata without a new switch case', () => {
    records.push({toolId:'future-diagnostic',title:'Future Diagnostic',summary:'New analysis',
        category:'diagnostic',iconKind:'acoustic',accent:'#74caff',entryPoint:'diagnostics/future/qml/FutureDiagnostic.qml'});
    context.count = records.length;
    assert.equal(context.entries(true, 'future')[0].toolId, 'future-diagnostic');
    assert.ok(context.findTool('future-diagnostic').entryPoint.endsWith('/FutureDiagnostic.qml'));
    records.pop(); context.count = records.length;
});

const hostSource = fs.readFileSync(path.join(root, 'src/shell/qml/DiagnosticToolHost.qml'), 'utf8');
const loadFunctions = hostSource.slice(hostSource.indexOf('            function ensureLoaded'),hostSource.indexOf('            Component.onCompleted'));
function makeLoader() {
    const calls = [];
    const probes = [], warnings = [];
    const probe = {status:3,errorString:()=> 'qrc:/future/FutureDiagnostic.qml:12: Type Example unavailable',
        destroyed:false,destroy(){this.destroyed=true},statusChanged:{connect(callback){probe.changed=callback}}};
    const loader = vm.createContext({
        opened:false, status:0, source:'', Loader:{Error:3},
        errorProbe:null,loadError:'',detailsVisible:false,
        Component:{PreferSynchronous:0,Ready:1,Loading:2,Error:3},qsTr:value=>value,
        console:{warn:message=>warnings.push(message)},
        Qt:{createComponent:(url,mode,parent)=>{probes.push({url,mode,parent});return probe}},
        modelData:{toolId:'future-diagnostic', entryPoint:'qrc:/future/FutureDiagnostic.qml'},
        host:{toolId:'',session:{mode:'idle'},uiScale:0.8},
        setSource:(url, properties) => calls.push({url,properties})
    });
    loader.toolLoader=loader;
    vm.runInContext(loadFunctions, loader);
    return {loader,calls,probe,probes,warnings};
}
test('tool loading is deferred and receives the shared session and display scale', () => {
    const {loader,calls} = makeLoader();
    loader.ensureLoaded(); assert.equal(calls.length, 0);
    loader.host.toolId = 'future-diagnostic'; loader.ensureLoaded();
    assert.equal(calls.length, 1);
    assert.equal(calls[0].properties.session, loader.host.session);
    assert.equal(calls[0].properties.uiScale, 0.8);
});
test('returning to an opened tool does not recreate it or discard state', () => {
    const {loader,calls} = makeLoader();
    loader.host.toolId = 'future-diagnostic'; loader.ensureLoaded();
    loader.host.toolId = ''; loader.ensureLoaded();
    loader.host.toolId = 'future-diagnostic'; loader.ensureLoaded();
    assert.equal(calls.length, 1);
});
test('a failed tool can be retried on the next visit', () => {
    const {loader,calls} = makeLoader();
    loader.host.toolId = 'future-diagnostic'; loader.ensureLoaded();
    loader.status = loader.Loader.Error; loader.source = calls[0].url;
    loader.ensureLoaded();
    assert.equal(calls.length, 2); assert.equal(loader.source, '');
});
test('component failures expose the Qt file/line error without creating a tool instance', () => {
    const {loader,probes,warnings}=makeLoader();
    loader.status=loader.Loader.Error; loader.diagnoseLoadError();
    assert.equal(probes.length,1); assert.equal(probes[0].url,loader.modelData.entryPoint);
    assert.match(loader.loadError,/FutureDiagnostic.qml:12: Type Example unavailable/);
    assert.match(warnings[0],/Tool load failed/);
    loader.diagnoseLoadError(); assert.equal(probes.length,1);
    // The fake probe deliberately has no createObject(): inspecting the error
    // must never instantiate a second copy of a diagnostic tool.
});
test('an asynchronous error probe publishes details when it finishes', () => {
    const {loader,probe}=makeLoader(); probe.status=loader.Component.Loading;
    loader.status=loader.Loader.Error; loader.diagnoseLoadError();
    assert.match(loader.loadError,/Collecting/);
    probe.status=loader.Component.Error; probe.changed();
    assert.match(loader.loadError,/qml:12/);
});
test('instance-creation failures with no component error direct users to Application Output', () => {
    const {loader,probe}=makeLoader(); probe.status=loader.Component.Ready; probe.errorString=()=>'';
    loader.status=loader.Loader.Error; loader.diagnoseLoadError();
    assert.match(loader.loadError,/Application Output/);
});
test('retry releases the prior probe and clears old error text and expanded details', () => {
    const {loader,probe}=makeLoader(); loader.status=loader.Loader.Error; loader.diagnoseLoadError();
    loader.detailsVisible=true; loader.clearLoadError();
    assert.equal(probe.destroyed,true); assert.equal(loader.errorProbe,null);
    assert.equal(loader.loadError,''); assert.equal(loader.detailsVisible,false);
});
test('successful or unopened tools do not create diagnostic error probes', () => {
    const {loader,probes}=makeLoader(); loader.diagnoseLoadError(); assert.equal(probes.length,0);
    loader.status=1; loader.diagnoseLoadError(); assert.equal(probes.length,0);
});
test('application and shell do not route by built-in diagnostic IDs or screen types', () => {
    for (const relative of ['src/app/qml/Main.qml','src/shell/qml/DiagnosticWorkspace.qml','src/shell/qml/DiagnosticToolHost.qml']) {
        const text = fs.readFileSync(path.join(root, relative), 'utf8');
        assert.doesNotMatch(text, /live-electrodogram|acoustic-analytics|DspCalibration|LiveElectrodogram|AutomaticMicCalibration/);
    }
});
process.stdout.write(`${checks} tool registry/lifecycle checks passed; no Qt runtime used.\n`);
