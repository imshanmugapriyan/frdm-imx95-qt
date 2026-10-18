#include "LanXiWorker.h"
#include "LanXiBackend.h"
#include "LanXiAdvanced.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
QVariant numeric(double v){return std::isfinite(v)?QVariant(v):QVariant();}
QVariantList list(const std::vector<double>&values){QVariantList out;for(double v:values)out.append(numeric(v));return out;}
QVariantList preview(const std::vector<double>&values){
    if(values.size()<=2048)return list(values);QVariantList out;
    for(std::size_t i=0;i<1024;++i){auto bounds=std::minmax_element(values.begin()+i*values.size()/1024,values.begin()+(i+1)*values.size()/1024);out.append(*bounds.first);out.append(*bounds.second);}return out;
}
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
}
void LanXiWorker::saveWorkspace(const QVariantMap &workspace) {
    if(m_busy)return;
    auto object=QJsonObject::fromVariantMap(workspace);object["schema"]=1;
    if(QJsonDocument(object).toJson().size()>262144){m_message="Workspace settings exceed 256 KiB.";publish();return;}
    QString error;
    if(writeJson(m_root+"/workspace.json",object,error)){m_workspace=object.toVariantMap();m_message="Workbench layout, processing recipe and project metadata saved.";}
    else m_message=error;publish();
}
void LanXiWorker::audition(double fullScale,double volume,quint64 generation) {
    if(m_busy||!m_owner)return;const auto &device=m_devices[m_selectedDevice];auto found=std::find(device.physical.begin(),device.physical.end(),m_selectedChannel);
    if(found==device.physical.end()){m_message="Select a captured or replayed channel before listening.";publish();return;}
    const auto &channel=device.channels[std::size_t(found-device.physical.begin())];if(channel.quality||channel.history.empty()){m_message="Audio requires a valid captured sample window.";publish();return;}
    std::vector<double> samples(channel.history.begin(),channel.history.end());
    QMetaObject::invokeMethod(m_owner,[owner=m_owner,samples=std::move(samples),rate=channel.descriptor.sampleRate(),fullScale,volume,generation]{if(owner&&!owner->busy()&&owner->m_audioGeneration==generation)owner->m_audio.play(samples,rate,fullScale,volume);});
}
void LanXiWorker::analyze(const QVariantMap &options) {
    if(m_busy)return;m_workbench.clear();m_processed.clear();m_busy=true;m_phase="processing";publish();
    try {
        const auto &device=m_devices[m_selectedDevice];
        auto selected=std::find(device.physical.begin(),device.physical.end(),m_selectedChannel);
        require(selected!=device.physical.end(),"Select a recorded channel first.");
        const auto &channel=device.channels[std::size_t(selected-device.physical.begin())];
        require(channel.quality==0&&channel.history.size()>=32,"A valid contiguous measurement window is required. Capture or replay a session first.");
        const double rate=channel.descriptor.sampleRate();const auto operation=options.value("operation","processing").toString();
        const int requested=options.value("windowSamples",262144).toInt();require(requested>=32&&requested<=262144,"Analysis window must be 32..262144 samples.");
        const auto size=std::min(channel.history.size(),std::size_t(requested));
        std::vector<double> samples(channel.history.end()-size,channel.history.end());
        QVariantMap result{{"schema",1},{"operation",operation},{"runId",m_runId},{"device",m_selectedDevice},{"channel",m_selectedChannel},
            {"serial",device.serial},{"unit",QString::fromStdString(channel.descriptor.unit)},{"sampleRate",rate},{"samples",int(size)},
            {"startSample",QString::number(channel.historyEnd-qint64(size))},{"duration",size/rate},
            {"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"options",options},
            {"scope","The final contiguous analysis window; raw recordings are unchanged."}};
        result["channelSettings"]=m_dataSettings.value("devices").toList()[m_selectedDevice].toMap().value("channels").toList()[m_selectedChannel-1];
        result["acquisitionAnalysis"]=m_dataSettings.value("analysis");
        result["sourceOutcome"]=m_review?m_reviewOutcome:m_failure.isEmpty()?"complete":"partial";
        auto localChannel=[&](int physical)->const Channel& {
            auto found=std::find(device.physical.begin(),device.physical.end(),physical);require(found!=device.physical.end(),"The reference/tachometer channel was not recorded on this module.");
            const auto &other=device.channels[std::size_t(found-device.physical.begin())];
            require(!other.quality&&other.historyEnd==channel.historyEnd&&other.history.size()>=size,"Reference and response must have aligned, valid sample windows.");return other;
        };
        if(operation=="processing") {
            std::vector<lanxi::ProcessStage> stages;
            for(const auto &value:options.value("stages").toList()){auto stage=value.toMap();stages.push_back({stage.value("type").toString().toStdString(),stage.value("value").toDouble()});}
            m_processed=lanxi::processSignal(samples,rate,stages);auto reduced=preview(m_processed);result["wave"]=reduced;
            result["waveStep"]=double(size-1)/(rate*(reduced.size()-1));result["rms"]=lanxi::rms(m_processed);
            result["processedUnit"]=options.value("outputUnit",result.value("unit"));
            std::size_t n=32;while(n*2<=std::min<std::size_t>(32768,size))n*=2;
            lanxi::SpectralAverage average(n,"hann",0,false);
            for(std::size_t first=0;first+n<=size;first+=n){std::vector<double> block(m_processed.begin()+first,m_processed.begin()+first+n);average.add(block,block,rate);}
            result["spectrum"]=list(average.spectrum().rmsBins);result["frequencyStep"]=rate/n;
            const int ref=options.value("referenceChannel",0).toInt();
            if(ref>0){const auto &other=localChannel(ref);std::vector<double> reference(other.history.end()-size,other.history.end());
                auto c=lanxi::correlate(reference,m_processed,rate);const auto graph=preview(c.values);
                result["correlation"]=graph;result["lagStart"]=c.firstLag/rate;result["lagStep"]=double(c.values.size()-1)/(rate*(graph.size()-1));
                result["delaySeconds"]=c.delaySeconds;result["coefficient"]=c.coefficient;}
        } else if(operation=="orders") {
            const int tachChannel=options.value("tachChannel",1).toInt();const auto &other=localChannel(tachChannel);
            std::vector<double> tach(other.history.end()-size,other.history.end());lanxi::OrderSettings setup;
            setup.pulsesPerRevolution=options.value("pulsesPerRevolution",1).toInt();setup.samplesPerRevolution=options.value("samplesPerRevolution",256).toInt();
            setup.revolutionsPerBlock=options.value("revolutionsPerBlock",4).toInt();setup.threshold=options.value("threshold",0).toDouble();
            setup.hysteresis=options.value("hysteresis",.1).toDouble();setup.maximumOrder=options.value("maximumOrder",20).toDouble();
            auto orders=lanxi::trackOrders(samples,tach,rate,setup);QVariantList map,trace;
            int bin=int(std::round(options.value("extractOrder",1).toDouble()/orders.orderStep));require(bin>=0&&bin<int(orders.rms.size()),"Extracted order must lie inside the configured range.");
            for(const auto &row:orders.map){map.append(QVariant(list(row)));trace.append(row[std::size_t(bin)]);}
            result["orders"]=list(orders.rms);result["orderStep"]=orders.orderStep;result["map"]=map;result["rpm"]=list(orders.rpm);result["times"]=list(orders.seconds);
            result["trace"]=trace;result["extractOrder"]=bin*orders.orderStep;result["minimumRpm"]=orders.minimumRpm;result["maximumRpm"]=orders.maximumRpm;
            result["pulses"]=orders.pulses;result["blocks"]=orders.blocks;result["tachChannel"]=tachChannel;
        } else if(operation=="structural") {
            const auto config=m_dataSettings.value("devices").toList()[m_selectedDevice].toMap();require(config.value("reference").toInt()>0,"Structural response requires a local reference.");
            const auto response=channel.average->response(m_referenceFloor);const double df=rate/m_fft;
            const double low=options.value("lowerHz",80).toDouble(),high=options.value("upperHz",120).toDouble(),coherence=options.value("coherenceFloor",.8).toDouble();
            require(std::isfinite(low)&&std::isfinite(high)&&low>0&&high>low&&high<rate/2.56&&coherence>=0&&coherence<=1,"Invalid fit frequency range or coherence floor.");
            const auto kind=options.value("responseKind","receptance").toString();require(kind=="receptance"||kind=="mobility"||kind=="accelerance","Choose receptance, mobility or accelerance.");
            const auto &force=localChannel(config.value("reference").toInt());
            require(force.descriptor.unit=="N","Structural fitting requires a calibrated force reference in N.");
            require(channel.descriptor.unit==(kind=="accelerance"?"m/s2":kind=="mobility"?"m/s":"m"),"Response engineering units must match the selected structural quantity.");
            std::vector<double> frequencies;std::vector<std::complex<double>> values;
            for(std::size_t k=1;k<response.magnitude.size();++k)if(k*df>=low&&k*df<=high&&response.valid[k]&&response.coherence[k]>=coherence){
                auto value=std::polar(response.magnitude[k],response.phase[k]*pi/180);const double omega=2*pi*k*df;
                if(kind=="mobility")value/=std::complex<double>(0,omega);else if(kind=="accelerance")value/=-omega*omega;
                frequencies.push_back(k*df);values.push_back(value);}
            require(frequencies.size()<=2048,"Narrow the fit range to at most 2048 valid bins.");
            auto fitted=lanxi::fitReceptance(frequencies,values);QVariantList table,shape,measuredPlot,fittedPlot;
            for(std::size_t k=0;k<frequencies.size();++k)table.append(QVariantMap{{"hz",frequencies[k]},{"measured",std::abs(values[k])},{"fitted",std::abs(fitted.fitted[k])}});
            std::size_t validIndex=0;
            for(auto bin=std::size_t(std::llround(frequencies.front()/df));bin<=std::size_t(std::llround(frequencies.back()/df));++bin){
                if(validIndex<frequencies.size()&&std::abs(frequencies[validIndex]-bin*df)<df*.1){measuredPlot.append(std::abs(values[validIndex]));fittedPlot.append(std::abs(fitted.fitted[validIndex++]));}
                else {measuredPlot.append(QVariant());fittedPlot.append(QVariant());}}
            const auto frequencyBin=std::size_t(std::llround(fitted.frequency/df));double largest=0;
            for(std::size_t i=0;i<device.physical.size();++i){const auto &c=device.channels[i];if(c.quality||!c.average||c.descriptor.unit!=channel.descriptor.unit)continue;auto h=c.average->response(m_referenceFloor);
                if(frequencyBin<h.valid.size()&&h.valid[frequencyBin]&&h.coherence[frequencyBin]>=coherence){largest=std::max(largest,h.magnitude[frequencyBin]);shape.append(QVariantMap{{"channel",device.physical[i]},{"amplitude",h.magnitude[frequencyBin]},{"phase",h.phase[frequencyBin]}});}}
            for(auto &value:shape){auto row=value.toMap();row["amplitude"]=largest>0?row.value("amplitude").toDouble()/largest:0;value=row;}
            result["frequency"]=fitted.frequency;result["dampingPercent"]=100*fitted.damping;result["relativeError"]=fitted.relativeError;result["frac"]=fitted.assurance;
            result["fit"]=table;result["shape"]=shape;result["method"]="Complex single-pole receptance fit; normalized measured local-reference FRF animation. Use an isolated mode with a calibrated force reference.";
            result["measuredPlot"]=measuredPlot;result["fittedPlot"]=fittedPlot;result["frequencyStart"]=frequencies.front();result["frequencyStep"]=df;
        } else if(operation=="limits") {
            const QString metric=options.value("metric","spectrum").toString();require(metric=="spectrum"||metric=="gain"||metric=="coherence","Unsupported limit metric.");
            const auto current=plotData();std::vector<double> v;for(const auto &x:current.value(metric).toList())v.push_back(x.isValid()&&!x.isNull()?x.toDouble():NAN);
            std::vector<lanxi::LimitPoint> points;for(const auto &value:options.value("limits").toList()){auto row=value.toMap();points.push_back({row.value("hz").toDouble(),row.value("lower").toDouble(),row.value("upper").toDouble()});}
            auto check=lanxi::checkLimits(v,0,current.value("frequencyStep").toDouble(),points);
            result["verdict"]=check.missing||check.evaluated==0?"invalid":check.pass?"pass":"fail";result["evaluated"]=check.evaluated;result["failed"]=check.failed;result["missing"]=check.missing;
            result["worstMargin"]=numeric(check.worstMargin);result["spectrum"]=current.value(metric);result["frequencyStep"]=current.value("frequencyStep");
        } else throw std::runtime_error("Unknown workbench operation.");
        m_workbench=result;m_message="Analysis finished. Results include the source channel, sample range and processing recipe.";m_phase="review";
    }catch(const std::exception &e){m_workbench={{"error",QString::fromUtf8(e.what())}};m_message=QString::fromUtf8(e.what());m_phase="error";}
    m_busy=false;publish();
}
void LanXiWorker::exportAnalysis() {
    if(m_busy)return;
    try {
        require(m_workbench.contains("operation"),"Calculate an analysis result first.");
        const QString folder=m_root+"/analyses/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");require(QDir().mkpath(folder),"Cannot create analysis directory.");
        auto record=QJsonObject::fromVariantMap(m_workbench);record["exportOutcome"]="in-progress";
        QString error;if(!writeJson(folder+"/result.json",record,error))throw std::runtime_error(error.toStdString());
        auto csv=[&](const QString &name,const QByteArray &bytes){QSaveFile f(folder+"/"+name);require(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit(),"Cannot save analysis CSV.");};
        const double rate=m_workbench.value("sampleRate").toDouble();QByteArray data="time_s,processed_value\n";
        if(!m_processed.empty()){for(std::size_t i=0;i<m_processed.size();++i)data+=QByteArray::number(i/rate,'g',17)+','+QByteArray::number(m_processed[i],'g',17)+'\n';csv("processed.csv",data);}
        if(m_workbench.contains("map")){data="time_s,rpm,order,rms\n";auto map=m_workbench.value("map").toList(),rpm=m_workbench.value("rpm").toList(),time=m_workbench.value("times").toList();double step=m_workbench.value("orderStep").toDouble();
            for(qsizetype r=0;r<map.size();++r){auto bins=map[r].toList();for(qsizetype k=0;k<bins.size();++k)data+=QByteArray::number(time[r].toDouble(),'g',17)+','+QByteArray::number(rpm[r].toDouble(),'g',17)+','+QByteArray::number(k*step,'g',17)+','+QByteArray::number(bins[k].toDouble(),'g',17)+'\n';}csv("orders.csv",data);}
        if(m_workbench.contains("fit")){data="frequency_hz,measured_receptance,fitted_receptance\n";for(const auto &value:m_workbench.value("fit").toList()){auto r=value.toMap();data+=QByteArray::number(r.value("hz").toDouble(),'g',17)+','+QByteArray::number(r.value("measured").toDouble(),'g',17)+','+QByteArray::number(r.value("fitted").toDouble(),'g',17)+'\n';}csv("modal-fit.csv",data);}
        record["exportOutcome"]="complete";if(!writeJson(folder+"/result.json",record,error))throw std::runtime_error(error.toStdString());
        m_message="Analysis exported to "+folder;m_workbench["exportPath"]=folder;
    }catch(const std::exception&e){m_message=QString::fromUtf8(e.what());}publish();
}
bool LanXiWorker::finishSequenceStep() {
    if(!m_sequenceActive)return false;
    const bool cancelled=m_cancel.load();QString verdict=m_failure.isEmpty()?"complete":"acquisition-error";
    auto acceptance=m_sequenceOptions.value("acceptance").toMap();QVariantMap result;
    if(!cancelled&&m_failure.isEmpty()&&!acceptance.isEmpty()) {
        const int d=acceptance.value("device",0).toInt(),c=acceptance.value("channel",1).toInt();
        if(d<0||d>1||c<1||c>4)verdict="invalid";
        else {const int previousDevice=m_selectedDevice,previousChannel=m_selectedChannel;m_selectedDevice=d;m_selectedChannel=c;
            acceptance["operation"]="limits";analyze(acceptance);result=m_workbench;verdict=result.value("verdict","invalid").toString();
            m_selectedDevice=previousDevice;m_selectedChannel=previousChannel;}
    }
    if(verdict!="complete"&&verdict!="pass")m_sequenceFailed=true;
    m_sequenceResults.append(QVariantMap{{"iteration",m_sequenceResults.size()+1},{"runId",m_runId},{"verdict",cancelled?"cancelled":verdict},{"result",result},{"error",m_failure}});
    const QString folder=m_root+"/Sequences";QString error;QDir().mkpath(folder);
    const bool more=!cancelled&&m_failure.isEmpty()&&m_sequenceRemaining>0&&(!m_sequenceFailed||!m_sequenceOptions.value("stopOnFail",true).toBool());
    const QJsonObject record{{"schema",1},{"id",m_sequenceId},{"options",QJsonObject::fromVariantMap(m_sequenceOptions)},
        {"results",QJsonArray::fromVariantList(m_sequenceResults)},{"state",more?"in-progress":cancelled?"cancelled":m_sequenceFailed?"failed":"complete"}};
    if(!writeJson(folder+"/"+m_sequenceId+".json",record,error)){m_failure="Cannot save sequence results: "+error;m_sequenceFailed=true;}
    if(more&&m_failure.isEmpty()) {
        --m_sequenceRemaining;m_busy=true;m_phase="sequence-wait";m_message=QString("Sequence: %1 complete; %2 remaining. Both modules are idle during the interval.").arg(m_sequenceResults.size()).arg(m_sequenceRemaining+1);
        refreshRuns();publish(true);const auto generation=m_generation;
        QTimer::singleShot(int(m_sequenceOptions.value("delaySeconds",1).toDouble()*1000),this,[this,generation]{
            if(generation!=m_generation||!m_sequenceActive||m_cancel.load())return;m_busy=false;start(m_ticket,m_settings);
        });return true;
    }
    m_sequenceActive=false;m_sequenceRemaining=0;
    if(m_sequenceFailed&&m_failure.isEmpty())m_failure="Sequence stopped with failed or invalid acceptance results.";
    return false;
}
