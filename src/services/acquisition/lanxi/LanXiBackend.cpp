#include "LanXiBackend.h"
#include "LanXiWorker.h"

LanXiBackend::LanXiBackend(QObject *parent) : QObject(parent) {
    connect(&m_audio,&LanXiAudio::changed,this,&LanXiBackend::audioChanged);
    m_state={{"busy",false},{"ready",false},{"phase","disconnected"},
             {"message","Enter both module addresses, then inspect the hardware."},
             {"output","Not checked"},{"channels",QVariantList{}},{"runs",QVariantList{}},
             {"profiles",QVariantList{}},{"plot",QVariantMap{}},{"steps",QVariantList{}}};
    m_worker=new LanXiWorker(this); m_worker->moveToThread(&m_thread);
    connect(&m_thread,&QThread::started,m_worker,[this]{m_worker->initialize();});
    connect(&m_thread,&QThread::finished,m_worker,&QObject::deleteLater);
    m_thread.setObjectName(QStringLiteral("LAN-XI acquisition")); m_thread.start();
}
LanXiBackend::~LanXiBackend() {
    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(m_worker,[this]{m_worker->shutdown();},Qt::BlockingQueuedConnection);
        m_thread.quit(); m_thread.wait();
    }
}
void LanXiBackend::receive(const QVariantMap &state) {
    // A queued idle snapshot must not unlock a newly submitted operation.
    if(state.value("commandId").toULongLong()<m_commandId)return;
    m_state=state; emit changed();
}
void LanXiBackend::lock(const QString &phase) { muteAudio();m_worker->resetCancellation();++m_commandId; m_state["busy"]=true; m_state["phase"]=phase; emit changed(); }
void LanXiBackend::inspect(const QVariantMap &s) {
    if (busy()) return; lock("inspecting"); QMetaObject::invokeMethod(m_worker,[this,s,id=m_commandId]{m_worker->beginCommand(id);m_worker->inspect(s);});
}
void LanXiBackend::start(int ticket,const QVariantMap &s) {
    if (busy()) { emit runEnded(ticket,false,tr("LAN-XI is busy.")); return; }
    lock("starting"); QMetaObject::invokeMethod(m_worker,[this,ticket,s,id=m_commandId]{m_worker->beginCommand(id);m_worker->start(ticket,s);});
}
void LanXiBackend::stop() { m_worker->cancelIO(); QMetaObject::invokeMethod(m_worker,[this]{m_worker->stop();}); }
void LanXiBackend::recover() { QMetaObject::invokeMethod(m_worker,[this]{m_worker->recover();}); }
void LanXiBackend::outputOff() { QMetaObject::invokeMethod(m_worker,[this]{m_worker->outputOff();}); }
void LanXiBackend::selectChannel(int d,int c) { muteAudio();QMetaObject::invokeMethod(m_worker,[this,d,c]{m_worker->selectChannel(d,c);}); }
void LanXiBackend::pauseDisplay(bool p) { QMetaObject::invokeMethod(m_worker,[this,p]{m_worker->pauseDisplay(p);}); }
void LanXiBackend::saveProfile(const QString &n,const QVariantMap &s) {
    if (!busy()) QMetaObject::invokeMethod(m_worker,[this,n,s]{m_worker->saveProfile(n,s);});
}
void LanXiBackend::loadProfile(const QString &id) { if (!busy()) QMetaObject::invokeMethod(m_worker,[this,id]{m_worker->loadProfile(id);}); }
void LanXiBackend::resetProfile(const QString &p) { if (!busy()) QMetaObject::invokeMethod(m_worker,[this,p]{m_worker->resetProfile(p);}); }
void LanXiBackend::refreshRuns() { if (!busy()) QMetaObject::invokeMethod(m_worker,[this]{m_worker->refreshRuns();}); }
void LanXiBackend::review(const QString &id) {
    if (busy()) return; lock("reviewing"); QMetaObject::invokeMethod(m_worker,[this,id,command=m_commandId]{m_worker->beginCommand(command);m_worker->review(id);});
}
void LanXiBackend::exportRun(const QString &id,const QString &format) {
    if (busy()) return; lock("exporting"); QMetaObject::invokeMethod(m_worker,[this,id,format,command=m_commandId]{m_worker->beginCommand(command);m_worker->exportRun(id,format);});
}
void LanXiBackend::calibrate(int d,int c,double level,double frequency) {
    if (!busy()) QMetaObject::invokeMethod(m_worker,[this,d,c,level,frequency]{m_worker->calibrate(d,c,level,frequency);});
}
void LanXiBackend::pinComparison() {if(!busy())QMetaObject::invokeMethod(m_worker,[this]{m_worker->pinComparison();});}
void LanXiBackend::clearComparison() {QMetaObject::invokeMethod(m_worker,[this]{m_worker->clearComparison();});}
void LanXiBackend::report(const QString &id,const QVariantMap &options) {
    if(busy())return;lock("reporting");QMetaObject::invokeMethod(m_worker,[this,id,options,command=m_commandId]{m_worker->beginCommand(command);m_worker->report(id,options);});
}
void LanXiBackend::reprocess(const QString &id,const QVariantMap &analysis) {
    if(busy())return;lock("reviewing");QMetaObject::invokeMethod(m_worker,[this,id,analysis,command=m_commandId]{m_worker->beginCommand(command);m_worker->reprocess(id,analysis);});
}
void LanXiBackend::discover(const QString &prefix) {
    if(busy())return;lock("discovering");QMetaObject::invokeMethod(m_worker,[this,prefix,command=m_commandId]{m_worker->beginCommand(command);m_worker->discover(prefix);});
}
void LanXiBackend::detectTransducers(int device) {
    if(busy())return;lock("transducers");QMetaObject::invokeMethod(m_worker,[this,device,command=m_commandId]{m_worker->beginCommand(command);m_worker->detectTransducers(device);});
}
void LanXiBackend::saveSensor(const QString &name,const QVariantMap &channel) {
    if(!busy())QMetaObject::invokeMethod(m_worker,[this,name,channel]{m_worker->saveSensor(name,channel);});
}
void LanXiBackend::batchReports(const QVariantList &ids,const QVariantMap &options) {
    if(busy())return;lock("batch");QMetaObject::invokeMethod(m_worker,[this,ids,options,command=m_commandId]{m_worker->beginCommand(command);m_worker->batchReports(ids,options);});
}
void LanXiBackend::analyze(const QVariantMap &options) {
    if(busy())return;lock("processing");QMetaObject::invokeMethod(m_worker,[this,options,id=m_commandId]{m_worker->beginCommand(id);m_worker->analyze(options);});
}
void LanXiBackend::exportAnalysis() {
    if(busy())return;lock("exporting");QMetaObject::invokeMethod(m_worker,[this,id=m_commandId]{m_worker->beginCommand(id);m_worker->exportAnalysis();});
}
void LanXiBackend::processRecording(const QString &runId,const QVariantMap &options) {
    if(busy())return;lock("processing-recording");QMetaObject::invokeMethod(m_worker,[this,runId,options,id=m_commandId]{m_worker->beginCommand(id);m_worker->processRecording(runId,options);});
}
void LanXiBackend::importUff(const QString &path) {
    if(busy())return;lock("importing");QMetaObject::invokeMethod(m_worker,[this,path,id=m_commandId]{m_worker->beginCommand(id);m_worker->importUff(path);});
}
void LanXiBackend::selectUff(int index) {if(!busy())QMetaObject::invokeMethod(m_worker,[this,index]{m_worker->selectUff(index);});}
void LanXiBackend::exportUff(const QVariantMap &options) {
    if(busy())return;lock("exporting");QMetaObject::invokeMethod(m_worker,[this,options,id=m_commandId]{m_worker->beginCommand(id);m_worker->exportUff(options);});
}
void LanXiBackend::structuralAnalysis(const QVariantMap &options) {
    if(busy())return;lock("structural-analysis");QMetaObject::invokeMethod(m_worker,[this,options,id=m_commandId]{m_worker->beginCommand(id);m_worker->structuralAnalysis(options);});
}
void LanXiBackend::soundPower(const QVariantMap &options) {
    if(busy())return;lock("acoustic-method");QMetaObject::invokeMethod(m_worker,[this,options,id=m_commandId]{m_worker->beginCommand(id);m_worker->soundPower(options);});
}
void LanXiBackend::saveWorkspace(const QVariantMap &workspace) {
    if(!busy())QMetaObject::invokeMethod(m_worker,[this,workspace]{m_worker->saveWorkspace(workspace);});
}
void LanXiBackend::audition(double fullScale,double volume) {if(!busy()){const auto generation=++m_audioGeneration;QMetaObject::invokeMethod(m_worker,[this,fullScale,volume,generation]{m_worker->audition(fullScale,volume,generation);});}}
void LanXiBackend::muteAudio(){++m_audioGeneration;m_audio.mute();}
