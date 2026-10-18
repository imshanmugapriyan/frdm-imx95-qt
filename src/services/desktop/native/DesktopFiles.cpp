#include "DesktopFiles.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUuid>

namespace {
constexpr qint64 maximumNoteBytes = 1024 * 1024;
constexpr qint64 maximumPreviewBytes = 128 * 1024;
QString digest(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QVariantMap defaultPreferences() {
    return {{"wallpaper", "dusk"}, {"clock24", true}};
}
bool validPreferences(const QVariantMap &p) {
    return (p.value("wallpaper").toString() == "dusk" || p.value("wallpaper").toString() == "midnight")
        && p.value("clock24").metaType().id() == QMetaType::Bool;
}
}

QString DesktopFiles::outputDirectory() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("SunoDiagnostics");
}
void DesktopFiles::setError(const QString &message) { m_error = message; emit errorChanged(); }
QString DesktopFiles::notePath(const QString &id) const {
    static const QRegularExpression validId(QStringLiteral("^[a-f0-9]{32}$"));
    return validId.match(id).hasMatch() ? QDir(outputDirectory()).filePath("Notes/" + id + ".json") : QString();
}
QVariantMap DesktopFiles::loadNote(const QString &id) {
    setError({});
    const QString path = notePath(id);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile() || info.isSymLink() || info.size() > maximumNoteBytes * 6 + 4096) {
        setError(tr("This note is missing or cannot be opened.")); return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { setError(file.errorString()); return {}; }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) { setError(file.errorString()); return {}; }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    const auto object = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || object.value("schema").toInt() != 1 || !object.value("title").isString()
        || !object.value("text").isString() || object.value("text").toString().toUtf8().size() > maximumNoteBytes) {
        setError(tr("The saved note has an invalid format.")); return {};
    }
    return {{"id", id}, {"title", object.value("title").toString()},
            {"text", object.value("text").toString()}, {"version", digest(bytes)},
            {"modified", info.lastModified().toString(Qt::ISODate)}};
}
QVariantList DesktopFiles::listNotes() {
    setError({});
    const QDir directory(QDir(outputDirectory()).filePath("Notes"));
    if (!directory.exists()) return {};
    QVariantList notes;
    const auto entries = directory.entryInfoList({"*.json"}, QDir::Files | QDir::NoSymLinks, QDir::Time);
    for (const auto &entry : entries) {
        auto note = loadNote(entry.completeBaseName());
        if (note.isEmpty()) continue;
        note.remove("text"); note.remove("version");
        notes.append(note);
        if (notes.size() >= 100) break;
    }
    // Individual malformed files are omitted; opening/saving reports detailed errors.
    setError({}); return notes;
}
QVariantMap DesktopFiles::saveNote(const QString &id, const QString &title, const QString &text,
                                 const QString &expectedVersion) {
    setError({});
    if (title.trimmed().isEmpty() || title.size() > 120 || text.toUtf8().size() > maximumNoteBytes) {
        setError(tr("Use a title of 1-120 characters and a note smaller than 1 MiB.")); return {};
    }
    const QString savedId = id.isEmpty() ? QUuid::createUuid().toString(QUuid::Id128) : id;
    const QString path = notePath(savedId);
    if (path.isEmpty()) { setError(tr("Invalid note identifier.")); return {}; }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setError(tr("Cannot create the notes folder.")); return {};
    }
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0)) { setError(tr("This note is being saved by another instance. Try again.")); return {}; }
    if (!id.isEmpty()) {
        const auto previous = loadNote(id);
        if (previous.isEmpty()) return {};
        if (previous.value("version").toString() != expectedVersion) {
            setError(tr("This note changed outside Notepad. Your edits are retained; export them before reopening.")); return {};
        }
    } else if (QFileInfo::exists(path)) {
        setError(tr("Could not allocate a new note. Try Save again.")); return {};
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"title", title.trimmed()}, {"text", text}}).toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(file.errorString()); return {};
    }
    return {{"id", savedId}, {"title", title.trimmed()}, {"text", text}, {"version", digest(bytes)}};
}
QString DesktopFiles::exportNote(const QString &title, const QString &text) {
    setError({});
    const QByteArray bytes = text.toUtf8();
    if (bytes.size() > maximumNoteBytes) { setError(tr("The note exceeds the 1 MiB export limit.")); return {}; }
    if (!QDir().mkpath(outputDirectory())) { setError(tr("Cannot create the results folder.")); return {}; }
    QString prefix = title.left(48);
    prefix.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    const QString path = QDir(outputDirectory()).filePath("Note_" + prefix + "_"
        + QUuid::createUuid().toString(QUuid::Id128) + ".txt");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(file.errorString()); return {};
    }
    return path;
}
QVariantList DesktopFiles::listResults() {
    setError({}); QVariantList results;
    const QDir directory(outputDirectory());
    if (!directory.exists()) return results;
    const auto entries = directory.entryInfoList({"*.csv", "*.txt"}, QDir::Files | QDir::NoSymLinks, QDir::Time);
    for (const auto &entry : entries) {
        results.append(QVariantMap{{"name", entry.fileName()}, {"bytes", entry.size()},
                                   {"modified", entry.lastModified().toString(Qt::ISODate)}});
        if (results.size() >= 1000) break;
    }
    return results;
}
QVariantMap DesktopFiles::readResult(const QString &name) {
    setError({});
    const QFileInfo info(QDir(outputDirectory()).filePath(name));
    const QString extension = info.suffix().toLower();
    if (name.isEmpty() || QFileInfo(name).fileName() != name || name.contains('\\')
        || !info.isFile() || info.isSymLink() || (extension != "csv" && extension != "txt")) {
        setError(tr("Select a CSV or text file from the results list.")); return {};
    }
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) { setError(file.errorString()); return {}; }
    const QByteArray bytes = file.read(maximumPreviewBytes);
    if (file.error() != QFileDevice::NoError) { setError(file.errorString()); return {}; }
    return {{"name", name}, {"text", QString::fromUtf8(bytes)}, {"truncated", file.size() > bytes.size()}};
}
QVariantMap DesktopFiles::storageInfo() const {
    QString existing = outputDirectory();
    while (!QFileInfo::exists(existing)) {
        const QString parent = QFileInfo(existing).absolutePath();
        if (parent == existing) break;
        existing = parent;
    }
    const QStorageInfo storage(existing);
    const bool available = storage.isValid() && storage.isReady() && storage.bytesTotal() > 0;
    return {{"available", available}, {"freeBytes", available ? storage.bytesAvailable() : 0},
            {"totalBytes", available ? storage.bytesTotal() : 0}, {"platform", QSysInfo::prettyProductName()}};
}
QString DesktopFiles::preferencesPath() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("desktop.json");
}
QVariantMap DesktopFiles::loadPreferences() {
    setError({}); QFile file(preferencesPath());
    if (!file.exists()) return defaultPreferences();
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4096) {
        setError(tr("Desktop preferences could not be read. Using defaults.")); return defaultPreferences();
    }
    const auto p = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    if (!validPreferences(p)) { setError(tr("Invalid desktop preferences. Using defaults.")); return defaultPreferences(); }
    return {{"wallpaper", p.value("wallpaper")}, {"clock24", p.value("clock24")}};
}
bool DesktopFiles::savePreferences(const QVariantMap &p) {
    setError({});
    if (!validPreferences(p)) { setError(tr("Invalid desktop preferences.")); return false; }
    const QString path = preferencesPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { setError(tr("Cannot create the settings folder.")); return false; }
    const QByteArray bytes = QJsonDocument(QJsonObject{{"wallpaper", p.value("wallpaper").toString()},
                                                     {"clock24", p.value("clock24").toBool()}}).toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(file.errorString()); return false;
    }
    return true;
}
