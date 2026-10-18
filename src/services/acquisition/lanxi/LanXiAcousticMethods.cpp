#include "LanXiWorker.h"
#include "LanXiSoundPower.h"
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <stdexcept>
void LanXiWorker::soundPower(const QVariantMap &options) {
    if(m_busy)return;m_acousticResult.clear();m_busy=true;m_phase="acoustic-method";
    try {
        auto need=[](bool ok,const char*s){if(!ok)throw std::runtime_error(s);};std::vector<lanxi::PressurePosition> positions;
        need(QJsonDocument(QJsonObject::fromVariantMap(options)).toJson().size()<=262144,"Sound-power input exceeds 256 KiB.");
        const QString weighting=options.value("weighting").toString();need(weighting=="A"||weighting=="C"||weighting=="Z","Select A, C or Z weighting for all pressure readings.");
        for(auto v:options.value("positions").toList()){auto p=v.toMap();need(p.contains("measuredDb")&&!p.value("measuredDb").isNull()&&p.contains("backgroundDb")&&!p.value("backgroundDb").isNull(),"Each position requires source-on and background measurements.");
            positions.push_back({p.value("area").toDouble(),p.value("measuredDb").toDouble(),p.value("backgroundDb").toDouble()});}
        auto p=lanxi::surfaceSoundPower(positions,options.value("environmentCorrection",0).toDouble(),options.value("minimumSeparation",6).toDouble());
        QVariantMap result{{"schema",1},{"operation","surface-sound-power"},{"weighting",weighting},{"options",options},{"area",p.area},{"measuredDb",p.measuredDb},{"backgroundDb",p.backgroundDb},
            {"k1",p.backgroundCorrection},{"k2",p.environmentCorrection},{"separationDb",p.separationDb},{"surfaceDb",p.surfaceDb},{"powerDb",p.powerDb},
            {"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"qualification","Engineering estimate from an enveloping measurement surface. No ISO conformity, uncertainty, fixture or environmental qualification is asserted."}};
        const QString folder=m_root+"/analyses/"+QUuid::createUuid().toString(QUuid::WithoutBraces);need(QDir().mkpath(folder),"Cannot create acoustic result directory.");result["exportPath"]=folder;QString error;
        need(writeJson(folder+"/result.json",QJsonObject::fromVariantMap(result),error),"Cannot write acoustic result JSON.");
        QByteArray csv="position,area_m2,source_on_db,background_db\n";for(std::size_t i=0;i<positions.size();++i){auto row=positions[i];csv+=QByteArray::number(qulonglong(i+1))+','+QByteArray::number(row.area,'g',17)+','+QByteArray::number(row.sourceAndBackgroundDb,'g',17)+','+QByteArray::number(row.backgroundDb,'g',17)+'\n';}
        QSaveFile file(folder+"/positions.csv");need(file.open(QIODevice::WriteOnly)&&file.write(csv)==csv.size()&&file.commit(),"Cannot save pressure-position CSV.");
        m_acousticResult=result;m_message="Sound-power engineering result saved to "+folder;m_phase="review";
    }catch(const std::exception&e){m_message=QString::fromUtf8(e.what());m_acousticResult={{"error",m_message}};m_phase="error";}
    m_busy=false;publish();
}
