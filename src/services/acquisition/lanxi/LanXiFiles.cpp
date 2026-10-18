#include "LanXiWorker.h"
#include "LanXiBackend.h"
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStorageInfo>
#include <QUuid>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool yes,const QString &why){if(!yes)throw std::runtime_error(why.toStdString());}
bool safeId(const QString &id){return QRegularExpression("^[a-f0-9-]{36}$").match(id).hasMatch();}
QJsonObject clockJson(const lanxi::Time &time) {
    QJsonArray family;for(auto exponent:time.family)family.append(int(exponent));
    return {{"family",family},{"count",QString::number(qulonglong(time.count))}};
}
}
QVariantMap LanXiWorker::defaults(const QString &workflow) {
    QVariantList devices;
    for(int d=0;d<2;++d) {
        QVariantList channels;
        for(int c=1;c<=4;++c)channels.append(QVariantMap{{"channel",c},{"enabled",true},{"name",QString("Ch %1").arg(c)},
            {"unit","V"},{"sensitivity",1.0},{"range","10 Vpeak"},{"filter","7.0 Hz"},{"ccld",false},{"polarization",false}});
        devices.append(QVariantMap{{"url",""},{"serial",""},{"sampleRate",65536},{"reference",1},{"channels",channels}});
    }
    return {{"schema",1},{"name","Measurement"},{"workflow",workflow},{"devices",devices},{"fftSize",4096},
        {"referenceFloor",1e-6},{"durationSeconds",10.0},{"record",true},
        {"analysis",QVariantMap{{"window","hann"},{"averaging","linear"},{"averages",8},{"removeMean",false},
            {"weighting","Z"},{"timeWeighting","Fast"},{"fraction",3},{"highPassHz",0.0},{"lowPassHz",0.0},
            {"toneHz",1000.0},{"harmonics",5}}},
        {"trigger",QVariantMap{{"enabled",false},{"device",0},{"channel",1},{"rising",true},{"level",0.0},
            {"hysteresis",0.01},{"preSeconds",0.1},{"postSeconds",0.5}}},
        {"generator",QVariantMap{{"mode","off"},{"armed",false},{"frequency",1000.0},{"levelVp",0.01},
            {"limitVp",0.1},{"startHz",100.0},{"stopHz",10000.0},{"steps",16},{"settleSeconds",0.25},
            {"dwellSeconds",0.5},{"rampSeconds",0.2}}}};
}
void LanXiWorker::resetProfile(const QString &workflow) {
    if(m_busy)return;
    if(!QStringList{"acoustic","microphone","amplifier"}.contains(workflow))return;
    const auto previous=m_settings.value("devices").toList();m_settings=defaults(workflow);
    auto devices=m_settings.value("devices").toList();
    if(previous.size()==2)for(int i=0;i<2;++i){auto d=devices[i].toMap();d["url"]=previous[i].toMap().value("url");devices[i]=d;}
    m_settings["devices"]=devices;m_ready=false;m_phase="disconnected";
    m_message="Choose channel sensitivities and units explicitly. New profiles use voltage inputs with powering off.";
    if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,s=m_settings]{if(owner){owner->m_settings=s;emit owner->settingsChanged();}});
    publish();
}
bool LanXiWorker::writeJson(const QString &path,const QJsonObject &value,QString &error) {
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    const auto bytes=QJsonDocument(value).toJson(QJsonDocument::Indented);
    if(file.write(bytes)!=bytes.size() || !file.commit()){error=file.errorString();return false;}return true;
}
QJsonObject LanXiWorker::readJson(const QString &path) {
    QFile file(path);require(!QFileInfo(path).isSymLink() && file.open(QIODevice::ReadOnly),"Cannot read "+path);
    require(file.size()<=2*1024*1024,"JSON file exceeds the size limit.");
    QJsonParseError error;const auto document=QJsonDocument::fromJson(file.readAll(),&error);
    require(error.error==QJsonParseError::NoError && document.isObject(),"Invalid JSON file: "+path);return document.object();
}
void LanXiWorker::saveProfile(const QString &name,const QVariantMap &settings) {
    if(m_busy)return;
    auto checked=settings;auto generator=checked.value("generator").toMap();generator["armed"]=true;checked["generator"]=generator;
    QString error;
    if(name.trimmed().isEmpty() || name.size()>100 || !validate(checked,error)){m_message=error.isEmpty()?"Enter a profile name (1–100 characters).":error;publish();return;}
    auto saved=settings;generator=saved.value("generator").toMap();generator["armed"]=false;saved["generator"]=generator;saved["name"]=name.trimmed();
    const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject doc{{"schema",1},{"id",id},{"name",name.trimmed()},{"settings",QJsonObject::fromVariantMap(saved)}};
    m_message=writeJson(m_root+"/Profiles/"+id+".json",doc,error)?"Profile saved.":error;refreshRuns();
}
void LanXiWorker::loadProfile(const QString &id) {
    if(m_busy)return;
    try {
        require(safeId(id),"Invalid profile identifier.");const auto doc=readJson(m_root+"/Profiles/"+id+".json");
        auto s=doc["settings"].toObject().toVariantMap();auto check=s;auto g=check.value("generator").toMap();g["armed"]=true;check["generator"]=g;QString error;
        require(doc["schema"].toInt()==1 && validate(check,error),error.isEmpty()?"Unsupported profile.":error);
        g=s.value("generator").toMap();g["armed"]=false;s["generator"]=g;m_settings=s;m_ready=false;m_phase="disconnected";
        m_message="Profile loaded with stimulus disarmed. Inspect both modules before acquisition.";
        if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,s]{if(owner){owner->m_settings=s;emit owner->settingsChanged();}});
    }catch(const std::exception &e){m_message=QString::fromUtf8(e.what());}publish();
}
QString LanXiWorker::archivePath(const QString &id) const {
    require(safeId(id),"Invalid session identifier.");const QString path=m_root+"/Runs/"+id;
    require(!QFileInfo(path).isSymLink(),"Session directory cannot be a symbolic link.");return path;
}
void LanXiWorker::refreshRuns() {
    m_runs.clear();m_profiles.clear();
    const auto profiles=QDir(m_root+"/Profiles").entryInfoList({"*.json"},QDir::Files|QDir::NoSymLinks,QDir::Time);
    for(const auto &file:profiles) {
        if(m_profiles.size()>=100)break;
        try{const auto doc=readJson(file.absoluteFilePath());if(safeId(doc["id"].toString()))m_profiles.append(QVariantMap{{"id",doc["id"].toString()},{"name",doc["name"].toString()}});}catch(const std::exception &){}
    }
    const auto runs=QDir(m_root+"/Runs").entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDir::Time);
    for(const auto &file:runs) {
        if(m_runs.size()>=100)break;
        try{const auto doc=readJson(file.absoluteFilePath()+"/manifest.json");if(safeId(file.fileName()))m_runs.append(QVariantMap{
            {"id",file.fileName()},{"name",doc["settings"].toObject()["name"].toString()},
            {"started",doc["startedUtc"].toString()},{"outcome",doc["outcome"].toString()},
            {"recorded",doc["recorded"].toBool()},{"workflow",doc["settings"].toObject()["workflow"].toString()}});}catch(const std::exception &){}
    }
    publish();
}
void LanXiWorker::beginArchive() {
    m_runId=QUuid::createUuid().toString(QUuid::WithoutBraces);m_runPath=archivePath(m_runId);
    m_startedUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    require(QDir().mkpath(m_runPath),"Cannot create the measurement directory.");
    QStorageInfo storage(m_runPath);require(storage.isValid() && storage.bytesAvailable()>64*1024*1024,"Insufficient space to start recording (64 MiB reserve required).");
    writeManifest("starting");
}
void LanXiWorker::writeRaw(int index,const QByteArray &bytes) {
    if(!m_record || m_review)return;
    auto &d=m_devices[index];
    if(!d.raw.isOpen() || d.raw.size()+bytes.size()>512LL*1024*1024) {
        if(d.raw.isOpen()){require(d.raw.flush(),d.raw.errorString());d.raw.close();}
        const QString name=QString("%1-%2.stream").arg(index==0?3676:3677).arg(d.segments.size(),4,10,QChar('0'));
        d.raw.setFileName(m_runPath+"/"+name);require(d.raw.open(QIODevice::WriteOnly|QIODevice::NewOnly),d.raw.errorString());d.segments.append(name);
        writeManifest("in-progress");
    }
    require(d.raw.write(bytes)==bytes.size(),"Raw stream write failed: "+d.raw.errorString());d.written+=bytes.size();
}
QJsonObject LanXiWorker::manifest() const {
    QJsonArray devices;
    for(int i=0;i<2;++i) {
        const auto &d=m_devices[i];QJsonArray channels,segments;
        for(const auto &s:d.segments)segments.append(s);
        for(std::size_t n=0;n<d.physical.size();++n) {
            const auto &c=d.channels[n];channels.append(QJsonObject{{"channel",d.physical[n]},
                {"signalId",int(n+1)},{"samples",QString::number(qulonglong(c.total))},{"firstTimestamp",clockJson(c.first)},
                {"lastBlockTimestamp",clockJson(c.last)},{"period",clockJson(c.descriptor.period)},
                {"scale",c.descriptor.scale},{"offset",c.descriptor.offset},{"unit",QString::fromStdString(c.descriptor.unit)},
                {"quality",int(c.quality)}});
        }
        devices.append(QJsonObject{{"type",i==0?3676:3677},{"serial",d.serial},{"url",d.url},{"info",d.info},
            {"setup",d.setup},{"channels",channels},{"segments",segments},{"rawBytes",QString::number(d.written)},
            {"idleConfirmed",m_gate.idle[i]}});
    }
    auto settings=m_settings;auto g=settings.value("generator").toMap();g["armed"]=false;settings["generator"]=g;
    return {{"schema",1},{"id",m_runId},{"startedUtc",m_startedUtc},{"updatedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"clockRelationship","independent"},{"settings",QJsonObject::fromVariantMap(settings)},{"devices",devices},
        {"recorded",m_record},{"actualFftSize",int(m_fft)},{"output",m_output},{"failure",m_failure},
        {"errors",QJsonArray::fromStringList(m_errors)},{"steps",QJsonArray::fromVariantList(m_steps)},
        {"qualityEvents",QJsonArray::fromVariantList(m_qualityEvents)}};
}
void LanXiWorker::writeManifest(const QString &outcome) {
    if(m_runPath.isEmpty() || m_review)return;
    auto doc=manifest();doc["outcome"]=outcome;QString error;
    if(!writeJson(m_runPath+"/manifest.json",doc,error)) {
        if(m_failure.isEmpty())m_failure="Cannot save the run manifest: "+error;
        m_message=m_failure;
        if(!m_stopping)throw std::runtime_error(m_failure.toStdString());
    }
    if(outcome=="complete"||outcome=="partial"||outcome=="recovery-required") {
        const QString summaryRoot=qEnvironmentVariable("SUNO_LANXI_DATA_DIR").isEmpty()
            ? QDir(m_root).absoluteFilePath("..") : m_root+"/Summaries";
        QDir().mkpath(summaryRoot);
        QSaveFile summary(summaryRoot+"/LanXi-"+m_runId+".txt");
        if(summary.open(QIODevice::WriteOnly)) {
            const QByteArray text=("LAN-XI Measurement Studio\nSession: "+m_settings.value("name").toString()+"\nOutcome: "+outcome
                +"\nTime: "+m_startedUtc+"\nIndependent module clocks\nArchive: "+m_runPath+"\n"+m_failure
                +"\nOpen LAN-XI Measurement Studio > Review to replay or export this session.\n").toUtf8();
            if(summary.write(text)==text.size())summary.commit();
        }
    }
}
void LanXiWorker::review(const QString &id) {if(!m_busy)readArchive(id,{});}
void LanXiWorker::exportRun(const QString &id,const QString &format) {
    if(m_busy)return;
    if(format!="csv"&&format!="wav"){m_message="Export format must be csv or wav.";publish();return;}
    readArchive(id,format);
}
void LanXiWorker::readArchive(const QString &id,const QString &format,const QVariantMap &analysis) {
    m_busy=true;m_review=true;m_ready=false;m_phase=format.isEmpty()?"reviewing":"exporting";if(!m_batch)m_cancel=false;
    QString exportPath; QStringList recoveryNotes; QJsonArray exportedChannels;
    try {
        const QString path=archivePath(id);const auto doc=readJson(path+"/manifest.json");
        require(doc["schema"].toInt()==1 && doc["clockRelationship"].toString()=="independent","Unsupported measurement archive.");
        require(doc["recorded"].toBool(),"This session has no raw recording to replay.");
        const auto outcome=doc["outcome"].toString();m_reviewOutcome=outcome;m_reviewNotes.clear();
        require(outcome=="complete"||outcome=="partial"||outcome=="interrupted"||outcome=="in-progress"||outcome=="recovery-required","This archive has not captured a stream.");
        auto savedSettings=doc["settings"].toObject().toVariantMap();
        if(!analysis.isEmpty())savedSettings["analysis"]=analysis;
        auto check=savedSettings;auto generator=check.value("generator").toMap();generator["armed"]=true;check["generator"]=generator;QString configError;
        require(validate(check,configError),configError);
        const int fft=doc["actualFftSize"].toInt();require(fft>=256 && fft<=32768 && !(fft&(fft-1)),"Invalid recorded FFT size.");
        m_settings=savedSettings;m_fft=std::size_t(fft);
        m_referenceFloor=m_settings.value("referenceFloor").toDouble();resetData();m_steps=doc["steps"].toArray().toVariantList();
        m_runId=id;m_runPath=path;m_startedUtc=doc["startedUtc"].toString();m_record=false;m_output="Replay — no hardware commands";
        const auto devices=doc["devices"].toArray();require(devices.size()==2,"Invalid device list in archive.");
        if(!format.isEmpty()) {exportPath=path+"/exports/"+QUuid::createUuid().toString(QUuid::WithoutBraces);require(QDir().mkpath(exportPath),"Cannot create export directory.");}
        for(int d=0;d<2;++d) {
            auto &device=m_devices[d];const auto data=devices[d].toObject();device.info=data["info"].toObject();device.setup=data["setup"].toObject();
            device.serial=data["serial"].toString();device.label=d==0?"Type 3676":"Type 3677";device.url=data["url"].toString();device.physical.clear();
            for(const auto &ch:device.setup["channels"].toArray())if(ch.toObject()["enabled"].toBool())device.physical.push_back(ch.toObject()["channel"].toInt());
            require(device.physical.size()>0 && device.physical.size()<=4,"Invalid saved channel mapping.");
            std::array<std::unique_ptr<QSaveFile>,4> outputs;
            std::array<quint32,4> wavSamples{};std::array<int,4> wavSegment{};std::array<std::uint64_t,4> total{};
            bool prefixStopped=false, writingExport=false;
            const auto closeWave=[&](int channel) {
                auto &out=outputs[std::size_t(channel)];if(!out)return;
                QDataStream stream(out.get());stream.setByteOrder(QDataStream::LittleEndian);
                require(out->seek(4),"Cannot update WAV size.");stream<<quint32(48+wavSamples[std::size_t(channel)]*4);
                require(out->seek(44),"Cannot update WAV sample count.");stream<<wavSamples[std::size_t(channel)];
                require(out->seek(52),"Cannot update WAV data size.");stream<<quint32(wavSamples[std::size_t(channel)]*4);
                require(stream.status()==QDataStream::Ok && out->commit(),"Cannot finalize WAV export.");out.reset();
            };
            for(const auto &segment:data["segments"].toArray()) {
                if(prefixStopped)break;
                const QString name=segment.toString();require(QRegularExpression(d==0?"^3676-[0-9]{4,}\\.stream$":"^3677-[0-9]{4,}\\.stream$").match(name).hasMatch(),"Invalid stream segment name.");
                QFile raw(path+"/"+name);require(!QFileInfo(raw).isSymLink() && raw.open(QIODevice::ReadOnly),"Cannot open stream segment.");
                while(!raw.atEnd()) {
                    require(!m_cancel.load(),"Operation cancelled.");const auto bytes=raw.read(65536);require(!bytes.isEmpty(),"Cannot read raw recording.");
                    try { device.decoder.feed(bytes.constData(),std::size_t(bytes.size()),[&,this,d](const lanxi::Event &event){
                        // Replay uses the same decoder, scaling, quality and analysis as live capture.
                        accept(d,event);
                        if(format.isEmpty() || event.kind!=lanxi::Event::Kind::Samples)return;
                        writingExport=true;
                        const int c=event.signal-1;require(c>=0 && c<int(device.physical.size()),"Invalid export signal.");
                        if(format=="wav" && outputs[c] && wavSamples[c]+event.samples.size()>128*1024*1024)closeWave(c);
                        if(!outputs[c]) {
                            QString filename=QString("%1-ch%2").arg(d==0?3676:3677).arg(device.physical[c]);
                            if(format=="wav")filename+=QString("-%1").arg(wavSegment[c]++,4,10,QChar('0'));
                            filename+="."+format;outputs[c]=std::make_unique<QSaveFile>(exportPath+"/"+filename);
                            require(outputs[c]->open(QIODevice::WriteOnly),"Cannot create export file.");
                            if(format=="csv")require(outputs[c]->write("sample_index,time_since_first_sample_s,value,unit,quality\n")>0,"CSV header write failed.");
                            else {
                                const auto rate=event.descriptor.sampleRate();require(std::abs(rate-std::round(rate))<1e-6,"WAV requires an integer sample rate.");
                                QDataStream s(outputs[c].get());s.setByteOrder(QDataStream::LittleEndian);
                                s.writeRawData("RIFF",4);s<<quint32(0);s.writeRawData("WAVEfmt ",8);s<<quint32(16)<<quint16(3)<<quint16(1)<<quint32(rate)<<quint32(rate*4)<<quint16(4)<<quint16(32);
                                s.writeRawData("fact",4);s<<quint32(4)<<quint32(0);s.writeRawData("data",4);s<<quint32(0);wavSamples[c]=0;
                                require(s.status()==QDataStream::Ok,"WAV header write failed.");
                            }
                        }
                        if(format=="csv") {
                            QByteArray rows;
                            for(double v:event.samples){rows+=QByteArray::number(qulonglong(total[c]))+','+QByteArray::number(total[c]/event.descriptor.sampleRate(),'g',17)+','+QByteArray::number(v,'g',17)+','+QByteArray::fromStdString(event.descriptor.unit)+','+QByteArray::number(device.channels[c].quality)+'\n';++total[c];}
                            require(outputs[c]->write(rows)==rows.size(),"CSV write failed.");
                        }else {
                            QDataStream s(outputs[c].get());s.setByteOrder(QDataStream::LittleEndian);s.setFloatingPointPrecision(QDataStream::SinglePrecision);
                            for(double v:event.samples)s<<float(v);wavSamples[c]+=quint32(event.samples.size());total[c]+=event.samples.size();
                            require(s.status()==QDataStream::Ok,"WAV write failed.");
                        }
                        writingExport=false;
                    }); } catch(const std::exception &e) {
                        // An interrupted recording may have an incomplete tail or
                        // an overrun. Recover only its valid prefix, never splice
                        // samples across a gap or hide an export/disk error.
                        if(outcome=="complete" || writingExport)throw;
                        recoveryNotes.append(device.label+": valid prefix only; "+QString::fromUtf8(e.what()));
                        prefixStopped=true;break;
                    }
                    m_message=QString("%1 %2 · %3 MiB read").arg(format.isEmpty()?"Replaying":"Exporting",device.label).arg(raw.pos()/1048576);publish(true);
                }
            }
            if(!prefixStopped)try{device.decoder.finish();}catch(const std::exception &e){
                if(outcome=="complete")throw;
                recoveryNotes.append(device.label+": incomplete tail omitted; "+QString::fromUtf8(e.what()));
            }
            for(int c=0;c<4;++c)if(outputs[c]){if(format=="wav")closeWave(c);else require(outputs[c]->commit(),"Cannot finalize CSV export.");}
            for(std::size_t c=0;c<device.physical.size();++c)exportedChannels.append(QJsonObject{
                {"device",d},{"channel",device.physical[c]},{"samples",QString::number(qulonglong(total[c]))}});
        }
        if(!format.isEmpty()){auto exported=doc;exported["format"]=format;exported["exportedChannels"]=exportedChannels;exported["recoveryNotes"]=QJsonArray::fromStringList(recoveryNotes);
            exported["note"]="Each file represents one physical channel. Modules have independent clocks. WAV float values are in the recorded engineering unit; consult this manifest for calibration and quality.";QString error;require(writeJson(exportPath+"/manifest.json",exported,error),error);}
        m_message=format.isEmpty()?"Recording loaded ("+outcome+"). Each module retains its own time base.":"Exported to "+exportPath;
        m_reviewNotes=recoveryNotes.join("; ");
        if(!recoveryNotes.isEmpty())m_message+=" Partial recovery: "+m_reviewNotes;
        m_phase="review";
    }catch(const std::exception &e){m_phase="error";m_message=QString::fromUtf8(e.what());if(!exportPath.isEmpty()){QString ignored;writeJson(exportPath+"/manifest.json",{{"outcome","partial-export"},{"error",m_message}},ignored);}}
    m_busy=false;
    if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,settings=m_settings]{if(owner){owner->m_settings=settings;emit owner->settingsChanged();}});
    publish(true);
}
