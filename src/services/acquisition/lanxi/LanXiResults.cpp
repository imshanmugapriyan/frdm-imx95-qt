#include "LanXiWorker.h"
#include "LanXiBackend.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
QVariant number(double v) {return std::isfinite(v)?QVariant(v):QVariant();}
double db(double v,double reference) {return v>0?20*std::log10(v/reference):-INFINITY;}
QString cell(const QVariant &v) {return v.isValid()?v.toString().toHtmlEscaped():QStringLiteral("unavailable");}
void saveText(const QString &path,const QByteArray &bytes) {
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error(("Cannot write "+path+": "+file.errorString()).toStdString());
}
}
QVariantMap LanXiWorker::analysisData(const Channel &c) const {
    if(!c.average||!c.average->averages()||c.quality)return {};
    const auto a=m_dataSettings.value("analysis").toMap();const auto weighting=a.value("weighting","Z").toString();
    const char w=weighting.at(0).toLatin1();const double reference=c.descriptor.unit=="Pa"?0.00002:1;
    const double rate=c.descriptor.sampleRate();const auto spectrum=c.average->spectrum();
    QVariantMap out{{"weighting",weighting},{"timeWeighting",a.value("timeWeighting","Fast")},
        {"levelUnit",c.descriptor.unit=="Pa"?"dB SPL":"dB re 1 "+QString::fromStdString(c.descriptor.unit)},
        {"filterNote","Levels and harmonic analysis use calibrated samples before analysis filters."}};
    if(rate>2000&&c.levels.seconds()>0) {
        const auto level=c.levels.level(w);
        out["level"]=number(db(a.value("timeWeighting","Fast").toString()=="Fast"?level.fast:level.slow,reference));
        out["leq"]=number(db(level.equivalent,reference));out["maxFast"]=number(db(level.maximumFast,reference));
        out["peak"]=number(db(level.peak,reference));out["seconds"]=c.levels.seconds();
    }
    QVariantList bands,weighted;
    for(const auto &b:lanxi::fractionalBands(spectrum,a.value("fraction",3).toInt(),w))bands.append(QVariantMap{
        {"hz",b.center},{"low",b.low},{"high",b.high},{"resolved",b.resolved},{"level",b.resolved?number(db(b.rms,reference)):QVariant()}});
    for(std::size_t k=0;k<spectrum.rmsBins.size();++k)weighted.append(number(db(spectrum.rmsBins[k]*lanxi::weightingGain(k*rate/m_fft,w),reference)));
    out["bands"]=bands;out["weightedSpectrum"]=weighted;
    QVariantList frames,times;
    for(std::size_t i=0;i<c.spectrogram.size();++i) {
        QVariantList row;
        for(std::size_t k=0;k<c.spectrogram[i].size();++k) {
            const double f=(k+0.5)*rate/(2*c.spectrogram[i].size());const double gain=lanxi::weightingGain(f,w);
            const double power=c.spectrogram[i][k]*gain*gain/(reference*reference);
            row.append(power>0?number(10*std::log10(power)):QVariant());
        }
        frames.append(QVariant(row));times.append(c.spectrogramTimes[i]);
    }
    out["spectrogram"]=frames;out["times"]=times;out["maximumHz"]=rate/2;
    try {
        const auto distortion=lanxi::distortion(c.latest,rate,a.value("toneHz",1000.0).toDouble(),a.value("harmonics",5).toInt());
        QVariantList harmonics;for(double v:distortion.harmonicRms)harmonics.append(v);
        out["distortion"]=QVariantMap{{"fundamentalRms",distortion.fundamental},{"thdPercent",100*distortion.thd},
            {"thdnPercent",100*distortion.thdn},{"sinadDb",number(distortion.sinadDb)},{"residualRms",distortion.residual},
            {"harmonics",harmonics},{"fundamentalHz",a.value("toneHz",1000.0)},
            {"bandwidthHz",rate/2},{"method","Joint harmonic least-squares fit; THD+N residual spans the acquired bandwidth."}};
    }catch(const std::exception &e){out["distortionError"]=QString::fromUtf8(e.what());}
    QVariantList event;const auto &capture=c.trigger.values();
    const std::size_t bins=std::min<std::size_t>(512,capture.size());
    for(std::size_t k=0;k<bins;++k) {const auto limits=std::minmax_element(capture.begin()+k*capture.size()/bins,capture.begin()+(k+1)*capture.size()/bins);event.append(*limits.first);event.append(*limits.second);}
    out["trigger"]=QVariantMap{{"triggered",c.trigger.triggered()},{"complete",c.trigger.complete()},
        {"index",QString::number(qulonglong(c.triggerOrigin+c.trigger.index()))},{"preSamples",int(c.trigger.preSamples())},
        {"samples",int(capture.size())},{"values",event},{"start",-double(c.trigger.preSamples())/rate},
        {"step",event.size()>1?double(capture.size()-1)/rate/(event.size()-1):0}};
    return out;
}
QVariantMap LanXiWorker::resultSnapshot() const {
    QVariantList channels;
    for(int d=0;d<2;++d)for(std::size_t i=0;i<m_devices[d].physical.size();++i) {
        const auto &c=m_devices[d].channels[i];const auto config=m_dataSettings.value("devices").toList()[d].toMap();
        const int physical=m_devices[d].physical[i];auto row=analysisData(c);
        row.remove("spectrogram");row.remove("times");row.remove("weightedSpectrum");
        row["device"]=d;row["channel"]=physical;row["serial"]=m_devices[d].serial;
        row["name"]=config.value("channels").toList()[physical-1].toMap().value("name");
        row["unit"]=QString::fromStdString(c.descriptor.unit);row["quality"]=int(c.quality);
        row["samples"]=QString::number(qulonglong(c.total));channels.append(row);
    }
    return {{"schema",1},{"runId",m_runId},{"name",m_dataSettings.value("name")},{"startedUtc",m_startedUtc},
        {"clockRelationship","independent"},{"settings",m_dataSettings},{"channels",channels},
        {"sourceOutcome",m_reviewOutcome},{"recoveryNotes",m_reviewNotes},{"steps",m_steps},{"qualityEvents",m_qualityEvents}};
}
void LanXiWorker::pinComparison() {
    if(m_busy)return;
    m_comparison=plotData();m_comparison["name"]=m_dataSettings.value("name");m_comparison["runId"]=m_runId;
    m_comparison.remove("analysis");m_comparison.remove("wave");
    m_message=m_comparison.value("spectrum").toList().isEmpty()?"Capture or replay a channel before pinning a comparison.":"Reference curve pinned. Replay another run and select a compatible channel to compare.";
    publish(true);
}
void LanXiWorker::clearComparison() {m_comparison.clear();publish(true);}
void LanXiWorker::reprocess(const QString &id,const QVariantMap &analysis) {if(!m_busy)readArchive(id,{},analysis);}
void LanXiWorker::report(const QString &id,const QVariantMap &options) {
    if(m_busy)return;
    m_reportPath.clear();m_reporting=true;
    readArchive(id,{},options.value("analysis").toMap());
    if(m_phase=="error"){m_reporting=false;publish();return;}
    m_busy=true;m_phase="reporting";publish();
    try {
        if(m_cancel.load())throw std::runtime_error("Report cancelled.");
        auto snapshot=resultSnapshot();snapshot["report"]=options;
        const QString folder=m_runPath+"/reports/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
        if(!QDir().mkpath(folder))throw std::runtime_error("Cannot create report directory.");
        auto record=QJsonObject::fromVariantMap(snapshot);record["reportOutcome"]="in-progress";
        QString error;if(!writeJson(folder+"/results.json",record,error))throw std::runtime_error(error.toStdString());
        QString html="<!doctype html><html lang='en'><meta charset='utf-8'><title>LAN-XI measurement report</title><style>"
            "body{font:16px sans-serif;max-width:1100px;margin:40px auto;color:#202330}table{border-collapse:collapse;width:100%}"
            "th,td{padding:10px;border:1px solid #ccd;text-align:left}pre{white-space:pre-wrap}small{color:#556}@media print{body{margin:0}}</style>";
        html+="<h1>"+cell(options.value("title",m_settings.value("name")))+"</h1><p>Session "+m_runId.toHtmlEscaped()+" | "+m_startedUtc.toHtmlEscaped()+"</p>";
        html+="<p>Operator: "+cell(options.value("operator"))+" | DUT: "+cell(options.value("dut"))+"</p><p>"+cell(options.value("notes"))+"</p>";
        html+="<p>Source outcome: "+m_reviewOutcome.toHtmlEscaped()+". "+m_reviewNotes.toHtmlEscaped()+"</p>";
        html+="<p>Independent module clocks. Levels use calibrated samples; spectra/FRF use the saved analysis filters and window. Engineering results; instrument conformance has not been established.</p>";
        html+="<table><tr><th>Module / channel</th><th>Name</th><th>Weighting</th><th>Level</th><th>Leq</th><th>THD %</th><th>THD+N %</th><th>Quality flags</th></tr>";
        for(const auto &value:snapshot.value("channels").toList()) {
            const auto c=value.toMap(),dist=c.value("distortion").toMap();
            html+="<tr><td>"+QString::number(c.value("device").toInt()==0?3676:3677)+" / "+cell(c.value("channel"))+"</td><td>"+cell(c.value("name"))+"</td><td>"+cell(c.value("weighting"))+" / "+cell(c.value("timeWeighting"))+"</td><td>"+cell(c.value("level"))+" "+cell(c.value("levelUnit"))+"</td><td>"+cell(c.value("leq"))+"</td><td>"+cell(dist.value("thdPercent"))+"</td><td>"+cell(dist.value("thdnPercent"))+"</td><td>"+cell(c.value("quality"))+"</td></tr>";
        }
        html+="</table><h2>Configuration and provenance</h2><pre>"+QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(snapshot)).toJson(QJsonDocument::Indented)).toHtmlEscaped()+"</pre></html>";
        for(int d=0;d<2;++d)for(std::size_t i=0;i<m_devices[d].physical.size();++i) {
            if(m_cancel.load())throw std::runtime_error("Report cancelled; completed files remain in the report directory.");
            const auto &c=m_devices[d].channels[i];if(!c.average||c.quality)continue;
            const auto s=c.average->spectrum();const auto h=c.average->response(m_referenceFloor);
            QByteArray csv="frequency_hz,rms,psd,h1_magnitude,h1_phase_deg,coherence\n";
            const bool localReference=m_settings.value("devices").toList()[d].toMap().value("reference").toInt()>0;
            for(std::size_t k=0;k<s.rmsBins.size();++k) {
                csv+=QByteArray::number(k*s.sampleRate/m_fft,'g',17)+','+QByteArray::number(s.rmsBins[k],'g',17)+','+QByteArray::number(s.psd[k],'g',17)+',';
                if(localReference&&h.valid[k])csv+=QByteArray::number(h.magnitude[k],'g',17)+','+QByteArray::number(h.phase[k],'g',17)+','+QByteArray::number(h.coherence[k],'g',17);
                else csv+=",,";csv+='\n';
            }
            saveText(folder+QString("/%1-ch%2-spectrum.csv").arg(d==0?3676:3677).arg(m_devices[d].physical[i]),csv);
            if(c.trigger.complete()) {
                QByteArray event="time_from_trigger_s,value,unit\n";std::size_t k=0;
                for(double v:c.trigger.values())event+=QByteArray::number((double(k++)-c.trigger.preSamples())/c.descriptor.sampleRate(),'g',17)+','+QByteArray::number(v,'g',17)+','+QByteArray::fromStdString(c.descriptor.unit)+'\n';
                saveText(folder+QString("/%1-ch%2-trigger.csv").arg(d==0?3676:3677).arg(m_devices[d].physical[i]),event);
            }
        }
        saveText(folder+"/report.html",html.toUtf8());record["reportOutcome"]="complete";
        if(!writeJson(folder+"/results.json",record,error))throw std::runtime_error(error.toStdString());
        m_message="Report, analysis CSV files and result metadata saved to "+folder;m_reportPath=folder;
        m_phase="review";
    }catch(const std::exception &e){m_message=QString::fromUtf8(e.what());m_phase="error";}
    m_busy=false;m_reporting=false;publish(true);
}
void LanXiWorker::batchReports(const QVariantList &ids,const QVariantMap &options) {
    if(m_busy)return;
    if(ids.isEmpty()||ids.size()>100){m_message="Select between 1 and 100 sessions for batch reporting.";publish();return;}
    m_batch=true;m_cancel=false;m_batchResults.clear();int failed=0;
    for(const auto &id:ids) {
        if(m_cancel.load())break;
        report(id.toString(),options);const bool success=m_phase!="error";
        if(!success)++failed;
        m_batchResults.append(QVariantMap{{"runId",id},{"success",success},{"message",m_message},{"path",m_reportPath}});
        m_message=QString("Batch: %1 of %2 sessions processed.").arg(m_batchResults.size()).arg(ids.size());publish();
    }
    m_batch=false;m_busy=false;m_phase=m_cancel.load()?"cancelled":failed?"error":"review";
    m_message=QString("Batch %1: %2 of %3 processed, %4 failed. Each successful report is stored with its source recording.")
        .arg(m_cancel.load()?"cancelled":"finished").arg(m_batchResults.size()).arg(ids.size()).arg(failed);publish(true);
}
