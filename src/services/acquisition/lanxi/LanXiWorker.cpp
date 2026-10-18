#include "LanXiWorker.h"
#include "LanXiBackend.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
QJsonObject outputSelection() { return {{"outputs",QJsonArray{QJsonObject{{"number",1}}}}}; }
QVariantMap deviceConfig(const QVariantMap &s,int d) { return s.value("devices").toList().at(d).toMap(); }
QString address(const QVariantMap &s,int d) { return deviceConfig(s,d).value("url").toString().trimmed(); }
QString serialOf(const QJsonObject &info) { return info["module"].toObject()["serial"].toVariant().toString(); }
QString typeOf(const QJsonObject &info) { return info["module"].toObject()["type"].toObject()["number"].toString(); }
bool finiteRange(double value,double low,double high) { return std::isfinite(value) && value>=low && value<=high; }
bool hasProtocolError(const QJsonValue &value) {
    if(value.isObject()) {
        const auto object=value.toObject();
        for(auto it=object.begin();it!=object.end();++it) {
            if(it.key()=="errorcode" && (!it.value().isDouble() || it.value().toDouble()!=0))return true;
            if(hasProtocolError(it.value()))return true;
        }
    }else if(value.isArray())for(const auto &item:value.toArray())if(hasProtocolError(item))return true;
    return false;
}
}
LanXiWorker::LanXiWorker(LanXiBackend *owner) : m_owner(owner) {}
void LanXiWorker::initialize() {
    m_network=new QNetworkAccessManager(this); m_network->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    m_timer=new QTimer(this); m_timer->setInterval(100);
    connect(m_timer,&QTimer::timeout,this,[this]{tick();}); m_timer->start(); m_clock.start();
    m_root=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/SunoDiagnostics/LanXi";
    const QString dataOverride=qEnvironmentVariable("SUNO_LANXI_DATA_DIR");
    if(!dataOverride.isEmpty() && QDir::isAbsolutePath(dataOverride))m_root=QDir::cleanPath(dataOverride);
    QDir().mkpath(m_root+"/Profiles"); QDir().mkpath(m_root+"/Runs");
    try {m_workspace=readJson(m_root+"/workspace.json").toVariantMap();}catch(const std::exception &){}
    resetProfile("acoustic"); refreshRuns();refreshSensors();
}
void LanXiWorker::shutdown() {
    ++m_generation;
    for (auto &d:m_devices) { if (d.reply) d.reply->abort(); if (d.socket) d.socket->abort(); if (d.raw.isOpen()) d.raw.close(); }
    for(auto &reply:m_discoveryReplies)if(reply)reply->abort();
    if (m_busy && !m_runPath.isEmpty() && !m_review && (m_phase=="starting"||m_phase=="running"||m_phase=="stopping"||m_phase=="recovery")) {
        m_stopping=true; writeManifest("interrupted");
    }
    // Normal application close is prevented by DiagnosticSession while busy.
    // Process termination cannot prove that a remote generator is muted.
    if (m_timer) m_timer->stop();
}
void LanXiWorker::request(int index,const QByteArray &method,const QString &path,const QJsonValue &body,Done done) {
    auto &d=m_devices[index]; const auto generation=m_generation;
    QNetworkRequest request(QUrl(d.url+"/rest/rec"+path));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    QByteArray payload;
    if (body.isObject()) payload=QJsonDocument(body.toObject()).toJson(QJsonDocument::Compact);
    else if (body.isArray()) payload=QJsonDocument(body.toArray()).toJson(QJsonDocument::Compact);
    auto *reply=m_network->sendCustomRequest(request,method,payload); d.reply=reply;
    auto bytes=std::make_shared<QByteArray>(); auto tooLarge=std::make_shared<bool>(false);
    connect(reply,&QIODevice::readyRead,this,[reply,bytes,tooLarge]{
        if (bytes->size()+reply->bytesAvailable()>2*1024*1024) { *tooLarge=true; reply->abort(); return; }
        bytes->append(reply->readAll());
    });
    QPointer<QNetworkReply> guard(reply);
    QTimer::singleShot(6000,this,[guard]{if (guard && !guard->isFinished()) guard->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,index,reply,bytes,tooLarge,generation,path,done=std::move(done)]{
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->bytesAvailable()+bytes->size()<=2*1024*1024) bytes->append(reply->readAll()); else *tooLarge=true;
        const QString transport=reply->errorString(); const auto networkError=reply->error();
        reply->deleteLater(); if (m_devices[index].reply==reply) m_devices[index].reply=nullptr;
        if (generation!=m_generation) return;
        if (*tooLarge || networkError!=QNetworkReply::NoError || status<200 || status>=300) {
            done(false,{},QString("%1 %2: HTTP %3; %4 %5").arg(m_devices[index].label,path).arg(status)
                 .arg(*tooLarge ? "Response exceeded size limit." : transport,QString::fromUtf8(bytes->left(600)))); return;
        }
        QJsonValue value;
        if (!bytes->trimmed().isEmpty()) {
            QJsonParseError error; const auto doc=QJsonDocument::fromJson(*bytes,&error);
            if (error.error!=QJsonParseError::NoError || doc.isNull()) { done(false,{},"Invalid JSON response from "+path); return; }
            value=doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
        }
        try {
            if(hasProtocolError(value))done(false,value,"Module reported a nonzero errorcode for "+path+": "+QString::fromUtf8(bytes->left(600)));
            else done(true,value,{});
        }
        catch(const std::exception &e) { fail(QString::fromUtf8(e.what())); }
    });
}
bool LanXiWorker::validate(const QVariantMap &s,QString &error) const {
    const auto reject=[&](const QString &why){error=why; return false;};
    const auto devices=s.value("devices").toList(); const auto workflow=s.value("workflow").toString();
    if (s.value("schema").toInt()!=1 || devices.size()!=2 || !QStringList{"acoustic","microphone","amplifier"}.contains(workflow)) return reject("Invalid LAN-XI profile version or workflow.");
    for(int d=0;d<2;++d) {
        const auto settings=devices[d].toMap(); const QUrl url(settings.value("url").toString());
        if (!url.isValid() || url.scheme()!="http" || url.host().isEmpty() || !url.userInfo().isEmpty() || !url.query().isEmpty()
            || !url.fragment().isEmpty() || (!url.path().isEmpty() && url.path()!="/")) return reject("Use a module address such as http://192.168.1.101 (no path or credentials).");
        const int reference=settings.value("reference").toInt(); const auto channels=settings.value("channels").toList();
        const int rate=settings.value("sampleRate").toInt();
        if (channels.size()!=4 || reference<0 || reference>4 || rate<128 || rate>65536) return reject("Each device requires four channel settings and a supported sample rate.");
        bool any=false;
        for(int c=0;c<4;++c) {
            const auto ch=channels[c].toMap(); any|=ch.value("enabled").toBool();
            if (ch.value("channel").toInt()!=c+1 || !finiteRange(ch.value("sensitivity").toDouble(),1e-12,1e9)
                || !QStringList{"V","Pa","N","m/s2","m/s","m"}.contains(ch.value("unit").toString())) return reject("Use V, Pa, N, m/s2, m/s or m with a positive sensitivity in V/unit.");
            if (reference==c+1 && !ch.value("enabled").toBool()) return reject("The selected reference channel is disabled.");
            if (workflow=="amplifier" && (ch.value("ccld").toBool() || ch.value("polarization").toBool() || ch.value("unit").toString()!="V"))
                return reject("Amplifier inputs require voltage units with sensor powering disabled.");
        }
        if (!any) return reject("Enable at least one input on each module.");
    }
    if (QUrl(address(s,0))==QUrl(address(s,1))) return reject("Select two different modules.");
    const int fft=s.value("fftSize").toInt();
    if (fft<256 || fft>32768 || (fft&(fft-1)) || !finiteRange(s.value("referenceFloor").toDouble(),1e-12,1e6)) return reject("Invalid FFT size or reference floor.");
    if (!finiteRange(s.value("durationSeconds").toDouble(),0,86400)) return reject("Duration must be 0 (manual stop) to 86400 seconds.");
    const auto a=s.value("analysis").toMap();
    if(!a.isEmpty()) {
        if(!QStringList{"hann","hamming","blackman","flat-top","rectangular"}.contains(a.value("window").toString())
            ||!QStringList{"linear","exponential"}.contains(a.value("averaging").toString())
            ||!QStringList{"Z","A","C"}.contains(a.value("weighting").toString())
            ||!QStringList{"Fast","Slow"}.contains(a.value("timeWeighting").toString())
            ||!finiteRange(a.value("averages").toDouble(),2,10000)
            ||a.value("averages").toDouble()!=a.value("averages").toInt()
            ||(a.value("fraction").toInt()!=1&&a.value("fraction").toInt()!=3)
            ||!finiteRange(a.value("toneHz").toDouble(),1,25600)
            ||!finiteRange(a.value("harmonics").toDouble(),2,10)
            ||a.value("harmonics").toDouble()!=a.value("harmonics").toInt())return reject("Invalid analysis settings.");
        for(int d=0;d<2;++d) {
            const double rate=devices[d].toMap().value("sampleRate").toDouble();
            const double hp=a.value("highPassHz").toDouble(),lp=a.value("lowPassHz").toDouble();
            if(!finiteRange(hp,0,rate/2.56)||!finiteRange(lp,0,rate/2.56)||(hp>0&&lp>0&&hp>=lp))return reject("Analysis filters must fit both sample rates, with high-pass below low-pass.");
            if(a.value("weighting").toString()!="Z"&&rate<=2000)return reject("A/C weighting requires a sample rate above 2 kHz.");
        }
    }
    const auto t=s.value("trigger").toMap();
    if(t.value("enabled").toBool()) {
        const int device=t.value("device").toInt(),channel=t.value("channel").toInt();
        if(device<0||device>1||channel<1||channel>4||!devices[device].toMap().value("channels").toList()[channel-1].toMap().value("enabled").toBool())return reject("Select an enabled trigger channel.");
        try {lanxi::TriggerCapture check;check.configure({true,t.value("rising").toBool(),t.value("level").toDouble(),t.value("hysteresis").toDouble(),
                t.value("preSeconds").toDouble(),t.value("postSeconds").toDouble()},devices[device].toMap().value("sampleRate").toDouble());}
        catch(const std::exception &e){return reject(QString::fromUtf8(e.what()));}
    }
    const auto g=s.value("generator").toMap(); const auto mode=g.value("mode").toString();
    if (!QStringList{"off","sine","steps","noise"}.contains(mode)) return reject("Unknown stimulus mode.");
    if (mode!="off") {
        if (!g.value("armed").toBool()) return reject("Confirm the stimulus wiring and limits before starting output.");
        const double level=g.value("levelVp").toDouble(), limit=g.value("limitVp").toDouble();
        if (!finiteRange(level,0.00001,9.99999) || !finiteRange(limit,0.00001,9.99999) || level>limit) return reject("Output amplitude exceeds the configured limit (maximum 9.99999 Vpeak).");
        const double maxHz=std::min(deviceConfig(s,0).value("sampleRate").toDouble(),deviceConfig(s,1).value("sampleRate").toDouble())/2.56;
        if ((mode=="sine" && !finiteRange(g.value("frequency").toDouble(),20,maxHz))
            || (mode!="sine" && (!finiteRange(g.value("startHz").toDouble(),20,maxHz)
            || !finiteRange(g.value("stopHz").toDouble(),20,maxHz) || g.value("startHz").toDouble()>=g.value("stopHz").toDouble())))
            return reject("Stimulus frequencies must fit both modules' configured measurement bandwidth.");
        if (!finiteRange(g.value("rampSeconds").toDouble(),0.05,5)
            || (mode=="steps" && (!finiteRange(g.value("settleSeconds").toDouble(),0.1,30) || !finiteRange(g.value("dwellSeconds").toDouble(),0.1,30)
            || !finiteRange(g.value("steps").toDouble(),2,256) || g.value("steps").toDouble()!=g.value("steps").toInt())))
            return reject("Invalid stimulus settling, dwell, ramp or step count.");
        for(int d=0;d<2;++d) if (deviceConfig(s,d).value("reference").toInt()==0) return reject("Stimulus measurements require a measured reference on each module.");
    }
    return true;
}
bool LanXiWorker::verifyIdentity(int index,const QJsonObject &info,QString &error) const {
    const QString expected=index==0 ? "3676" : "3677";
    if (typeOf(info)!=expected || info["numberOfInputChannels"].toInt()!=4 || info["numberOfOutputChannels"].toInt()!=(index==1 ? 1 : 0)) {
        error="Expected Type "+expected+" with its documented input/output count."; return false;
    }
    const auto serial=serialOf(info);
    if (serial.isEmpty() || (!m_devices[index].serial.isEmpty() && serial!=m_devices[index].serial)) { error="Module identity changed; inspect the addresses again."; return false; }
    if (info["license"].toString()=="noLicense") { error="The module reports no Open API licence. Check its firmware and licence page."; return false; }
    return true;
}
void LanXiWorker::inspect(const QVariantMap &settings) {
    if (m_busy) return;
    auto validationSettings=settings;
    auto g=validationSettings.value("generator").toMap(); g["mode"]="off"; validationSettings["generator"]=g;
    QString error;
    if (!validate(validationSettings,error)) { m_message=error; m_phase="disconnected"; m_ready=false; publish(); return; }
    m_settings=settings;
    // A new inspection changes module identities/configuration. Do not attach
    // retained samples from the previous setup to the new hardware metadata.
    resetData();m_runId.clear();m_runPath.clear();
    ++m_generation; m_busy=true; m_ready=false; m_review=false; m_phase="inspecting"; m_message="Reading both modules' identity and capabilities.";
    for(int i=0;i<2;++i) { m_devices[i].url=address(settings,i); if(m_devices[i].url.endsWith('/')) m_devices[i].url.chop(1); m_devices[i].serial.clear(); m_devices[i].label=i==0 ? "Type 3676" : "Type 3677"; }
    publish(); inspectDevice(0);
}
void LanXiWorker::inspectDevice(int index) {
    request(index,"GET","/module/info",{},[this,index](bool ok,const QJsonValue &value,const QString &why){
        QString error=why;
        if (!ok || !value.isObject() || !verifyIdentity(index,value.toObject(),error)) {
            m_busy=false; m_ready=false; m_phase="disconnected"; m_message=error.isEmpty() ? "Invalid module information." : error; publish(); return;
        }
        auto &d=m_devices[index]; d.info=value.toObject(); d.serial=serialOf(d.info); d.state=d.info["moduleState"].toString();
        const auto savedSerial=deviceConfig(m_settings,index).value("serial").toString();
        if(!savedSerial.isEmpty() && savedSerial!=d.serial) {
            m_busy=false;m_ready=false;m_phase="disconnected";m_message="The saved profile belongs to a different module serial number. Create a new profile and calibration for this hardware.";publish();return;
        }
        if (d.state!="Idle") { m_busy=false; m_ready=false; m_phase="disconnected"; m_message=d.label+" is in use ("+d.state+"). Close the other measurement application first."; publish(); return; }
        if (index==0) { inspectDevice(1); return; }
        if (m_devices[0].serial==m_devices[1].serial) { m_busy=false; m_phase="disconnected"; m_message="The two addresses resolve to the same module."; publish(); return; }
        m_busy=false; m_ready=true; m_phase="ready"; m_message="Both modules identified. Review channel conditioning and stimulus limits before Start.";
        auto devices=m_settings.value("devices").toList();
        for(int i=0;i<2;++i) { auto config=devices[i].toMap(); config["serial"]=m_devices[i].serial; devices[i]=config; }
        m_settings["devices"]=devices;
        if (m_owner) QMetaObject::invokeMethod(m_owner,[owner=m_owner,settings=m_settings]{if(owner){owner->m_settings=settings; emit owner->settingsChanged();}});
        publish();
    });
}
void LanXiWorker::start(int ticket,const QVariantMap &settings) {
    if (m_busy) return;
    QString error;
    if (!m_ready || !validate(settings,error) || address(settings,0).remove(QRegularExpression("/$"))!=m_devices[0].url
        || address(settings,1).remove(QRegularExpression("/$"))!=m_devices[1].url) {
        m_sequenceActive=false;m_sequenceRemaining=0;
        m_message=error.isEmpty() ? "Inspect both module addresses before starting." : error; publish();
        if (m_owner) QMetaObject::invokeMethod(m_owner,[owner=m_owner,ticket,message=m_message]{if(owner) emit owner->runEnded(ticket,false,message);}); return;
    }
    if(!m_sequenceActive) {
        const auto sequence=settings.value("sequence").toMap();const int repeats=sequence.value("repetitions",1).toInt();
        const double delay=sequence.value("delaySeconds",1).toDouble();
        if(repeats<1||repeats>100||!finiteRange(delay,0,3600)||(repeats>1&&settings.value("durationSeconds").toDouble()<=0&&settings.value("generator").toMap().value("mode").toString()!="steps")) {
            m_message="A sequence needs 1..100 repetitions, a 0..3600 s delay and a finite measurement duration or stepped-sine plan.";publish();
            if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,ticket,message=m_message]{if(owner)emit owner->runEnded(ticket,false,message);});return;
        }
        m_sequenceOptions=sequence;m_sequenceRemaining=repeats-1;m_sequenceActive=repeats>1||!sequence.value("acceptance").toMap().isEmpty();m_sequenceFailed=false;m_sequenceResults.clear();
        m_sequenceId=m_sequenceActive?QUuid::createUuid().toString(QUuid::WithoutBraces):QString();
    }
    m_cancel=false;
    ++m_generation; m_ticket=ticket; m_settings=settings; m_busy=true; m_stopping=false; m_review=false;
    m_phase="starting"; m_failure.clear(); m_errors.clear(); m_gate={}; m_stopDone={false,false}; m_output="Not checked";
    m_startedMs=m_clock.elapsed(); m_generator=false; m_generatorCommand=false; m_stepCollecting=false; m_step=0; m_steps.clear(); m_frequencies.clear();
    m_fft=static_cast<std::size_t>(settings.value("fftSize").toInt()); m_referenceFloor=settings.value("referenceFloor").toDouble();
    const auto generator=settings.value("generator").toMap();
    if(generator.value("mode").toString()=="steps") {
        const double rate=std::max(deviceConfig(settings,0).value("sampleRate").toDouble(),deviceConfig(settings,1).value("sampleRate").toDouble());
        while(m_fft<32768 && m_fft<4*rate/generator.value("startHz").toDouble()) m_fft*=2;
    }
    m_runPath.clear();m_runId.clear();m_record=settings.value("record").toBool(); resetData();
    try { beginArchive(); } catch(const std::exception &e) { fail(QString::fromUtf8(e.what())); return; }
    m_message="Opening the two independent acquisition streams."; publish(); openDevice(0);
}
void LanXiWorker::waitState(int index,const QString &expected,std::function<void()> done,int remaining) {
    const auto generation=m_generation;
    request(index,"GET","/onchange?last=0",{},[this,index,expected,done=std::move(done),remaining,generation](bool ok,const QJsonValue &value,const QString &error){
        if (!ok) { fail(error); return; }
        const auto object=value.toObject(); m_devices[index].state=object["moduleState"].toString();
        if (m_devices[index].state==expected) { done(); return; }
        if (remaining<=0) { fail("Timed out waiting for "+m_devices[index].label+" to enter "+expected); return; }
        QTimer::singleShot(100,this,[this,index,expected,done,remaining,generation]{if(generation==m_generation) waitState(index,expected,done,remaining-1);});
    });
}
void LanXiWorker::openDevice(int index) {
    request(index,"GET","/module/info",{},[this,index](bool ok,const QJsonValue &value,const QString &why){
        QString error=why;
        if (!ok || !verifyIdentity(index,value.toObject(),error)) { fail(error); return; }
        if (value.toObject()["moduleState"].toString()!="Idle") { fail(m_devices[index].label+" was opened by another client."); return; }
        m_gate.touched[index]=true; // An unanswered Open may still have executed.
        request(index,"PUT","/open",QJsonObject{{"performTransducerDetection",false},{"singleModule",true}},[this,index](bool ok,const QJsonValue &,const QString &error){
            if (!ok) { fail(error); return; }
            waitState(index,"RecorderOpened",[this,index]{
                const auto create=[this,index]{request(index,"PUT","/create",{},[this,index](bool ok,const QJsonValue &,const QString &error){
                    if(!ok){fail(error);return;} waitState(index,"RecorderConfiguring",[this,index]{configureDevice(index);});
                });};
                if (index==1) {
                    m_gate.outputTouched=true; m_output="Unknown";
                    request(1,"PUT","/generator/stop",outputSelection(),[this,create](bool ok,const QJsonValue &,const QString &error){
                        if(!ok){fail(error);return;} m_gate.outputOff=true; m_output="Off (acknowledged)"; create();
                    });
                } else create();
            });
        });
    });
}
bool LanXiWorker::applySetup(int index,QString &error) {
    auto &d=m_devices[index]; const auto config=deviceConfig(m_settings,index);
    const auto requested=config.value("channels").toList(); auto channels=d.setup["channels"].toArray();
    const int rate=config.value("sampleRate").toInt();
    if (channels.size()!=4 || !d.info["supportedSampleRates"].toArray().contains(rate)) { error="Sample rate or channel count is not supported by "+d.label; return false; }
    d.physical.clear(); const QString bandwidth=QString::number(rate/2560.0,'g',8)+" kHz";
    bool anyPolarization=false;
    for(int i=0;i<4;++i) {
        const auto ch=requested[i].toMap(); auto actual=channels[i].toObject();
        const QString range=ch.value("range").toString(), filter=ch.value("filter").toString();
        if (!d.info["supportedRanges"].toArray().contains(range) || !d.info["supportedFilters"].toArray().contains(filter)) { error="Select a supported input range and filter for "+d.label; return false; }
        anyPolarization|=ch.value("polarization").toBool();
        if (ch.value("ccld").toBool() && d.info.contains("preampInputSupported") && !d.info["preampInputSupported"].toBool()) { error=d.label+" does not advertise sensor powering."; return false; }
        actual["channel"]=i+1; actual["enabled"]=ch.value("enabled").toBool(); actual["name"]=ch.value("name").toString();
        actual["destinations"]=QJsonArray{"socket"}; actual["bandwidth"]=bandwidth; actual["range"]=range; actual["filter"]=filter;
        actual["ccld"]=ch.value("ccld").toBool(); actual["polVolt"]=ch.value("polarization").toBool(); actual["floating"]=false;
        auto transducer=actual["transducer"].toObject(); transducer["sensitivity"]=ch.value("sensitivity").toDouble(); transducer["unit"]=ch.value("unit").toString();
        transducer["requiresCcld"]=ch.value("ccld").toBool(); transducer["requires200V"]=ch.value("polarization").toBool(); transducer.remove("teds");
        actual["transducer"]=transducer; channels[i]=actual;
        if(ch.value("enabled").toBool()) d.physical.push_back(i+1);
    }
    if(anyPolarization) {
        const auto panel=d.info["module"].toObject()["frontpanel"].toObject()["type"].toObject();
        if(index!=1 || panel["number"].toString()!="3102" || panel["variant"].toString()!="041") { error="200 V polarization requires the verified UA-3102-041 front panel."; return false; }
        for(const auto &ch:requested) if(!ch.toMap().value("polarization").toBool()) { error="The module requires the same polarization setting on all channels."; return false; }
    }
    d.setup["channels"]=channels; d.setup["name"]=m_settings.value("name").toString(); return true;
}
void LanXiWorker::configureDevice(int index) {
    request(index,"GET","/channels/input/default",{},[this,index](bool ok,const QJsonValue &value,const QString &why){
        if(!ok){fail(why);return;} m_devices[index].setup=value.toObject(); QString error;
        if(!applySetup(index,error)){fail(error);return;}
        request(index,"PUT","/channels/input",m_devices[index].setup,[this,index](bool ok,const QJsonValue &,const QString &error){
            if(!ok){fail(error);return;} waitState(index,"RecorderStreaming",[this,index]{connectStream(index);});
        });
    });
}
void LanXiWorker::connectStream(int index) {
    request(index,"GET","/destination/socket",{},[this,index](bool ok,const QJsonValue &value,const QString &error){
        const int port=value.toObject()["tcpPort"].toInt(); if(!ok || port<1 || port>65535){fail(ok ? "Invalid streaming port." : error);return;}
        auto &d=m_devices[index]; auto *socket=new QTcpSocket(this); d.socket=socket; socket->setReadBufferSize(4*1024*1024);
        connect(socket,&QTcpSocket::readyRead,this,[this,index,socket]{if(m_devices[index].socket==socket) consume(index);});
        connect(socket,&QTcpSocket::errorOccurred,this,[this,index,socket](QAbstractSocket::SocketError){
            if(m_devices[index].socket==socket && !m_stopping) fail(m_devices[index].label+": "+socket->errorString());
        });
        connect(socket,&QTcpSocket::disconnected,this,[this,index,socket]{
            if(m_devices[index].socket==socket && !m_stopping && m_busy) fail(m_devices[index].label+": stream disconnected.");
        });
        const auto generation=m_generation;
        connect(socket,&QTcpSocket::connected,this,[this,index,generation]{
            if(generation!=m_generation)return;
            request(index,"POST","/measurements",{},[this,index](bool ok,const QJsonValue &,const QString &error){
                if(!ok){fail(error);return;} waitState(index,"RecorderRecording",[this,index]{
                    if(index==0) openDevice(1); else acquisitionReady();
                });
            });
        });
        QTimer::singleShot(6000,this,[this,index,generation]{if(generation==m_generation && m_devices[index].socket && m_devices[index].socket->state()!=QAbstractSocket::ConnectedState) fail("Streaming socket connection timed out.");});
        socket->connectToHost(QUrl(d.url).host(),static_cast<quint16>(port));
    });
}
void LanXiWorker::acquisitionReady() {
    if(m_phase!="starting" || m_stopping)return;
    for(const auto &d:m_devices) {
        if(d.state!="RecorderRecording" || !d.receiving)return;
        for(std::size_t i=0;i<d.physical.size();++i)if(!d.channels[i].total || d.channels[i].quality)return;
    }
    m_phase="running"; m_message="Acquiring both modules with independent sample clocks."; m_startedMs=m_clock.elapsed();
    if(m_owner) QMetaObject::invokeMethod(m_owner,[owner=m_owner,ticket=m_ticket]{if(owner)emit owner->runStarted(ticket);});
    if(m_settings.value("generator").toMap().value("mode").toString()!="off") prepareGenerator();
    publish();
}
void LanXiWorker::prepareGenerator() {
    if(m_stopping)return;
    m_generatorCommand=true; m_gate.outputTouched=true;
    const auto g=m_settings.value("generator").toMap(); const auto mode=g.value("mode").toString();
    m_currentFrequency=g.value("frequency").toDouble();
    if(mode=="steps") { m_frequencies=lanxi::logarithmicSteps(g.value("startHz").toDouble(),g.value("stopHz").toDouble(),g.value("steps").toInt()); m_currentFrequency=m_frequencies.front(); }
    // Prepare/configure AFTER both acquisitions start. The documented single-
    // module trigger must not be changed by starting input between setup/start.
    request(1,"PUT","/generator/prepare",outputSelection(),[this](bool ok,const QJsonValue &,const QString &error){
        if(!ok){fail(error);return;} m_gate.outputOff=true;
        setGenerator(m_currentFrequency,0,[this]{startGenerator();});
    });
}
void LanXiWorker::setGenerator(double frequency,double level,std::function<void()> done) {
    if(m_stopping)return;
    const auto g=m_settings.value("generator").toMap();
    if(!finiteRange(level,0,g.value("limitVp").toDouble()) || level>=10) { fail("Generator amplitude limit violated."); return; }
    QJsonObject input{{"number",1},{"signalType","sine"},{"gain",1.0},{"offset",0.0},{"frequency",frequency},{"phase",0.0}};
    if(g.value("mode").toString()=="noise") {
        input["signalType"]="random"; input.remove("frequency"); input.remove("phase");
        input["center_frequency"]=(g.value("startHz").toDouble()+g.value("stopHz").toDouble())/2;
        input["bandwidth"]=g.value("stopHz").toDouble()-g.value("startHz").toDouble();
        input["hp_filter"]=false; input["pink_filter"]=false; input["gen_number"]=0;
    }
    QJsonObject output{{"number",1},{"gain",level/10.0},{"offset",0.0},{"floating",false},{"mixfunction","sum"},
        {"inputs",QJsonArray{input,QJsonObject{{"number",2},{"signalType","none"}}}}};
    request(1,"PUT","/generator/output",QJsonObject{{"outputs",QJsonArray{output}}},[this,done](bool ok,const QJsonValue &,const QString &error){if(!ok){fail(error);return;}done();});
}
void LanXiWorker::startGenerator() {
    m_gate.outputOff=false; m_output="Starting";
    request(1,"PUT","/generator/start",outputSelection(),[this](bool ok,const QJsonValue &,const QString &error){
        if(!ok){m_output="Unknown";fail(error);return;}
        m_generator=true; m_generatorMs=m_clock.elapsed(); m_output="On (ramping)";
        rampGenerator(1);
    });
}
void LanXiWorker::rampGenerator(int step) {
    const auto generation=m_generation;const auto g=m_settings.value("generator").toMap();
    const int interval=std::max(10,int(g.value("rampSeconds").toDouble()*100));
    setGenerator(m_currentFrequency,g.value("levelVp").toDouble()*step/10,[this,generation,step,interval]{
        if(step<10)QTimer::singleShot(interval,this,[this,generation,step]{if(generation==m_generation&&!m_stopping)rampGenerator(step+1);});
        else{m_generatorCommand=false;m_output="On";m_stepMs=m_clock.elapsed();m_stepCollecting=false;publish();}
    });
}
void LanXiWorker::outputOff() { if(m_busy && m_gate.outputTouched) stop(); }
void LanXiWorker::fail(const QString &message) {
    if(m_failure.isEmpty()) m_failure=message.isEmpty() ? "LAN-XI operation failed." : message;
    if(!m_stopping) stop(m_failure);
}
void LanXiWorker::stop(const QString &reason) {
    if(!m_busy || m_stopping)return;
    if(m_phase=="sequence-wait") {
        ++m_generation;m_busy=false;m_sequenceActive=false;m_sequenceRemaining=0;m_phase="finished";m_message="Sequence cancelled between measurements; both modules remain idle.";publish();
        try {const QString path=m_root+"/Sequences/"+m_sequenceId+".json";auto record=readJson(path);record["state"]="cancelled";record["cancelledUtc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);QString error;
            if(!writeJson(path,record,error))m_message+=" Could not update sequence record: "+error;
        }catch(const std::exception&e){m_message+=" Could not update sequence record: "+QString::fromUtf8(e.what());}
        publish();
        if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,ticket=m_ticket,message=m_message]{if(owner)emit owner->runEnded(ticket,true,message);});return;
    }
    if(m_phase=="inspecting"||m_phase=="discovering"||m_phase=="transducers") {
        ++m_generation;for(auto &d:m_devices)if(d.reply)d.reply->abort();
        for(auto &reply:m_discoveryReplies)if(reply)reply->abort();m_discoveryReplies.clear();
        m_busy=false;m_ready=false;m_phase="disconnected";m_message="Hardware inspection cancelled.";publish();return;
    }
    if(m_review){m_cancel=true;return;}
    if(!reason.isEmpty() && m_failure.isEmpty())m_failure=reason;
    ++m_generation; m_stopping=true; m_phase="stopping"; m_message="Stopping output, then both inputs and files."; m_generatorCommand=false;
    for(auto &d:m_devices) if(d.reply)d.reply->abort();
    m_stopDone={false,false}; publish();
    // Stop the generator first, but only after rechecking the owned module's identity.
    if(m_gate.outputTouched) {
        request(1,"GET","/module/info",{},[this](bool ok,const QJsonValue &value,const QString &why){
            QString error=why;
            if(!ok || !verifyIdentity(1,value.toObject(),error)) {
                m_errors.append(error); m_output="Unknown"; m_gate.outputOff=false;
                if(m_failure.isEmpty())m_failure="Cannot confirm generator stop: "+error;
                stopDevice(0); stopDevice(1); return;
            }
            request(1,"PUT","/generator/stop",outputSelection(),[this](bool ok,const QJsonValue &,const QString &error){
                m_gate.outputOff=ok; m_output=ok ? "Off (acknowledged)" : "Unknown"; m_generator=false;
                if(!ok){m_errors.append(error);if(m_failure.isEmpty())m_failure="Generator stop was not acknowledged: "+error;}
                const auto generation=m_generation;
                QTimer::singleShot(ok ? 200 : 0,this,[this,generation]{if(generation==m_generation){stopDevice(0);stopDevice(1);}});
            });
        });
    } else {stopDevice(0);stopDevice(1);}
}
void LanXiWorker::stopDevice(int index,int remaining) {
    if(!m_gate.touched[index]){stoppedDevice(index);return;}
    if(remaining<=0){stoppedDevice(index,"Module did not acknowledge the stop sequence.");return;}
    request(index,"GET","/module/info",{},[this,index,remaining](bool ok,const QJsonValue &value,const QString &why){
        QString error=why;
        if(!ok || !verifyIdentity(index,value.toObject(),error)){stoppedDevice(index,error);return;}
        const QString state=value.toObject()["moduleState"].toString(); m_devices[index].state=state;
        if(state=="Idle"){m_gate.idle[index]=true;stoppedDevice(index);return;}
        QString path;
        if(state=="RecorderRecording")path="/measurements/stop";
        else if(state=="RecorderStreaming")path="/finish";
        else if(state=="RecorderConfiguring")path="/cancel";
        else if(state=="RecorderOpened")path="/close";
        else {stoppedDevice(index,"Unrecognized module state during recovery: "+state);return;}
        request(index,"PUT",path,{},[this,index,remaining](bool ok,const QJsonValue &,const QString &error){
            if(!ok){stoppedDevice(index,error);return;} const auto generation=m_generation;
            QTimer::singleShot(150,this,[this,index,remaining,generation]{if(generation==m_generation)stopDevice(index,remaining-1);});
        });
    });
}
void LanXiWorker::stoppedDevice(int index,const QString &error) {
    if(!error.isEmpty()){m_errors.append(m_devices[index].label+": "+error);if(m_failure.isEmpty())m_failure="Module stop was not acknowledged: "+error;}
    auto &d=m_devices[index]; if(d.socket){while(d.socket && d.socket->bytesAvailable()>0)consume(index);d.socket->disconnect(this);d.socket->abort();d.socket->deleteLater();d.socket=nullptr;}
    try{d.decoder.finish();}catch(const std::exception &e){if(m_failure.isEmpty())m_failure=QString::fromUtf8(e.what());}
    if(d.raw.isOpen()){if(!d.raw.flush() && m_failure.isEmpty())m_failure=d.raw.errorString();d.raw.close();}
    m_stopDone[index]=true; if(m_stopDone[0] && m_stopDone[1])finishStop();
}
void LanXiWorker::finishStop() {
    if(!m_gate.released()) {
        m_phase="recovery"; m_ready=false; m_busy=true; m_message="Hardware state is unresolved. Output may still be active. Restore the connection and use Retry stop. "+m_errors.join("; ");
        writeManifest("recovery-required"); publish(true);
        if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,ticket=m_ticket,message=m_message]{if(owner)emit owner->recoveryRequired(ticket,message);}); return;
    }
    // Keep the stop guard set while committing files: a full disk is an error
    // outcome, not an exception escaping the network event loop.
    writeManifest(m_failure.isEmpty() ? "complete" : "partial");
    m_busy=false; m_stopping=false; m_phase=m_failure.isEmpty() ? "finished" : "error";
    if(finishSequenceStep())return;
    m_phase=m_failure.isEmpty()?"finished":"error";
    const QString released=m_gate.touched[0]&&m_gate.touched[1] ? "Both modules are now idle."
        : m_gate.touched[0]||m_gate.touched[1] ? "The opened module is now idle." : "No module was opened.";
    m_message=(m_failure.isEmpty()?"Measurement finished. ":m_failure+" ")+released
        +(m_gate.outputTouched?" Generator output is off (acknowledged).":" No generator command was sent.");
    refreshRuns(); publish(true);
    if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,ticket=m_ticket,ok=m_failure.isEmpty(),message=m_message]{if(owner)emit owner->runEnded(ticket,ok,message);});
}
void LanXiWorker::recover() {if(m_phase=="recovery"){m_stopping=false; stop();}}
void LanXiWorker::selectChannel(int device,int channel) {if(device>=0&&device<2&&channel>=1&&channel<=4){m_selectedDevice=device;m_selectedChannel=channel;publish(true);}}
void LanXiWorker::pauseDisplay(bool value) {m_paused=value; publish(!value);}
