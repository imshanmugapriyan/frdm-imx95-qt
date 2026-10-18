#pragma once
#include <QObject>
#include <QVariantMap>
#include <memory>
#include <vector>

class LanXiAudio : public QObject {
    Q_OBJECT
public:
    explicit LanXiAudio(QObject *parent=nullptr);
    ~LanXiAudio() override;
    void play(const std::vector<double> &samples,double rate,double fullScale,double volume);
    void mute();
    QVariantMap state() const;
signals:
    void changed();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
