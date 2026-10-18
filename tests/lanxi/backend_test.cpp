#include "LanXiBackend.h"
#include "LanXiUff.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <cmath>
#include <iostream>
#include <sstream>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(argc!=4){std::cerr<<"Usage: lanxi_backend_test URL3676 URL3677 scenario\n";return 2;}
    LanXiBackend backend;QString scenario=QString::fromLocal8Bit(argv[3]);
    int stage=0;bool started=false,recovered=false,failed=false;QString runId;qulonglong expectedSamples=0;
    const auto fail=[&](const QString &message){failed=true;std::cerr<<message.toStdString()<<'\n';app.exit(1);};
    QObject::connect(&backend,&LanXiBackend::settingsChanged,&app,[&]{
        if(stage!=0 || backend.settings().isEmpty())return;
        stage=1;auto settings=backend.settings();auto devices=settings["devices"].toList();
        for(int d=0;d<2;++d){auto config=devices[d].toMap();config["url"]=QString::fromLocal8Bit(argv[d+1]);config["sampleRate"]=8192;devices[d]=config;}
        settings["devices"]=devices;settings["durationSeconds"]=2.5;settings["fftSize"]=1024;
        if(scenario=="sequence")settings["sequence"]=QVariantMap{{"repetitions",2},{"delaySeconds",0.1}};
        auto analysis=settings["analysis"].toMap();analysis["toneHz"]=1024;settings["analysis"]=analysis;
        auto trigger=settings["trigger"].toMap();trigger["enabled"]=true;trigger["level"]=0;trigger["hysteresis"]=0.001;
        trigger["preSeconds"]=0.02;trigger["postSeconds"]=0.04;settings["trigger"]=trigger;
        auto g=settings["generator"].toMap();g["mode"]=scenario=="steps"?"steps":"sine";g["frequency"]=1024;g["startHz"]=512;g["stopHz"]=2048;
        g["steps"]=3;g["levelVp"]=0.05;g["armed"]=true;g["settleSeconds"]=0.1;g["dwellSeconds"]=0.2;settings["generator"]=g;
        backend.inspect(settings);
    });
    QObject::connect(&backend,&LanXiBackend::changed,&app,[&]{
        const auto state=backend.state();
        if(stage==1 && backend.ready() && !backend.busy()){stage=10;backend.detectTransducers(0);}
        else if(stage==10 && !backend.busy()) {
            if(state["transducers"].toList().size()!=4){fail("TEDS inspection failed.");return;}
            stage=2;backend.start(1,backend.settings());
        }
        else if(stage==4 && !backend.busy()) {
            if(state["phase"].toString()!="review"){fail("Replay failed: "+state["message"].toString());return;}
            stage=5;backend.exportRun(runId,"csv");
        } else if(stage==5 && !backend.busy()) {
            if(state["phase"].toString()=="error"){fail(state["message"].toString());return;}
            stage=6;backend.exportRun(runId,"wav");
        } else if(stage==6 && !backend.busy()) {
            if(state["phase"].toString()=="error"){fail(state["message"].toString());return;}
            stage=7;auto analysis=backend.settings()["analysis"].toMap();analysis["window"]="flat-top";analysis["removeMean"]=true;
            backend.reprocess(runId,analysis);
        } else if(stage==7 && !backend.busy()) {
            if(state["phase"].toString()=="error"){fail(state["message"].toString());return;}
            const auto analysis=state["plot"].toMap()["analysis"].toMap();
            if(analysis["spectrogram"].toList().isEmpty()||!analysis["trigger"].toMap()["complete"].toBool()){fail("Missing spectrogram or event capture.");return;}
            stage=8;backend.report(runId,{{"operator","Fixture <test>"},{"analysis",backend.settings()["analysis"]}});
        } else if(stage==8 && !backend.busy()) {
            if(state["phase"].toString()=="error"||!QFile::exists(state["reportPath"].toString()+"/report.html")){fail("Report export failed.");return;}
            stage=9;backend.batchReports({runId},{});
        } else if(stage==9 && !backend.busy()) {
            const auto results=state["batchResults"].toList();
            if(results.size()!=1||!results[0].toMap()["success"].toBool()){fail("Batch reporting failed.");return;}
            stage=11;backend.analyze({{"operation","processing"},{"windowSamples",8192},{"referenceChannel",2},
                {"stages",QVariantList{QVariantMap{{"type","removeMean"},{"value",0}},QVariantMap{{"type","gain"},{"value",2}}}}});
        } else if(stage==11 && !backend.busy()) {
            const auto result=state["workbench"].toMap();
            if(result.contains("error")||result["wave"].toList().isEmpty()||result["correlation"].toList().isEmpty()||result["rms"].toDouble()<=0){fail("Workbench processing failed: "+result["error"].toString());return;}
            stage=12;backend.exportAnalysis();
        } else if(stage==12 && !backend.busy()) {
            if(!QFile::exists(state["workbench"].toMap()["exportPath"].toString()+"/processed.csv")){fail("Processed CSV export failed.");return;}
            stage=13;backend.analyze({{"operation","limits"},{"metric","gain"},
                {"limits",QVariantList{QVariantMap{{"hz",100},{"lower",100},{"upper",101}},QVariantMap{{"hz",2000},{"lower",100},{"upper",101}}}}});
        } else if(stage==13 && !backend.busy()) {
            const auto result=state["workbench"].toMap();
            if(result.contains("error")||result["verdict"].toString()=="pass"||!result.contains("verdict")){fail("Acceptance incorrectly passed unavailable or out-of-range results.");return;}
            if(scenario=="truncated"){std::cout<<"PASS partial replay, exports, reprocessing and workbench\n";app.quit();return;}
            for(auto v:state["channels"].toList()){auto c=v.toMap();if(c["device"].toInt()==0&&c["channel"].toInt()==1)expectedSamples=c["samples"].toULongLong();}
            stage=14;backend.processRecording(runId,{{"device",0},{"channel",1},{"stages",QVariantList{QVariantMap{{"type","removeMean"},{"value",0}},QVariantMap{{"type","gain"},{"value",2}}}}});
        } else if(stage==14 && !backend.busy()) {
            const auto result=state["recordResult"].toMap();if(result.contains("error")||!QFile::exists(result["exportPath"].toString()+"/processed.uff")||result["samples"].toULongLong()!=expectedSamples||!expectedSamples){fail("Whole-record processing failed: "+result["error"].toString());return;}
            stage=15;backend.importUff(result["exportPath"].toString()+"/processed.uff");
        } else if(stage==15 && !backend.busy()) {
            auto exchange=state["interchange"].toMap();if(exchange.contains("error")||exchange["selected"].toMap()["samples"].toULongLong()!=expectedSamples){fail("UFF import did not preserve all recording samples.");return;}
            stage=16;backend.exportUff({{"source","import"},{"index",0}});
        } else if(stage==16 && !backend.busy()) {
            if(!QFile::exists(state["interchange"].toMap()["exportPath"].toString())){fail("UFF export failed.");return;}
            lanxi::UffFunction f;f.type=4;f.complex=true;f.even=false;f.referenceNode=1;f.referenceDirection=3;f.responseNode=2;f.responseDirection=3;f.axes[0].unit="Hz";f.axes[1].unit="m";f.axes[2].unit="N";
            const double pi=3.14159265358979323846;for(int k=0;k<400;++k){const double hz=70+k*.2,w=2*pi*hz;f.x.push_back(hz);f.y.push_back(1./std::complex<double>(std::pow(2*pi*100,2)-w*w,2*.02*2*pi*100*w)+2./std::complex<double>(std::pow(2*pi*115,2)-w*w,2*.03*2*pi*115*w));}
            lanxi::UffDocument document;document.functions.push_back(f);std::ostringstream output;lanxi::writeUff(output,document);
            const auto path=qEnvironmentVariable("SUNO_LANXI_DATA_DIR")+"/modal-fixture.uff";QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(QByteArray::fromStdString(output.str()))<0){fail("Cannot write modal fixture.");return;}file.close();stage=17;backend.importUff(path);
        } else if(stage==17 && !backend.busy()) {
            stage=18;backend.structuralAnalysis({{"source","import"},{"index",0},{"seeds",QVariantList{99,116}},{"lowerHz",70},{"upperHz",149},{"residualTerms",false}});
        } else if(stage==18 && !backend.busy()) {
            const auto result=state["modalResult"].toMap();const auto modes=result["modes"].toList();if(result.contains("error")||modes.size()!=2||std::abs(modes[0].toMap()["frequency"].toDouble()-100)>.1||!QFile::exists(result["exportPath"].toString()+"/reconstructed.uff")){fail("Imported modal analysis failed: "+result["error"].toString());return;}
            stage=19;backend.soundPower({{"weighting","Z"},{"environmentCorrection",0},{"positions",QVariantList{QVariantMap{{"area",2},{"measuredDb",80},{"backgroundDb",60}}}}});
        } else if(stage==19 && !backend.busy()) {
            const auto result=state["acousticResult"].toMap();if(result.contains("error")||std::abs(result["powerDb"].toDouble()-82.9666519)>.0001){fail("Sound power integration failed.");return;}
            std::cout<<"PASS paired capture, replay, exports, workbench, whole recording, UFF interchange, modal fit and sound power\n";app.quit();
        }
    });
    QObject::connect(&backend,&LanXiBackend::runStarted,&app,[&](int ticket){started=ticket==1;});
    QObject::connect(&backend,&LanXiBackend::recoveryRequired,&app,[&](int ticket,const QString &){
        if(scenario!="stop"||ticket!=1||recovered){fail("Unexpected recovery state.");return;}
        recovered=true;QTimer::singleShot(100,&backend,[&]{backend.recover();});
    });
    QObject::connect(&backend,&LanXiBackend::runEnded,&app,[&](int ticket,bool success,const QString &message){
        if(ticket!=1){fail("Stale run callback.");return;}
        const bool fault=scenario=="start"||scenario=="stop"||scenario=="disconnect"||scenario=="truncated";
        if(success==fault){fail("Unexpected run result: "+message);return;}
        if(!fault && !started){fail("Missing paired start acknowledgement.");return;}
        if(scenario=="stop"&&!recovered){fail("Output-stop failure did not retain ownership.");return;}
        QTimer::singleShot(200,&app,[&,fault]{
            if(failed)return;
            const auto state=backend.state();if(backend.busy()){fail("Ownership still held after confirmed recovery.");return;}
            runId=state["runId"].toString();
            if(scenario=="sequence"&&state["sequenceResults"].toList().size()!=2){fail("Expected two completed sequence measurements.");return;}
            QFile file(state["runPath"].toString()+"/manifest.json");
            if(!file.open(QIODevice::ReadOnly)){fail("No run manifest.");return;}
            const auto manifest=QJsonDocument::fromJson(file.readAll()).object();
            if(manifest["clockRelationship"].toString()!="independent"){fail("Clock domains were merged.");return;}
            if(fault && scenario!="truncated"){std::cout<<"PASS "<<scenario.toStdString()<<" rollback/recovery\n";app.quit();return;}
            if(state["channels"].toList().size()!=8){fail("Expected eight physical channel records.");return;}
            if(scenario=="steps"&&state["steps"].toList().size()!=18){fail("Missing per-device stepped response results.");return;}
            stage=4;backend.review(runId);
        });
    });
    QTimer::singleShot(40000,&app,[&]{fail("Backend integration test timed out: "+backend.state()["message"].toString());});
    return app.exec();
}
