#include "DiagnosticFiles.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString safeName(QString value) {
    value.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")),QStringLiteral("_"));
    return value.left(48);
}
QString uniqueName(const QString &prefix) {
    return safeName(prefix)+"_"+QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")
           +"_"+QUuid::createUuid().toString(QUuid::Id128).left(8)+".csv";
}
}
QString DiagnosticFiles::outputDirectory() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("SunoDiagnostics");
}
void DiagnosticFiles::setError(const QString &value) { m_error=value; emit errorChanged(); }
QString DiagnosticFiles::exportCsv(const QString &prefix, const QString &content) {
    setError({});
    if (!QDir().mkpath(outputDirectory())) { setError(tr("Cannot create the results folder.")); return {}; }
    const QString path=QDir(outputDirectory()).filePath(uniqueName(prefix));
    QSaveFile file(path);
    const auto bytes=content.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {
        setError(file.errorString()); return {};
    }
    return path;
}
QString DiagnosticFiles::profilePath(const QString &name) const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(safeName(name)+".json");
}
bool DiagnosticFiles::saveProfile(const QString &name, const QVariantMap &settings) {
    setError({});
    const QString path=profilePath(name);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { setError(tr("Cannot create the settings folder.")); return false; }
    QSaveFile file(path);
    const auto bytes=QJsonDocument(QJsonObject::fromVariantMap(settings)).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {
        setError(file.errorString()); return false;
    }
    return true;
}
QVariantMap DiagnosticFiles::loadProfile(const QString &name) {
    setError({}); QFile file(profilePath(name));
    if (!file.open(QIODevice::ReadOnly)) { setError(file.errorString()); return {}; }
    if (file.size()>65536) { setError(tr("Profile is too large.")); return {}; }
    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(file.readAll(),&error);
    if (error.error!=QJsonParseError::NoError || !document.isObject()) { setError(tr("Invalid settings profile.")); return {}; }
    return document.object().toVariantMap();
}
QString DiagnosticFiles::beginRecording() {
    setError({});
    if (m_recording.isOpen()) { setError(tr("A recording is already open.")); return {}; }
    if (!QDir().mkpath(outputDirectory())) { setError(tr("Cannot create the results folder.")); return {}; }
    m_recording.setFileName(QDir(outputDirectory()).filePath(uniqueName("LiveElectrodogram")));
    if (!m_recording.open(QIODevice::WriteOnly|QIODevice::NewOnly)) { setError(m_recording.errorString()); return {}; }
    QString header="sequence,batch_elapsed_ms";
    for (int ch=1;ch<=16;++ch) header+=",E"+QString::number(ch);
    if (!appendRecording(header+"\n")) return {};
    return m_recording.fileName();
}
bool DiagnosticFiles::appendRecording(const QString &rows) {
    if (!m_recording.isOpen()) { setError(tr("No recording is open.")); return false; }
    const auto bytes=rows.toUtf8();
    if (m_recording.write(bytes)!=bytes.size()) { setError(m_recording.errorString()); m_recording.close(); return false; }
    return true;
}
bool DiagnosticFiles::finishRecording() {
    if (!m_recording.isOpen()) return true;
    const bool ok=m_recording.flush();
    if (!ok) setError(m_recording.errorString());
    m_recording.close(); return ok;
}
