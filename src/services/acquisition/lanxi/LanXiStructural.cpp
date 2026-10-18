#include "LanXiWorker.h"
#include "LanXiModal.h"
#include "LanXiAdvanced.h"
#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
using C=std::complex<double>;constexpr double pi=3.14159265358979323846;
void need(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
C receptance(C value,double hz,const std::string &unit){const double w=2*pi*hz;if(unit=="m/s2")return value/(-w*w);if(unit=="m/s")return value/C(0,w);need(unit=="m","Structural response must be calibrated in m, m/s or m/s2.");return value;}
QVariantList shape(const std::vector<C> &v,const std::vector<lanxi::UffFunction>&functions){double maximum=0;for(auto x:v)maximum=std::max(maximum,std::abs(x));QVariantList rows;for(std::size_t i=0;i<v.size();++i)rows.append(QVariantMap{{"node",functions[i].responseNode},{"channel",functions[i].responseNode},{"direction",functions[i].responseDirection},
    {"real",v[i].real()},{"imaginary",v[i].imag()},{"amplitude",maximum?std::abs(v[i])/maximum:0},{"phase",std::arg(v[i])*180/pi}});return rows;}
}
void LanXiWorker::structuralAnalysis(const QVariantMap &options) {
    if(m_busy)return;m_busy=true;m_modalResult.clear();m_phase="structural-analysis";publish();
    try {
        const auto source=options.value("source","capture").toString(),operation=options.value("operation","modal").toString();need(source=="capture"||source=="import","Unknown structural data source.");need(operation=="modal"||operation=="ods","Unknown structural operation.");
        const bool imported=source=="import";const bool ods=operation=="ods";
        std::vector<lanxi::UffFunction> functions;const double low=options.value("lowerHz",70).toDouble(),high=options.value("upperHz",150).toDouble();
        const double coherence=options.value("coherenceFloor",.8).toDouble();need(std::isfinite(low)&&std::isfinite(high)&&low>0&&high>low&&std::isfinite(coherence)&&coherence>=0&&coherence<=1,"Invalid structural frequency range or coherence floor.");
        QVariantMap result{{"schema",1},{"operation",ods?"ods":"modal"},{"source",imported?"import":"capture"},{"options",options},{"runId",imported?QString():m_runId},
            {"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        if(imported) {
            need(!ods,"Operating shapes require captured/replayed response spectra and cross phases, not imported FRFs.");
            // Unknown global unit scaling cannot be silently treated as calibrated SI.
            need(!m_imported.nonSiUnitSystem,"Convert the UFF global unit system to SI before structural fitting; implicit global unit conversion is not implemented.");
            const int index=options.value("index",0).toInt();need(index>=0&&std::size_t(index)<m_imported.functions.size(),"Select an imported FRF.");const auto &reference=m_imported.functions[std::size_t(index)];
            need(reference.type==4&&reference.complex&&reference.loadCase==0&&reference.axes[0].unit=="Hz"&&reference.axes[2].unit=="N","Selected function must be a complex calibrated Hz-domain FRF with N reference.");
            for(std::size_t i=0;i<m_imported.functions.size();++i) {
                const auto &f=m_imported.functions[i];if(int(i)!=index&&!options.value("allCompatible",false).toBool())continue;
                if(f.type!=4||!f.complex||f.loadCase!=0||f.referenceNode!=reference.referenceNode||f.referenceDirection!=reference.referenceDirection||f.referenceEntity!=reference.referenceEntity)continue;
                if(f.axes[0].unit!="Hz"||f.axes[2].unit!="N"||(f.axes[1].unit!="m"&&f.axes[1].unit!="m/s"&&f.axes[1].unit!="m/s2"))continue;
                lanxi::UffFunction reduced=f;reduced.x.clear();reduced.y.clear();for(std::size_t k=0;k<f.x.size();++k)if(f.x[k]>=low&&f.x[k]<=high){reduced.x.push_back(f.x[k]);reduced.y.push_back(receptance(f.y[k],f.x[k],f.axes[1].unit));}reduced.axes[1].unit="m";functions.push_back(std::move(reduced));
            }
            need(!functions.empty(),"Select a complex Hz-domain FRF with calibrated m, m/s or m/s2 response and an N reference.");result["importPath"]=m_interchange.value("path");result["importSha256"]=m_interchange.value("sha256");
        }else {
            const auto &device=m_devices[m_selectedDevice];need(m_dataSettings.value("devices").toList().size()==2&&!device.physical.empty(),"Capture or replay motion data first.");const int ref=m_dataSettings.value("devices").toList()[m_selectedDevice].toMap().value("reference").toInt();
            auto refIndex=std::find(device.physical.begin(),device.physical.end(),ref);need(refIndex!=device.physical.end(),"Configure and record a reference on this module.");const auto &reference=device.channels[std::size_t(refIndex-device.physical.begin())];
            need(!reference.quality&&reference.average&&reference.average->averages()>=2,"A valid reference and at least two averages are required.");if(!ods)need(reference.descriptor.unit=="N","Modal fitting requires a force reference in N.");
            std::vector<const Channel*> channels;std::vector<lanxi::Response> responses;double rate=reference.descriptor.sampleRate(),df=rate/m_fft;
            for(std::size_t i=0;i<device.physical.size();++i){const auto &c=device.channels[i];if(c.quality||!c.average||c.average->averages()<2)continue;const auto &unit=c.descriptor.unit;
                if(unit!="m"&&unit!="m/s"&&unit!="m/s2")continue;
                need(c.descriptor.sampleRate()==rate,"Structural channels have different rates.");lanxi::UffFunction f;f.responseNode=device.physical[i];f.responseDirection=3;f.referenceNode=ref;f.referenceDirection=3;f.type=4;f.complex=true;f.even=false;
                for(auto v:options.value("geometry").toList()){auto p=v.toMap();if(p.value("channel").toInt()==device.physical[i]){f.responseNode=p.value("node",device.physical[i]).toInt();f.responseDirection=p.value("direction",3).toInt();break;}}
                need(f.responseNode>0&&std::abs(f.responseDirection)>=1&&std::abs(f.responseDirection)<=3,"Map each motion channel to a positive node and signed XYZ translation direction.");
                f.responseEntity=m_selectedDevice==0?"3676":"3677";f.referenceEntity=f.responseEntity;f.axes[0].unit="Hz";f.axes[1].unit="m";f.axes[2].unit=ods?"NONE":"N";
                functions.push_back(f);channels.push_back(&c);responses.push_back(c.average->response(m_referenceFloor));
            }
            need(!functions.empty(),"Record calibrated motion response channels (m, m/s or m/s2).");
            if(ods) {
                const double requested=options.value("frequency",100).toDouble();need(std::isfinite(requested)&&requested>0&&requested<rate/2.56,"ODS frequency must be inside measurement bandwidth.");const auto k=std::size_t(std::llround(requested/df));need(k>0,"ODS frequency rounds to DC; increase frequency or FFT resolution.");std::vector<C> values;
                for(std::size_t i=0;i<channels.size();++i){const auto &h=responses[i];need(k<h.valid.size()&&h.valid[k]&&h.coherence[k]>=coherence,"ODS coherence is insufficient at this frequency on a motion channel.");const auto s=channels[i]->average->spectrum();values.push_back(receptance(std::polar(s.rmsBins[k],h.phase[k]*pi/180),k*df,channels[i]->descriptor.unit));}
                result["shape"]=shape(values,functions);result["frequency"]=k*df;result["unit"]="m RMS";result["method"]="Spectral operating shape: response RMS amplitude with cross-spectrum phase relative to a common local reference, converted to displacement. Normalized animation; not an identified mode.";
            }else {
                for(std::size_t k=1;k<responses[0].valid.size();++k)if(k*df>=low&&k*df<=high&&k*df<rate/2.56){bool valid=true;for(const auto &h:responses)valid=valid&&k<h.valid.size()&&h.valid[k]&&h.coherence[k]>=coherence;if(!valid)continue;
                    for(std::size_t i=0;i<functions.size();++i){functions[i].x.push_back(k*df);functions[i].y.push_back(receptance(std::polar(responses[i].magnitude[k],responses[i].phase[k]*pi/180),k*df,channels[i]->descriptor.unit));}}
            }
            result["device"]=m_selectedDevice;result["serial"]=device.serial;result["referenceChannel"]=ref;result["acquisitionSettings"]=m_dataSettings;result["sourceOutcome"]=m_review?m_reviewOutcome:m_failure.isEmpty()?"complete":"partial";result["recoveryNotes"]=m_reviewNotes;
        }
        if(!ods) {
            const auto &hz=functions.front().x;std::vector<std::vector<C>> frfs;
            for(const auto &f:functions){need(f.x.size()==hz.size(),"FRFs must share the same frequency grid.");for(std::size_t k=0;k<hz.size();++k)need(std::abs(f.x[k]-hz[k])<=1e-8*std::max(1.0,hz[k]),"FRFs must share the same frequency grid.");frfs.push_back(f.y);}
            std::vector<double> seeds;for(auto v:options.value("seeds").toList())seeds.push_back(v.toDouble());auto fit=lanxi::fitModes(hz,frfs,seeds,options.value("residualTerms",true).toBool(),[this]{return m_cancel.load();});
            QVariantList modes,mac,errors,frac,measured,fitted,frequencies;
            for(std::size_t j=0;j<fit.modes.size();++j){const auto &mode=fit.modes[j];modes.append(QVariantMap{{"frequency",mode.frequency},{"dampingPercent",100*mode.damping},{"shape",shape(mode.residues,functions)}});QVariantList row;for(const auto &other:fit.modes)row.append(lanxi::modalAssurance(mode.residues,other.residues));mac.append(QVariant(row));}
            for(double v:fit.relativeError)errors.append(v);for(double v:fit.frac)frac.append(v);for(std::size_t k=0;k<hz.size();++k){frequencies.append(hz[k]);measured.append(std::abs(frfs[0][k]));fitted.append(std::abs(fit.fitted[0][k]));}
            result["modes"]=modes;result["mac"]=mac;result["relativeErrors"]=errors;result["frac"]=frac;result["iterations"]=fit.iterations;result["converged"]=fit.converged;result["responseCount"]=int(functions.size());
            result["measured"]=measured;result["fitted"]=fitted;result["frequencies"]=frequencies;result["method"]="Common-pole receptance model with complex residues, seeded bounded variable projection and optional constant/inverse-frequency-squared residuals. MAC compares fitted residue vectors across these response DOFs; no polyreference or stabilization claim.";
            for(std::size_t i=0;i<functions.size();++i){functions[i].y=fit.fitted[i];functions[i].identification[0]="Suno reconstructed receptance";functions[i].axes[0]={18,0,0,0,"Frequency","Hz"};functions[i].axes[1]={8,0,0,0,"Displacement","m"};functions[i].axes[2]={13,0,0,0,"Force","N"};}
        }
        need(!m_cancel.load(),"Structural analysis cancelled.");const QString folder=m_root+"/analyses/"+QUuid::createUuid().toString(QUuid::WithoutBraces);need(QDir().mkpath(folder),"Cannot create structural result directory.");
        if(!ods){QSaveFile file(folder+"/reconstructed.uff");need(file.open(QIODevice::WriteOnly),"Cannot save reconstructed functions.");for(auto &f:functions){f.count=f.y.size();f.even=false;lanxi::UffWriter writer(f,[&](const std::string&s){need(file.write(s.data(),qint64(s.size()))==qint64(s.size()),"Structural UFF write failed.");});for(std::size_t k=0;k<f.y.size();++k)writer.add(f.x[k],f.y[k]);writer.finish();}need(file.commit(),"Cannot finalize structural UFF export.");}
        result["exportPath"]=folder;QString error;need(writeJson(folder+"/result.json",QJsonObject::fromVariantMap(result),error),"Cannot write structural result JSON.");
        m_modalResult=result;m_message="Structural result saved to "+folder;m_phase="review";
    }catch(const std::exception&e){m_message=QString::fromUtf8(e.what());m_modalResult={{"error",m_message}};m_phase="error";}
    m_busy=false;publish();
}
