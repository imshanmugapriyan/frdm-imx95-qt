#include "LanXiWorker.h"
#include "LanXiBackend.h"
#include <QDateTime>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
QVariant nullable(double v) { return std::isfinite(v) ? QVariant(v) : QVariant(); }
QVariantMap dc(const QVariantMap &s,int d) {return s.value("devices").toList()[d].toMap();}
QVariantList values(const std::vector<double> &v,const std::vector<bool> *valid=nullptr) {
    QVariantList out; out.reserve(qsizetype(v.size()));
    for(std::size_t i=0;i<v.size();++i) out.append(valid && !(*valid)[i] ? QVariant() : nullable(v[i]));
    return out;
}
}
void LanXiWorker::resetData() {
    m_dataSettings=m_settings;
    m_snapshot.clear(); m_paused=false;m_qualityEvents.clear();m_workbench.clear();m_processed.clear();
    for(auto &d:m_devices) {
        d.decoder.reset(); d.hasOrigin=false; d.receiving=false; d.lastDataMs=m_clock.elapsed(); d.blockCount=0;
        d.written=0; d.segments.clear();d.physical.clear();d.setup={};
        const auto a=m_settings.value("analysis").toMap();
        for(auto &c:d.channels) {c=Channel{};c.average=std::make_unique<lanxi::SpectralAverage>(m_fft,
            a.value("window","hann").toString().toStdString(),a.value("averaging").toString()=="exponential"?a.value("averages",8).toInt():0,a.value("removeMean").toBool());}
    }
}
void LanXiWorker::consume(int index) {
    auto &d=m_devices[index]; if(!d.socket)return;
    try {
        // Bound each callback's work so network control and Stop remain responsive.
        for(int batch=0;batch<4 && d.socket && d.socket->bytesAvailable()>0;++batch) {
            const auto bytes=d.socket->read(65536); if(bytes.isEmpty())break;
            writeRaw(index,bytes);
            d.decoder.feed(bytes.constData(),static_cast<std::size_t>(bytes.size()),[this,index](const lanxi::Event &e){accept(index,e);});
        }
        if(d.socket && d.socket->bytesAvailable()>0) QTimer::singleShot(0,this,[this,index]{consume(index);});
        acquisitionReady();
    }catch(const std::exception &e){fail(d.label+": "+QString::fromUtf8(e.what()));}
}
void LanXiWorker::accept(int index,const lanxi::Event &e) {
    auto &d=m_devices[index];
    if(e.signal<1 || std::size_t(e.signal)>d.physical.size()) throw std::runtime_error("Signal ID does not match the enabled physical channel map.");
    auto &c=d.channels[std::size_t(e.signal-1)];
    if(e.kind==lanxi::Event::Kind::Quality) {
        c.quality=e.quality;
        m_qualityEvents.append(QVariantMap{{"device",index},{"channel",d.physical[std::size_t(e.signal-1)]},
            {"flags",int(e.quality)},{"timestampCount",QString::number(qulonglong(e.time.count))}});
        if(m_qualityEvents.size()>2000)m_qualityEvents.removeFirst();
        for(auto &other:d.channels) {if(other.average)other.average->reset(); other.stableSeconds=0;other.pending.clear();other.latest.clear();
            other.analysisConfigured=false;other.spectrogram.clear();other.spectrogramTimes.clear();other.history.clear();}
        if(e.quality&16)throw std::runtime_error("The module reported lost measurement samples.");
        return;
    }
    if(e.kind==lanxi::Event::Kind::Interpretation) {
        if(c.total && (c.descriptor.scale!=e.descriptor.scale || c.descriptor.offset!=e.descriptor.offset
            || c.descriptor.unit!=e.descriptor.unit || c.descriptor.sampleRate()!=e.descriptor.sampleRate())) throw std::runtime_error("Interpretation changed during a measurement.");
        c.descriptor=e.descriptor; return;
    }
    const double rate=e.descriptor.sampleRate(); const int physical=d.physical[std::size_t(e.signal-1)];
    const auto config=dc(m_settings,index); const auto channel=config.value("channels").toList()[physical-1].toMap();
    if(std::abs(rate-config.value("sampleRate").toDouble())>1e-6 || QString::fromStdString(e.descriptor.unit)!=channel.value("unit").toString())
        throw std::runtime_error("Stream rate or engineering unit does not match the accepted configuration.");
    if(!d.hasOrigin){d.origin=e.time;d.hasOrigin=true;}
    const long double coordinate=e.time.since(d.origin)*rate;
    const auto sampleIndex=static_cast<qint64>(std::llround(coordinate));
    if(std::abs(coordinate-sampleIndex)>0.25L)throw std::runtime_error("Sample timestamps do not lie on the module's sample grid.");
    if(c.total) {
        const auto elapsed=e.time.since(c.first)*rate;
        if(std::abs(elapsed-static_cast<long double>(c.total))>0.25L)throw std::runtime_error("Sample gap, duplicate or out-of-order data detected.");
    }else {c.first=e.time;c.firstIndex=sampleIndex;}
    if(c.pending.empty())c.pendingIndex=sampleIndex;
    c.descriptor=e.descriptor;c.last=e.time;c.total+=e.samples.size();
    if(!c.analysisConfigured) {
        c.triggerOrigin=c.total-e.samples.size();
        if(rate>2000)c.levels.configure(rate);
        c.filters.clear();const auto a=m_settings.value("analysis").toMap();
        if(a.value("highPassHz").toDouble()>0)c.filters.push_back(lanxi::Biquad::pass(rate,a.value("highPassHz").toDouble(),true));
        if(a.value("lowPassHz").toDouble()>0)c.filters.push_back(lanxi::Biquad::pass(rate,a.value("lowPassHz").toDouble(),false));
        const auto t=m_settings.value("trigger").toMap();
        c.trigger.configure({t.value("enabled").toBool()&&t.value("device").toInt()==index&&t.value("channel").toInt()==physical,
            t.value("rising",true).toBool(),t.value("level",0.0).toDouble(),t.value("hysteresis",0.01).toDouble(),
            t.value("preSeconds",0.1).toDouble(),t.value("postSeconds",0.5).toDouble()},rate);
        c.analysisConfigured=true;
    }
    if(!c.quality) {if(rate>2000)c.levels.add(e.samples);c.trigger.add(e.samples);}
    c.pending.insert(c.pending.end(),e.samples.begin(),e.samples.end());
    c.preview.insert(c.preview.end(),e.samples.begin(),e.samples.end());
    while(c.preview.size()>8192)c.preview.pop_front();
    if(c.pending.size()>262144)throw std::runtime_error("Unmatched channel buffer exceeded its limit; acquisition stopped.");
    d.receiving=true;d.lastDataMs=m_clock.elapsed();processBlocks(index);
}
void LanXiWorker::processBlocks(int index) {
    auto &d=m_devices[index];const auto count=d.physical.size();if(!count)return;
    while(true) {
        qint64 first=std::numeric_limits<qint64>::min();
        for(std::size_t i=0;i<count;++i){if(d.channels[i].pending.empty())return;first=std::max(first,d.channels[i].pendingIndex);}
        for(std::size_t i=0;i<count;++i) {
            auto &c=d.channels[i];
            while(!c.pending.empty() && c.pendingIndex<first){c.pending.pop_front();++c.pendingIndex;}
            if(c.pending.size()<m_fft)return;
        }
        std::vector<std::vector<double>> blocks(count);
        for(std::size_t i=0;i<count;++i) {
            auto &c=d.channels[i]; blocks[i].assign(c.pending.begin(),c.pending.begin()+m_fft);
            c.pending.erase(c.pending.begin(),c.pending.begin()+m_fft);c.pendingIndex+=qint64(m_fft);
        }
        const int reference=dc(m_settings,index).value("reference").toInt();
        const auto ref=std::find(d.physical.begin(),d.physical.end(),reference);
        const int referenceIndex=ref==d.physical.end() ? -1 : int(ref-d.physical.begin());
        auto processed=blocks;
        for(std::size_t i=0;i<count;++i)for(auto &v:processed[i])for(auto &filter:d.channels[i].filters)v=filter.process(v);
        for(std::size_t i=0;i<count;++i) {
            auto &c=d.channels[i]; const auto &x=processed[referenceIndex<0 ? i : std::size_t(referenceIndex)];
            if(c.quality || (referenceIndex>=0 && d.channels[std::size_t(referenceIndex)].quality)) {c.average->reset();c.stableSeconds=0;c.latest.clear();continue;}
            c.average->add(x,processed[i],c.descriptor.sampleRate());c.latest=blocks[i];c.recentRms=lanxi::rms(blocks[i]);
            c.history.insert(c.history.end(),blocks[i].begin(),blocks[i].end());c.historyEnd=c.pendingIndex;
            while(c.history.size()>262144)c.history.pop_front();
            const auto a=m_settings.value("analysis").toMap();
            lanxi::SpectralAverage instant(m_fft,a.value("window","hann").toString().toStdString(),0,a.value("removeMean").toBool());
            instant.add(processed[i],processed[i],c.descriptor.sampleRate());const auto frame=instant.spectrum();
            std::vector<double> row;const std::size_t bins=std::min<std::size_t>(256,frame.psd.size());
            for(std::size_t k=0;k<bins;++k) {
                const auto start=k*frame.psd.size()/bins,end=(k+1)*frame.psd.size()/bins;
                double power=0;for(auto b=start;b<end;++b)power+=frame.psd[b];row.push_back(power/(end-start));
            }
            c.spectrogram.push_back(std::move(row));c.spectrogramTimes.push_back(double(c.pendingIndex-c.firstIndex)/c.descriptor.sampleRate());
            while(c.spectrogram.size()>96){c.spectrogram.pop_front();c.spectrogramTimes.pop_front();}
            const auto spectrum=c.average->spectrum();c.peakHz=spectrum.peakHz;
            const double db=c.recentRms>0 ? 20*std::log10(c.recentRms) : -1000;
            c.stableSeconds=std::abs(db-c.previousDb)<=0.2 ? c.stableSeconds+m_fft/c.descriptor.sampleRate() : 0;
            c.previousDb=db;
        }
        ++d.blockCount;
    }
}
QVariantList LanXiWorker::channelData() const {
    QVariantList rows;
    for(int d=0;d<2;++d)for(std::size_t i=0;i<m_devices[d].physical.size();++i) {
        const auto &c=m_devices[d].channels[i];const int physical=m_devices[d].physical[i];
        const auto ch=dc(m_dataSettings,d).value("channels").toList()[physical-1].toMap();
        rows.append(QVariantMap{{"device",d},{"channel",physical},{"name",ch.value("name")},
            {"unit",QString::fromStdString(c.descriptor.unit)},{"samples",QString::number(qulonglong(c.total))},
            {"rms",c.latest.empty()?QVariant():nullable(c.recentRms)},
            {"spl",c.descriptor.unit=="Pa"&&!c.latest.empty()?nullable(lanxi::spl(c.recentRms)):QVariant()},
            {"quality",int(c.quality)},{"averages",QString::number(qulonglong(c.average ? c.average->averages():0))},
            {"rate",c.descriptor.ready()?c.descriptor.sampleRate():0},{"reference",physical==dc(m_dataSettings,d).value("reference").toInt()}});
    }
    return rows;
}
QVariantMap LanXiWorker::plotData() const {
    const auto &d=m_devices[m_selectedDevice];
    const auto found=std::find(d.physical.begin(),d.physical.end(),m_selectedChannel);
    if(found==d.physical.end())return {};
    const auto &c=d.channels[std::size_t(found-d.physical.begin())]; if(!c.total || !c.descriptor.ready())return {};
    QVariantList wave;const std::size_t bins=std::min<std::size_t>(512,c.preview.size());
    double extent=1e-12;
    for(std::size_t i=0;i<bins;++i) {
        const auto first=i*c.preview.size()/bins,last=(i+1)*c.preview.size()/bins;
        auto limits=std::minmax_element(c.preview.begin()+first,c.preview.begin()+last);
        wave.append(*limits.first);wave.append(*limits.second);extent=std::max({extent,std::abs(*limits.first),std::abs(*limits.second)});
    }
    const auto spectrum=c.average->spectrum();const auto response=c.average->response(m_referenceFloor);
    const auto unit=QString::fromStdString(c.descriptor.unit);
    std::vector<double> db;for(double v:spectrum.rmsBins)db.push_back(v>0?20*std::log10(v/(unit=="Pa"?0.00002:1.0)):-std::numeric_limits<double>::infinity());
    const int reference=dc(m_dataSettings,m_selectedDevice).value("reference").toInt();
    QString responseUnit;
    if(reference>0)responseUnit=unit+"/"+dc(m_dataSettings,m_selectedDevice).value("channels").toList()[reference-1].toMap().value("unit").toString();
    std::vector<double> gainDb;for(double v:response.magnitude)gainDb.push_back(v>0?20*std::log10(v):-std::numeric_limits<double>::infinity());
    return {{"analysis",analysisData(c)},{"wave",wave},{"waveExtent",extent*1.1},{"waveStart",double(c.total-c.preview.size())/c.descriptor.sampleRate()},
        {"waveStep",wave.size()>1 ? double(c.preview.size()-1)/c.descriptor.sampleRate()/(wave.size()-1):0},
        {"spectrum",values(db)},{"psd",values(spectrum.psd)},
        {"frequencyStep",c.descriptor.sampleRate()/m_fft},{"gain",reference>0?values(gainDb,&response.valid):QVariantList{}},
        {"magnitude",reference>0?values(response.magnitude,&response.valid):QVariantList{}},
        {"phase",reference>0?values(response.phase,&response.valid):QVariantList{}},
        {"coherence",reference>0?values(response.coherence,&response.valid):QVariantList{}},
        {"unit",unit},{"responseUnit",responseUnit},{"reference",reference},{"quality",int(c.quality)},
        {"device",m_selectedDevice},{"channel",m_selectedChannel},{"averages",QString::number(qulonglong(c.average->averages()))}};
}
void LanXiWorker::publish(bool plot) {
    if(plot && !m_paused)m_snapshot=plotData();
    if(!m_owner || m_publishPending->exchange(true))return;
    QVariantList devices;
    for(const auto &d:m_devices)devices.append(QVariantMap{{"label",d.label},{"url",d.url},{"serial",d.serial},{"state",d.state},{"info",d.info.toVariantMap()}});
    QVariantMap state{{"commandId",m_commandId},{"busy",m_busy||m_batch||m_reporting||m_sequenceActive},{"ready",m_ready},{"phase",m_batch?"batch":m_reporting?"reporting":m_phase},{"message",m_message},{"output",m_output},
        {"devices",devices},{"channels",channelData()},{"plot",m_snapshot},{"runs",m_runs},{"profiles",m_profiles},
        {"comparison",m_comparison},{"reportPath",m_reportPath},{"discovered",m_discovered},{"sensors",m_sensors},{"transducers",m_transducers},
        {"batchResults",m_batchResults},{"workbench",m_workbench},{"workspace",m_workspace},
        {"interchange",m_interchange},{"recordResult",m_recordResult},{"modalResult",m_modalResult},{"acousticResult",m_acousticResult},
        {"sequenceResults",m_sequenceResults},{"sequenceRemaining",m_sequenceRemaining},{"sequenceId",m_sequenceId},
        {"runId",m_runId},{"runPath",m_runPath},{"elapsed",m_busy&&!m_review?(m_clock.elapsed()-m_startedMs)/1000.0:0},
        {"paused",m_paused},{"recording",m_busy&&m_record&&!m_review&&!m_stopping&&(m_phase=="starting"||m_phase=="running")},{"steps",m_steps},{"qualityEvents",m_qualityEvents},
        {"frequency",m_currentFrequency},{"fftSize",int(m_fft)},{"review",m_review}};
    QMetaObject::invokeMethod(m_owner,[owner=m_owner,state,pending=m_publishPending]{if(owner)owner->receive(state);pending->store(false);});
}
void LanXiWorker::tick() {
    try {
        const auto now=m_clock.elapsed();
        if(m_busy && !m_stopping && !m_review && (m_phase=="starting" || m_phase=="running")) {
            if(m_phase=="starting" && now-m_startedMs>30000){fail("Paired acquisition did not become ready within 30 seconds.");return;}
            if(m_phase=="running") {
                for(const auto &d:m_devices)if(now-d.lastDataMs>5000){fail(d.label+": no measurement samples for five seconds.");return;}
                const double duration=m_settings.value("durationSeconds").toDouble();
                const auto g=m_settings.value("generator").toMap();
                if(duration>0 && (now-m_startedMs)/1000.0>=duration && g.value("mode").toString()!="steps"){stop();return;}
                if(m_generator && !m_generatorCommand && g.value("mode").toString()=="steps" && !m_devices[1].reply) {
                    const double elapsed=(now-m_stepMs)/1000.0;
                    if(!m_stepCollecting && elapsed>=g.value("settleSeconds").toDouble()) {
                        m_stepCollecting=true;
                        for(int i=0;i<2;++i){m_stepBlocks[i]=m_devices[i].blockCount;for(auto &c:m_devices[i].channels){c.pending.clear();c.latest.clear();c.average->reset();c.stableSeconds=0;}}
                    }
                    bool enough=m_stepCollecting;
                    for(int i=0;i<2;++i)enough &= m_devices[i].blockCount-m_stepBlocks[i]>=2
                        && (m_devices[i].blockCount-m_stepBlocks[i])*double(m_fft)/dc(m_settings,i).value("sampleRate").toDouble()>=g.value("dwellSeconds").toDouble();
                    if(enough) {
                        QVariantList stepResults;bool valid=true;
                        for(int i=0;i<2;++i) {
                            const auto &d=m_devices[i];const int ref=dc(m_settings,i).value("reference").toInt();
                            const auto it=std::find(d.physical.begin(),d.physical.end(),ref);if(it==d.physical.end()){valid=false;break;}
                            const auto &reference=d.channels[std::size_t(it-d.physical.begin())];
                            if(reference.latest.size()!=m_fft||reference.quality){valid=false;break;}
                            const auto x=lanxi::fitTone(reference.latest,reference.descriptor.sampleRate(),m_currentFrequency);
                            if(x.rms<m_referenceFloor || x.residualRms>x.rms*0.2 || reference.stableSeconds<=0){valid=false;break;}
                            for(std::size_t ch=0;ch<d.physical.size();++ch)if(d.physical[ch]!=ref) {
                                const auto &c=d.channels[ch];if(c.latest.size()!=m_fft||c.quality){valid=false;break;}
                                const auto y=lanxi::fitTone(c.latest,c.descriptor.sampleRate(),m_currentFrequency);const auto h=y.phasor/x.phasor;
                                stepResults.append(QVariantMap{{"device",i},{"channel",d.physical[ch]},{"reference",ref},{"frequencyHz",m_currentFrequency},
                                    {"magnitude",std::abs(h)},{"phase",std::abs(h)>0?nullable(std::arg(h)*180/3.14159265358979323846):QVariant()},{"referenceRms",x.rms},
                                    {"residualRms",y.residualRms},{"unit",QString::fromStdString(c.descriptor.unit+"/"+reference.descriptor.unit)}});
                            }
                        }
                        if(valid) {
                            m_steps.append(stepResults);++m_step;
                            if(std::size_t(m_step)>=m_frequencies.size()){stop();return;}
                            m_generatorCommand=true;m_currentFrequency=m_frequencies[std::size_t(m_step)];
                            setGenerator(m_currentFrequency,g.value("levelVp").toDouble(),[this]{m_stepMs=m_clock.elapsed();m_stepCollecting=false;m_generatorCommand=false;});
                        }
                    }
                    if(elapsed>g.value("settleSeconds").toDouble()+g.value("dwellSeconds").toDouble()+10){fail("Stepped sine could not validate settled reference data from both modules.");return;}
                }
            }
            if(now-m_checkpointMs>5000){m_checkpointMs=now;writeManifest("in-progress");for(auto &d:m_devices)if(d.raw.isOpen()&&!d.raw.flush())throw std::runtime_error("Recording flush failed.");}
        }
        publish(m_phase=="running");
    }catch(const std::exception &e){fail(QString::fromUtf8(e.what()));}
}
void LanXiWorker::calibrate(int device,int physical,double levelDb,double frequency) {
    if(m_busy || device<0 || device>1 || physical<1 || physical>4)return;
    try {
        if(m_settings.value("workflow").toString()=="amplifier")throw std::runtime_error("Select an acoustic workflow for microphone calibration.");
        if(!std::isfinite(levelDb)||levelDb<40||levelDb>150)throw std::runtime_error("Calibrator level must be between 40 and 150 dB SPL.");
        const auto &d=m_devices[device];const auto it=std::find(d.physical.begin(),d.physical.end(),physical);
        if(it==d.physical.end())throw std::runtime_error("Capture this channel with the calibrator first.");
        const auto &c=d.channels[std::size_t(it-d.physical.begin())];
        if(c.descriptor.unit!="V"&&c.descriptor.unit!="Pa")throw std::runtime_error("Microphone calibration requires voltage or pressure input; use the sensor library for force and motion sensitivity.");
        if(c.quality || c.latest.empty() || c.stableSeconds<2)throw std::runtime_error("Calibration requires at least two seconds of stable, valid samples.");
        const auto tone=lanxi::fitTone(c.latest,c.descriptor.sampleRate(),frequency);
        if(tone.rms<=0 || tone.residualRms>tone.rms*0.05)throw std::runtime_error("The measured signal does not match the calibrator tone.");
        auto devices=m_settings.value("devices").toList();auto config=devices[device].toMap();auto channels=config.value("channels").toList();auto ch=channels[physical-1].toMap();
        const double target=0.00002*std::pow(10,levelDb/20);
        const double volts=c.descriptor.unit=="V" ? tone.rms : tone.rms*ch.value("sensitivity").toDouble();
        ch["sensitivity"]=volts/target;ch["unit"]="Pa";ch["calibrationLevelDb"]=levelDb;ch["calibrationHz"]=frequency;
        ch["calibrationUtc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        channels[physical-1]=ch;config["channels"]=channels;devices[device]=config;m_settings["devices"]=devices;
        m_message="Sensitivity calculated from the captured calibrator signal. Save the profile and verify with a new capture.";
        if(m_owner)QMetaObject::invokeMethod(m_owner,[owner=m_owner,settings=m_settings]{if(owner){owner->m_settings=settings;emit owner->settingsChanged();}});
    }catch(const std::exception &e){m_message=QString::fromUtf8(e.what());}
    publish();
}
