#pragma once

#include <QObject>
#include <QFile>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class DiagnosticFiles : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString outputDirectory READ outputDirectory CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit DiagnosticFiles(QObject *parent=nullptr) : QObject(parent) { m_clock.start(); }
    QString outputDirectory() const;
    QString error() const { return m_error; }
    Q_INVOKABLE QString exportCsv(const QString &prefix, const QString &content);
    Q_INVOKABLE bool saveProfile(const QString &name, const QVariantMap &settings);
    Q_INVOKABLE QVariantMap loadProfile(const QString &name);
    Q_INVOKABLE QString beginRecording();
    Q_INVOKABLE bool appendRecording(const QString &rows);
    Q_INVOKABLE bool finishRecording();
    Q_INVOKABLE qint64 monotonicMs() const { return m_clock.elapsed(); }
signals:
    void errorChanged();
private:
    void setError(const QString &error);
    QString profilePath(const QString &name) const;
    QFile m_recording;
    QElapsedTimer m_clock;
    QString m_error;
};
