#pragma once
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "LanXiAudio.h"

class LanXiWorker;
class LanXiBackend : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QVariantMap audio READ audio NOTIFY audioChanged)
public:
    explicit LanXiBackend(QObject *parent=nullptr);
    ~LanXiBackend() override;
    QVariantMap state() const { return m_state; }
    QVariantMap settings() const { return m_settings; }
    bool busy() const { return m_state.value("busy").toBool(); }
    bool ready() const { return m_state.value("ready").toBool(); }
    QVariantMap audio() const { return m_audio.state(); }
    Q_INVOKABLE void inspect(const QVariantMap &settings);
    Q_INVOKABLE void start(int ticket, const QVariantMap &settings);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void recover();
    Q_INVOKABLE void outputOff();
    Q_INVOKABLE void selectChannel(int device, int channel);
    Q_INVOKABLE void pauseDisplay(bool paused);
    Q_INVOKABLE void saveProfile(const QString &name, const QVariantMap &settings);
    Q_INVOKABLE void loadProfile(const QString &id);
    Q_INVOKABLE void resetProfile(const QString &workflow);
    Q_INVOKABLE void refreshRuns();
    Q_INVOKABLE void review(const QString &runId);
    Q_INVOKABLE void exportRun(const QString &runId, const QString &format);
    Q_INVOKABLE void calibrate(int device, int channel, double levelDb, double frequency);
    Q_INVOKABLE void pinComparison();
    Q_INVOKABLE void clearComparison();
    Q_INVOKABLE void report(const QString &runId, const QVariantMap &options);
    Q_INVOKABLE void reprocess(const QString &runId, const QVariantMap &analysis);
    Q_INVOKABLE void discover(const QString &prefix);
    Q_INVOKABLE void detectTransducers(int device);
    Q_INVOKABLE void saveSensor(const QString &name, const QVariantMap &channel);
    Q_INVOKABLE void batchReports(const QVariantList &runIds, const QVariantMap &options);
    Q_INVOKABLE void analyze(const QVariantMap &options);
    Q_INVOKABLE void processRecording(const QString &runId,const QVariantMap &options);
    Q_INVOKABLE void importUff(const QString &path);
    Q_INVOKABLE void selectUff(int index);
    Q_INVOKABLE void exportUff(const QVariantMap &options);
    Q_INVOKABLE void structuralAnalysis(const QVariantMap &options);
    Q_INVOKABLE void soundPower(const QVariantMap &options);
    Q_INVOKABLE void exportAnalysis();
    Q_INVOKABLE void saveWorkspace(const QVariantMap &workspace);
    Q_INVOKABLE void audition(double fullScale, double volume);
    Q_INVOKABLE void muteAudio();
signals:
    void changed();
    void audioChanged();
    void settingsChanged();
    void runStarted(int ticket);
    void runEnded(int ticket, bool success, const QString &message);
    void recoveryRequired(int ticket, const QString &message);
private:
    friend class LanXiWorker;
    void receive(const QVariantMap &state);
    void lock(const QString &phase);
    QThread m_thread;
    LanXiWorker *m_worker = nullptr;
    QVariantMap m_state, m_settings;
    quint64 m_commandId = 0;
    quint64 m_audioGeneration = 0;
    LanXiAudio m_audio;
};
