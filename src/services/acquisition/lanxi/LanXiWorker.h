#pragma once
#include "LanXiCore.h"
#include "LanXiAnalysis.h"
#include "LanXiUff.h"
#include <QObject>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantMap>
#include <atomic>
#include <deque>
#include <memory>

class LanXiBackend;
class LanXiWorker : public QObject {
public:
    explicit LanXiWorker(LanXiBackend *owner);
    void initialize();
    void inspect(const QVariantMap &settings);
    void start(int ticket, const QVariantMap &settings);
    void stop(const QString &reason = {});
    void recover();
    void outputOff();
    void selectChannel(int device, int channel);
    void pauseDisplay(bool value);
    void saveProfile(const QString &name, const QVariantMap &settings);
    void loadProfile(const QString &id);
    void resetProfile(const QString &workflow);
    void refreshRuns();
    void review(const QString &id);
    void exportRun(const QString &id, const QString &format);
    void calibrate(int device, int channel, double levelDb, double frequency);
    void pinComparison();
    void clearComparison();
    void report(const QString &id, const QVariantMap &options);
    void reprocess(const QString &id, const QVariantMap &analysis);
    void discover(const QString &prefix);
    void detectTransducers(int device);
    void saveSensor(const QString &name, const QVariantMap &channel);
    void refreshSensors();
    void batchReports(const QVariantList &ids, const QVariantMap &options);
    void analyze(const QVariantMap &options);
    void processRecording(const QString &id,const QVariantMap &options);
    void importUff(const QString &path);
    void selectUff(int index);
    void exportUff(const QVariantMap &options);
    void structuralAnalysis(const QVariantMap &options);
    void soundPower(const QVariantMap &options);
    void resetCancellation() { m_cancel.store(false); }
    void exportAnalysis();
    void saveWorkspace(const QVariantMap &workspace);
    void audition(double fullScale,double volume,quint64 generation);
    void shutdown();
    void cancelIO() { m_cancel.store(true); }
    void beginCommand(quint64 id) { m_commandId=id; }
private:
    struct Channel {
        lanxi::Descriptor descriptor;
        lanxi::Time first, last;
        std::uint64_t total = 0;
        qint64 firstIndex = 0, pendingIndex = 0;
        std::deque<double> pending, preview;
        std::unique_ptr<lanxi::SpectralAverage> average;
        std::uint16_t quality = 0;
        std::vector<double> latest;
        std::deque<double> history;
        qint64 historyEnd=0;
        double recentRms = 0, peakHz = 0, stableSeconds = 0, previousDb = 0;
        lanxi::LevelMeter levels;
        lanxi::TriggerCapture trigger;
        std::vector<lanxi::Biquad> filters;
        std::deque<std::vector<double>> spectrogram;
        std::deque<double> spectrogramTimes;
        bool analysisConfigured=false;
        std::uint64_t triggerOrigin=0;
    };
    struct Device {
        QString url, serial, state, label;
        QJsonObject info, setup;
        QPointer<QNetworkReply> reply;
        QPointer<QTcpSocket> socket;
        lanxi::Decoder decoder;
        std::array<Channel,4> channels;
        std::vector<int> physical;
        lanxi::Time origin;
        bool hasOrigin = false, receiving = false;
        qint64 lastDataMs = 0;
        QFile raw;
        QStringList segments;
        qint64 written = 0;
        std::uint64_t blockCount = 0;
    };
    using Done = std::function<void(bool, const QJsonValue &, const QString &)>;
    void request(int device, const QByteArray &method, const QString &path, const QJsonValue &body, Done done);
    void waitState(int device, const QString &expected, std::function<void()> done, int remaining=80);
    void inspectDevice(int device);
    void probeNext();
    void openDevice(int device);
    void configureDevice(int device);
    void connectStream(int device);
    void consume(int device);
    void accept(int device, const lanxi::Event &event);
    void processBlocks(int device);
    void acquisitionReady();
    void prepareGenerator();
    void setGenerator(double frequency, double level, std::function<void()> done);
    void startGenerator();
    void rampGenerator(int step);
    void tick();
    void stopDevice(int device, int remaining=12);
    void stoppedDevice(int device, const QString &error={});
    void finishStop();
    bool finishSequenceStep();
    void fail(const QString &message);
    void publish(bool plot=false);
    QVariantMap plotData() const;
    QVariantList channelData() const;
    QVariantMap analysisData(const Channel &channel) const;
    QVariantMap resultSnapshot() const;
    bool validate(const QVariantMap &settings, QString &error) const;
    bool applySetup(int device, QString &error);
    bool verifyIdentity(int device, const QJsonObject &info, QString &error) const;
    void resetData();
    void beginArchive();
    void writeRaw(int device, const QByteArray &bytes);
    void writeManifest(const QString &outcome);
    QString archivePath(const QString &id) const;
    void readArchive(const QString &id, const QString &exportFormat, const QVariantMap &analysis = {});
    QJsonObject manifest() const;
    static QVariantMap defaults(const QString &workflow);
    static bool writeJson(const QString &path, const QJsonObject &value, QString &error);
    static QJsonObject readJson(const QString &path);

    QPointer<LanXiBackend> m_owner;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_timer = nullptr;
    std::array<Device,2> m_devices;
    lanxi::StopGate m_gate;
    std::array<bool,2> m_stopDone{false,false};
    QVariantMap m_settings, m_dataSettings, m_snapshot, m_comparison;
    QVariantMap m_workbench, m_workspace;
    QVariantMap m_interchange, m_recordResult, m_modalResult, m_acousticResult;
    lanxi::UffDocument m_imported;
    QVariantList m_sequenceResults;
    QVariantMap m_sequenceOptions;
    QString m_sequenceId;
    int m_sequenceRemaining=0;
    bool m_sequenceActive=false, m_sequenceFailed=false;
    std::vector<double> m_processed;
    QString m_reportPath, m_reviewOutcome, m_reviewNotes;
    QVariantList m_discovered, m_sensors, m_transducers;
    std::vector<QPointer<QNetworkReply>> m_discoveryReplies;
    QStringList m_discoveryQueue;
    int m_probeCount=0;
    bool m_batch=false, m_reporting=false;
    QVariantList m_batchResults;
    QVariantList m_runs, m_profiles, m_steps, m_qualityEvents;
    QString m_root, m_runId, m_runPath, m_startedUtc, m_message, m_failure, m_phase="disconnected", m_output="Not checked";
    QStringList m_errors;
    QElapsedTimer m_clock;
    quint64 m_generation=0, m_commandId=0;
    int m_ticket=0, m_selectedDevice=0, m_selectedChannel=1, m_step=0;
    std::size_t m_fft=4096;
    bool m_busy=false, m_ready=false, m_stopping=false, m_paused=false, m_record=false, m_review=false;
    bool m_generator=false, m_generatorCommand=false, m_stepCollecting=false;
    double m_currentFrequency=0, m_referenceFloor=1e-6;
    qint64 m_startedMs=0, m_generatorMs=0, m_checkpointMs=0, m_stepMs=0;
    std::array<std::uint64_t,2> m_stepBlocks{0,0};
    std::vector<double> m_frequencies;
    std::shared_ptr<std::atomic<bool>> m_publishPending = std::make_shared<std::atomic<bool>>(false);
    std::atomic<bool> m_cancel{false};
};
