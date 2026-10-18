#include "LanXiWorker.h"
#include "LanXiBackend.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>
#include <cmath>
#include <stdexcept>

void LanXiWorker::discover(const QString &prefix) {
    if(m_busy)return;
    const auto match=QRegularExpression("^([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})$").match(prefix.trimmed());
    if(!match.hasMatch()||match.captured(1).toInt()==0||match.captured(1).toInt()>=224||match.captured(2).toInt()>255||match.captured(3).toInt()>255) {
        m_message="Enter an IPv4 /24 prefix, for example 192.168.1. Discovery reads module identity on port 80.";publish();return;
    }
    ++m_generation;m_busy=true;m_phase="discovering";m_discovered.clear();m_discoveryReplies.clear();m_discoveryQueue.clear();m_probeCount=0;
    for(int i=1;i<=254;++i)m_discoveryQueue.append(QString("http://%1.%2.%3.%4").arg(match.captured(1).toInt()).arg(match.captured(2).toInt()).arg(match.captured(3).toInt()).arg(i));
    m_message="Discovering LAN-XI Light modules on "+prefix+".0/24. Stop cancels the search.";publish();probeNext();
}
void LanXiWorker::probeNext() {
    if(m_phase!="discovering")return;
    while(m_probeCount<8&&!m_discoveryQueue.isEmpty()) {
        const QString address=m_discoveryQueue.takeFirst();const auto generation=m_generation;
        QNetworkRequest request(QUrl(address+"/rest/rec/module/info"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto *reply=m_network->get(request);++m_probeCount;m_discoveryReplies.push_back(reply);
        auto bytes=std::make_shared<QByteArray>();auto excessive=std::make_shared<bool>(false);
        connect(reply,&QIODevice::readyRead,this,[reply,bytes,excessive]{
            if(bytes->size()+reply->bytesAvailable()>1024*1024){*excessive=true;reply->abort();}else bytes->append(reply->readAll());
        });
        const QPointer<QNetworkReply> guard(reply);
        QTimer::singleShot(1200,this,[guard]{if(guard&&!guard->isFinished())guard->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,address,reply,bytes,excessive,generation]{
            reply->deleteLater();if(generation!=m_generation)return;
            --m_probeCount;
            if(!*excessive&&reply->error()==QNetworkReply::NoError&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200
                &&bytes->size()+reply->bytesAvailable()<=1024*1024) {
                bytes->append(reply->readAll());const auto object=QJsonDocument::fromJson(*bytes).object();
                const auto module=object["module"].toObject();const auto type=module["type"].toObject()["number"].toString();
                if(type=="3676"||type=="3677")m_discovered.append(QVariantMap{{"url",address},{"type",type},
                    {"serial",module["serial"].toVariant().toString()},{"state",object["moduleState"].toString()}});
            }
            probeNext();
        });
    }
    if(m_probeCount==0&&m_discoveryQueue.isEmpty()) {
        m_discoveryReplies.clear();m_busy=false;m_phase=m_ready?"ready":"disconnected";
        m_message=QString("Discovery finished: %1 LAN-XI Light modules found. Select addresses, then inspect both modules.").arg(m_discovered.size());publish();
    }
}
void LanXiWorker::detectTransducers(int device) {
    if(m_busy)return;
    if(!m_ready||device<0||device>1){m_message="Inspect both modules before reading TEDS.";publish();return;}
    ++m_generation;m_busy=true;m_phase="transducers";m_message="Detecting transducers. Sensor power and channel settings are not applied automatically.";publish();
    const auto finish=[this](const QString &error){m_busy=false;m_phase=m_ready?"ready":"disconnected";m_message=error;publish();};
    request(device,"GET","/module/info",{},[this,device,finish](bool ok,const QJsonValue &value,const QString &why){
        QString error=why;
        if(!ok||!verifyIdentity(device,value.toObject(),error)||value.toObject()["moduleState"].toString()!="Idle") {
            finish(error.isEmpty()?"TEDS detection requires an idle module.":error);return;
        }
        request(device,"POST","/channels/input/all/transducers/detect",{},[this,device,finish](bool ok,const QJsonValue &,const QString &error){
            if(!ok){finish(error);return;}
            request(device,"GET","/channels/input/all/transducers",{},[this,device,finish](bool ok,const QJsonValue &value,const QString &error){
                if(!ok||!value.isArray()||value.toArray().size()!=4){finish(error.isEmpty()?"Invalid TEDS channel list.":error);return;}
                QVariantList rows;for(const auto &row:m_transducers)if(row.toMap().value("device").toInt()!=device)rows.append(row);
                const auto array=value.toArray();
                for(int c=0;c<4;++c) {auto item=array[c].toObject().toVariantMap();item["device"]=device;item["channel"]=c+1;
                    item["available"]=!array[c].isNull()&&!array[c].toObject().isEmpty();rows.append(item);}
                m_transducers=rows;finish("TEDS information read. Review each sensitivity, unit and powering requirement before applying it.");
            });
        });
    });
}
void LanXiWorker::refreshSensors() {
    m_sensors.clear();QDir().mkpath(m_root+"/Sensors");
    const auto files=QDir(m_root+"/Sensors").entryInfoList({"*.json"},QDir::Files|QDir::NoSymLinks,QDir::Name);
    for(const auto &file:files) {
        if(m_sensors.size()>=500)break;
        try {const auto value=readJson(file.absoluteFilePath());if(value["schema"].toInt()==1)m_sensors.append(value.toVariantMap());}catch(const std::exception &){}
    }
    publish();
}
void LanXiWorker::saveSensor(const QString &name,const QVariantMap &channel) {
    if(m_busy)return;
    const double sensitivity=channel.value("sensitivity").toDouble();
    if(name.trimmed().isEmpty()||name.size()>120||!std::isfinite(sensitivity)||sensitivity<=0||sensitivity>1e9||!QStringList{"V","Pa","N","m/s2","m/s","m"}.contains(channel.value("unit").toString())) {
        m_message="Enter a sensor name, supported engineering unit and positive sensitivity in V/unit.";publish();return;
    }
    const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);QVariantMap saved;
    for(const auto &key:QStringList{"unit","sensitivity","sensorSerial","calibrationUtc","calibrationHz","calibrationLevelDb","ccld","polarization"})if(channel.contains(key))saved[key]=channel.value(key);
    QString error;const QJsonObject record{{"schema",1},{"id",id},{"name",name.trimmed()},{"savedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"channel",QJsonObject::fromVariantMap(saved)}};
    QDir().mkpath(m_root+"/Sensors");m_message=writeJson(m_root+"/Sensors/"+id+".json",record,error)?"Sensor saved to the local library.":error;refreshSensors();
}
