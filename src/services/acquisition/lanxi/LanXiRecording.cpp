#include "LanXiWorker.h"
#include "LanXiStreaming.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void need(bool ok,const QString&s){if(!ok)throw std::runtime_error(s.toStdString());}
void put(QSaveFile &f,const QByteArray &bytes){need(f.write(bytes)==bytes.size(),"Recording export write failed: "+f.errorString());}
}
void LanXiWorker::processRecording(const QString &id,const QVariantMap &options) {
    if(m_busy)return;m_busy=true;m_recordResult.clear();m_phase="processing-recording";publish();QString folder;
    try {
        const auto path=archivePath(id);const auto manifest=readJson(path+"/manifest.json");
        need(manifest["schema"].toInt()==1&&manifest["recorded"].toBool()&&manifest["clockRelationship"].toString()=="independent","Unsupported recording.");
        need(manifest["outcome"].toString()=="complete","Full-record processing requires a complete recording. Review/export valid prefixes separately.");
        const int d=options.value("device",m_selectedDevice).toInt(),physical=options.value("channel",m_selectedChannel).toInt();
        const auto devices=manifest["devices"].toArray();need(d>=0&&d<2&&devices.size()==2&&physical>=1&&physical<=4,"Invalid recording channel.");
        const auto device=devices[d].toObject();int signal=0,enabled=0;
        for(const auto &v:device["setup"].toObject()["channels"].toArray())if(v.toObject()["enabled"].toBool()){++enabled;if(v.toObject()["channel"].toInt()==physical)signal=enabled;}
        need(signal>0&&enabled<=4,"Selected channel was not recorded.");
        const auto settings=manifest["settings"].toObject()["devices"].toArray();need(settings.size()==2,"Missing recorded settings.");
        const auto channels=settings[d].toObject()["channels"].toArray();need(channels.size()==4,"Invalid recorded channel settings.");
        const double rate=settings[d].toObject()["sampleRate"].toDouble();const QString unit=channels[physical-1].toObject()["unit"].toString();
        need(std::isfinite(rate)&&rate>0&&!unit.isEmpty(),"Missing sample rate or engineering unit.");
        const auto segments=device["segments"].toArray();need(!segments.isEmpty(),"No recorded stream segments.");
        std::vector<lanxi::ProcessStage> stages;
        for(const auto &v:options.value("stages").toList()){auto s=v.toMap();stages.push_back({s.value("type").toString().toStdString(),s.value("value").toDouble()});}
        // Validate before reading or creating result files.
        lanxi::StreamingProcessor validateRecipe(rate,stages,0);
        const auto scan=[&](const std::function<void(double)> &sample,const QString &phase) {
            lanxi::Decoder decoder;lanxi::Descriptor descriptor;lanxi::Time first;std::uint64_t count=0;std::uint16_t quality=0;
            QCryptographicHash hash(QCryptographicHash::Sha256);qint64 bytesRead=0;QElapsedTimer notify;notify.start();
            for(const auto &v:segments){const auto name=v.toString();need(QRegularExpression(d==0?"^3676-[0-9]{4,}\\.stream$":"^3677-[0-9]{4,}\\.stream$").match(name).hasMatch(),"Invalid recorded segment name.");
                QFile file(path+"/"+name);need(!QFileInfo(file).isSymLink()&&file.open(QIODevice::ReadOnly),"Cannot read recording segment.");
                while(!file.atEnd()){need(!m_cancel.load(),"Recording processing cancelled.");const auto bytes=file.read(65536);need(!bytes.isEmpty(),"Recording read failed.");hash.addData(bytes);bytesRead+=bytes.size();
                    decoder.feed(bytes.constData(),std::size_t(bytes.size()),[&](const lanxi::Event &e){
                        if(e.signal!=signal)return;
                        if(e.kind==lanxi::Event::Kind::Quality){quality=e.quality;need(quality==0,"Selected recording contains overload, invalid data or sample loss; full-record processing refused.");return;}
                        if(e.kind!=lanxi::Event::Kind::Samples)return;
                        need(!quality&&std::abs(e.descriptor.sampleRate()-rate)<1e-6&&QString::fromStdString(e.descriptor.unit)==unit,"Recording metadata or quality changed.");
                        if(count){need(descriptor.scale==e.descriptor.scale&&descriptor.offset==e.descriptor.offset,"Recording scaling changed.");need(std::abs(e.time.since(first)*rate-static_cast<long double>(count))<=.25L,"Recording has gaps, duplicates or out-of-order samples.");}
                        else {first=e.time;descriptor=e.descriptor;}
                        for(double value:e.samples){sample(value);++count;}
                    });
                    if(notify.elapsed()>200){m_message=QString("%1: %2 MiB read").arg(phase).arg(bytesRead/1048576);publish();notify.restart();}
                }
            }
            decoder.finish();need(count>0,"Recorded channel has no samples.");return hash.result().toHex();
        };
        lanxi::RunningStatistics source;const auto digest=scan([&](double v){source.add(v);},"Checking entire recording");
        need(source.count<=9999999999ULL,"Recording exceeds the UFF dataset sample-count field.");
        folder=m_root+"/analyses/"+QUuid::createUuid().toString(QUuid::WithoutBraces);need(QDir().mkpath(folder),"Cannot create recording result directory.");
        auto record=QJsonObject{{"schema",1},{"operation","full-recording"},{"outcome","in-progress"},{"runId",id},{"device",d},{"channel",physical},
            {"serial",device["serial"]},{"sourceManifest",manifest},{"streamSha256",QString::fromLatin1(digest)},{"sampleCount",QString::number(qulonglong(source.count))},
            {"sampleRate",rate},{"duration",double(source.count)/rate},{"unit",unit},{"options",QJsonObject::fromVariantMap(options)},
            {"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};QString error;need(writeJson(folder+"/result.json",record,error),error);
        QSaveFile csv(folder+"/processed.csv"),uff(folder+"/processed.uff");need(csv.open(QIODevice::WriteOnly)&&uff.open(QIODevice::WriteOnly),"Cannot create processed outputs.");put(csv,"sample_index,time_s,value\n");
        lanxi::UffFunction header;header.count=source.count;header.increment=1/rate;header.responseNode=physical;header.responseEntity=d==0?"3676":"3677";
        header.identification[0]="Suno full-recording processing";header.identification[1]=id.toStdString();header.axes[0]={17,0,0,0,"Time","s"};
        const QString outputUnit=options.value("outputUnit",unit).toString().trimmed();need(!outputUnit.isEmpty(),"Specify the processed engineering unit.");header.axes[1].unit=outputUnit.toStdString();header.axes[1].label="Processed signal";
        lanxi::UffWriter writer(header,[&](const std::string &s){put(uff,QByteArray::fromStdString(s));});lanxi::StreamingProcessor processor(rate,stages,double(source.mean));lanxi::RunningStatistics output;
        const std::size_t bins=std::size_t(std::min<std::uint64_t>(1024,source.count));std::vector<double> low(bins,INFINITY),high(bins,-INFINITY),block;block.reserve(4096);
        lanxi::SpectralAverage average(4096,"hann",0,false);QByteArray rows;
        const auto processedDigest=scan([&](double input){const double y=processor.process(input);const auto n=output.count;const auto b=std::min(bins-1,std::size_t(n*bins/source.count));low[b]=std::min(low[b],y);high[b]=std::max(high[b],y);
            rows+=QByteArray::number(qulonglong(n))+','+QByteArray::number(n/rate,'g',17)+','+QByteArray::number(y,'g',17)+'\n';if(rows.size()>=65536){put(csv,rows);rows.clear();}
            writer.add(n/rate,y);output.add(y);block.push_back(y);if(block.size()==4096){average.add(block,block,rate);block.clear();}
        },"Processing entire recording");
        need(digest==processedDigest&&output.count==source.count,"Source recording changed between passes. Result discarded.");need(!m_cancel.load(),"Recording processing cancelled.");
        writer.finish();put(csv,rows);need(csv.commit()&&uff.commit(),"Cannot finalize recording outputs.");
        QVariantList wave,spectrum;for(std::size_t i=0;i<bins;++i){wave.append(low[i]);wave.append(high[i]);}if(average.averages())for(double v:average.spectrum().rmsBins)spectrum.append(v);
        m_recordResult={{"operation","processing"},{"scope","entire-recording"},{"runId",id},{"device",d},{"channel",physical},{"samples",QString::number(qulonglong(output.count))},{"duration",output.count/rate},
            {"unit",outputUnit},{"processedUnit",outputUnit},{"rms",output.rms()},{"peak",output.peak},{"wave",wave},{"waveStep",(output.count-1)/(rate*std::max<qsizetype>(1,wave.size()-1))},
            {"spectrum",spectrum},{"frequencyStep",rate/4096},{"fftBlocks",QString::number(qulonglong(average.averages()))},{"fftTailSamples",int(block.size())},
            {"exportPath",folder},{"streamSha256",QString::fromLatin1(digest)}};
        record["outcome"]="complete";record["result"]=QJsonObject::fromVariantMap(m_recordResult);record["method"]="Two-pass whole-record mean; stateful causal filters, backward difference and leaky bilinear integration. FFT: non-overlapping 4096-sample Hann blocks; tail included in CSV, UFF and RMS, omitted from FFT.";
        need(writeJson(folder+"/result.json",record,error),error);m_message="Entire recording processed: "+folder;m_phase="review";
    }catch(const std::exception &e){m_recordResult={{"error",QString::fromUtf8(e.what())}};m_message=QString::fromUtf8(e.what());m_phase="error";
        if(!folder.isEmpty()){QString ignored;writeJson(folder+"/result.json",{{"outcome",m_cancel.load()?"cancelled":"failed"},{"error",m_message},{"runId",id}},ignored);}}
    m_busy=false;publish();
}
