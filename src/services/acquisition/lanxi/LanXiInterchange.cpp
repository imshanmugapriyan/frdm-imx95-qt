#include "LanXiWorker.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <istream>
#include <streambuf>
#include <stdexcept>

namespace {
void need(bool ok,const QString &s){if(!ok)throw std::runtime_error(s.toStdString());}
constexpr double pi=3.14159265358979323846;
class FileBuffer:public std::streambuf {
public:
    explicit FileBuffer(QFile &file):m_file(file){}
    QByteArray digest()const{return m_hash.result().toHex();}
protected:
    int_type underflow() override {if(gptr()<egptr())return traits_type::to_int_type(*gptr());auto n=m_file.read(m_buffer,sizeof(m_buffer));need(n>=0,"UFF read failed.");if(!n)return traits_type::eof();m_hash.addData(QByteArrayView(m_buffer,n));setg(m_buffer,m_buffer,m_buffer+n);return traits_type::to_int_type(*gptr());}
private:
    QFile &m_file;char m_buffer[65536];QCryptographicHash m_hash{QCryptographicHash::Sha256};
};
QVariantMap describe(const lanxi::UffFunction &f,int index){return {{"index",index},{"name",QString::fromStdString(f.identification[0])},{"type",f.type},{"id",f.id},{"samples",int(f.y.size())},
    {"complex",f.complex},{"even",f.even},{"responseNode",f.responseNode},{"responseDirection",f.responseDirection},{"referenceNode",f.referenceNode},{"referenceDirection",f.referenceDirection},
    {"responseEntity",QString::fromStdString(f.responseEntity)},{"referenceEntity",QString::fromStdString(f.referenceEntity)},
    {"xUnit",QString::fromStdString(f.axes[0].unit)},{"numeratorUnit",QString::fromStdString(f.axes[1].unit)},{"denominatorUnit",QString::fromStdString(f.axes[2].unit)},
    {"start",f.x.front()},{"end",f.x.back()}};}
}
void LanXiWorker::importUff(const QString &value) {
    if(m_busy)return;m_busy=true;m_phase="importing";publish();
    try {
        const QUrl url(value);const auto path=url.isLocalFile()?url.toLocalFile():value;QFile file(path);
        need(QFileInfo(path).isAbsolute()&&QFileInfo(path).isFile(),"Choose an existing local UFF file using its absolute path.");
        need(file.size()<=256*1024*1024&&file.open(QIODevice::ReadOnly),"UFF file cannot be opened or exceeds 256 MiB.");
        FileBuffer buffer(file);std::istream input(&buffer);auto imported=lanxi::readUff(input,1048576,[this]{return m_cancel.load();});
        QVariantList catalogue,skipped;for(std::size_t i=0;i<imported.functions.size();++i)catalogue.append(describe(imported.functions[i],int(i)));for(int code:imported.skippedDatasets)skipped.append(code);
        m_imported=std::move(imported);m_interchange={{"path",QFileInfo(file).absoluteFilePath()},{"sha256",QString::fromLatin1(buffer.digest())},{"functions",catalogue},{"skipped",skipped},
            {"nonSiUnitSystem",m_imported.nonSiUnitSystem},{"unitsCode",m_imported.unitsCode},
            {"unitPolicy","Original values, unit labels and DOFs preserved. Unit conversion and coordinate transforms are not applied."}};
        m_message=QString("Imported %1 ASCII UFF functions; skipped %2 other datasets. Original units retained.").arg(catalogue.size()).arg(skipped.size());m_phase="review";m_busy=false;selectUff(0);return;
    }catch(const std::exception &e){m_message=QString::fromUtf8(e.what());m_interchange["error"]=m_message;m_phase="error";}
    m_busy=false;publish();
}
void LanXiWorker::selectUff(int index) {
    if(m_busy)return;
    if(index<0||std::size_t(index)>=m_imported.functions.size()){m_message="Select an imported UFF function.";publish();return;}
    const auto &f=m_imported.functions[std::size_t(index)];auto view=describe(f,index);QVariantList amplitude,phase,x;
    // Decimated display only. All original abscissas/complex values stay in m_imported.
    const auto n=std::min<std::size_t>(4096,f.y.size());for(std::size_t j=0;j<n;++j){const auto k=n==1?0:j*(f.y.size()-1)/(n-1);amplitude.append(f.complex?std::abs(f.y[k]):f.y[k].real());phase.append(f.complex?std::arg(f.y[k])*180/pi:0);x.append(f.x[k]);}
    view["amplitude"]=amplitude;view["phase"]=phase;view["x"]=x;view["previewStride"]=n>1?double(f.y.size()-1)/(n-1):1;
    m_interchange["selected"]=view;m_interchange.remove("error");publish();
}
void LanXiWorker::exportUff(const QVariantMap &options) {
    if(m_busy)return;m_busy=true;QString folder;
    try {
        lanxi::UffFunction f;const QString source=options.value("source","capture").toString();
        if(source=="import") {need(!m_imported.nonSiUnitSystem,"Convert the global non-SI unit system before exporting standalone functions; dropping its scale factors would change their meaning.");const int index=options.value("index",0).toInt();need(index>=0&&std::size_t(index)<m_imported.functions.size(),"Select an imported UFF function.");f=m_imported.functions[std::size_t(index)];}
        else {
            need(source=="capture","Unsupported UFF export source.");const auto &device=m_devices[m_selectedDevice];auto found=std::find(device.physical.begin(),device.physical.end(),m_selectedChannel);
            need(found!=device.physical.end(),"Capture or replay a channel first.");const auto &c=device.channels[std::size_t(found-device.physical.begin())];need(!c.quality&&c.average&&c.average->averages()>0,"No valid averaged function is available.");
            const QString kind=options.value("kind","frf").toString();need(kind=="frf"||kind=="spectrum"||kind=="psd","Select FRF, spectrum or PSD.");
            f.type=kind=="frf"?4:kind=="psd"?9:2;f.complex=kind=="frf";f.even=!f.complex;f.increment=c.descriptor.sampleRate()/m_fft;
            f.identification[0]="Suno "+kind.toStdString();f.identification[1]=m_runId.toStdString();f.axes[0]={18,0,0,0,"Frequency","Hz"};f.axes[1].unit=c.descriptor.unit;f.responseEntity=m_selectedDevice==0?"3676":"3677";
            f.responseNode=options.value("responseNode",m_selectedChannel).toInt();f.responseDirection=options.value("responseDirection",0).toInt();
            if(f.complex){const int ref=m_dataSettings.value("devices").toList()[m_selectedDevice].toMap().value("reference").toInt();auto r=std::find(device.physical.begin(),device.physical.end(),ref);need(r!=device.physical.end(),"FRF export requires a local recorded reference.");
                f.referenceEntity=f.responseEntity;f.referenceNode=options.value("referenceNode",ref).toInt();f.referenceDirection=options.value("referenceDirection",0).toInt();f.axes[2].unit=device.channels[std::size_t(r-device.physical.begin())].descriptor.unit;
                auto h=c.average->response(m_referenceFloor);for(std::size_t k=0;k<h.magnitude.size();++k)if(h.valid[k]){f.x.push_back(k*f.increment);f.y.push_back(std::polar(h.magnitude[k],h.phase[k]*pi/180));}
            }else {auto spectrum=c.average->spectrum();const auto &values=kind=="psd"?spectrum.psd:spectrum.rmsBins;if(kind=="psd")f.axes[1].unit+="^2/Hz";for(std::size_t k=0;k<values.size();++k){f.x.push_back(k*f.increment);f.y.emplace_back(values[k],0);}}
        }
        need(!f.y.empty(),"The function has no valid points to export.");f.count=f.y.size();folder=m_root+"/interchange/"+QUuid::createUuid().toString(QUuid::WithoutBraces);need(QDir().mkpath(folder),"Cannot create UFF export directory.");
        QSaveFile output(folder+"/function.uff");need(output.open(QIODevice::WriteOnly),"Cannot create UFF file.");
        lanxi::UffWriter writer(f,[&](const std::string &s){need(!m_cancel.load(),"UFF export cancelled.");need(output.write(s.data(),qint64(s.size()))==qint64(s.size()),"UFF write failed.");});
        for(std::size_t k=0;k<f.y.size();++k)writer.add(f.x[k],f.y[k]);writer.finish();need(output.commit(),"Cannot finalize UFF export.");QString error;
        need(writeJson(folder+"/provenance.json",{{"schema",1},{"source",source},{"runId",source=="capture"?m_runId:QString()},{"importSha256",source=="import"?m_interchange.value("sha256").toString():QString()},
            {"function",QJsonObject::fromVariantMap(describe(f,0))},{"options",QJsonObject::fromVariantMap(options)},{"unitsConverted",false},
            {"acquisitionSettings",source=="capture"?QJsonObject::fromVariantMap(m_dataSettings):QJsonObject{}},{"sourceOutcome",source=="capture"?(m_review?m_reviewOutcome:m_failure.isEmpty()?"complete":"partial"):QString("imported")}},error),error);
        m_interchange["exportPath"]=folder+"/function.uff";m_message="UFF function exported to "+folder;m_phase="review";
    }catch(const std::exception &e){m_message=QString::fromUtf8(e.what());m_phase="error";}
    m_busy=false;publish();
}
