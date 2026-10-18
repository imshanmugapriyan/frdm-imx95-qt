#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class DesktopFiles : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString outputDirectory READ outputDirectory CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit DesktopFiles(QObject *parent = nullptr) : QObject(parent) {}
    QString outputDirectory() const;
    QString error() const { return m_error; }
    Q_INVOKABLE QVariantList listNotes();
    Q_INVOKABLE QVariantMap loadNote(const QString &id);
    Q_INVOKABLE QVariantMap saveNote(const QString &id, const QString &title,
                                    const QString &text, const QString &expectedVersion);
    Q_INVOKABLE QString exportNote(const QString &title, const QString &text);
    Q_INVOKABLE QVariantList listResults();
    Q_INVOKABLE QVariantMap readResult(const QString &name);
    Q_INVOKABLE QVariantMap storageInfo() const;
    Q_INVOKABLE QVariantMap loadPreferences();
    Q_INVOKABLE bool savePreferences(const QVariantMap &preferences);
signals:
    void errorChanged();
private:
    void setError(const QString &message);
    QString notePath(const QString &id) const;
    QString preferencesPath() const;
    QString m_error;
};
