#include "LanXiAudio.h"
#include <QString>
#include <algorithm>
#include <cmath>
#ifdef SUNO_LANXI_AUDIO
#include <QAudioSink>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QBuffer>
#include <QMediaDevices>
#include <QtEndian>
#endif

struct LanXiAudio::Impl {
    QString message="Muted";
    bool playing=false;
    int clipped=0;
#ifdef SUNO_LANXI_AUDIO
    QAudioSink *sink=nullptr;
    QBuffer *buffer=nullptr;
#endif
};
LanXiAudio::LanXiAudio(QObject *parent):QObject(parent),m_impl(std::make_unique<Impl>()){}
LanXiAudio::~LanXiAudio(){mute();}
QVariantMap LanXiAudio::state() const {
#ifdef SUNO_LANXI_AUDIO
    constexpr bool available=true;
#else
    constexpr bool available=false;
#endif
    return {{"available",available},{"playing",m_impl->playing},{"clipped",m_impl->clipped},{"message",m_impl->message}};
}
void LanXiAudio::mute() {
#ifdef SUNO_LANXI_AUDIO
    if(m_impl->sink){disconnect(m_impl->sink,nullptr,this,nullptr);m_impl->sink->reset();delete m_impl->sink;m_impl->sink=nullptr;}
    delete m_impl->buffer;m_impl->buffer=nullptr;
#endif
    m_impl->playing=false;m_impl->message="Muted";emit changed();
}
void LanXiAudio::play(const std::vector<double> &samples,double rate,double fullScale,double volume) {
    mute();m_impl->clipped=0;
#ifndef SUNO_LANXI_AUDIO
    Q_UNUSED(samples);Q_UNUSED(rate);Q_UNUSED(fullScale);Q_UNUSED(volume);
    m_impl->message="Audio playback requires an approved build with SUNO_LANXI_AUDIO=ON and Qt Multimedia.";
#else
    if(samples.size()<2||samples.size()>262144||!std::isfinite(rate)||rate<1||!std::isfinite(fullScale)||fullScale<=0||!std::isfinite(volume)||volume<0||volume>1){m_impl->message="Invalid audio window, full scale or volume.";emit changed();return;}
    const auto device=QMediaDevices::defaultAudioOutput();QAudioFormat format;format.setSampleRate(48000);format.setChannelCount(2);format.setSampleFormat(QAudioFormat::Int16);
    if(device.isNull()||!device.isFormatSupported(format)){m_impl->message="The default audio output does not support stereo 48 kHz PCM16.";emit changed();return;}
    const auto count=std::size_t(std::floor((samples.size()-1)*48000/rate));
    if(count>48000*120){m_impl->message="Audio preview is limited to 120 seconds.";emit changed();return;}
    QByteArray pcm;pcm.resize(qsizetype(count*4));
    // Windowed-sinc reconstruction (32 samples each side), with anti-alias cutoff.
    // Listening is an audition path; it is never used for measurement results.
    constexpr double pi=3.14159265358979323846;const double cutoff=.45*std::min(1.0,48000/rate);
    for(std::size_t i=0;i<count;++i){double position=i*rate/48000;auto center=qint64(position);double value=0,weight=0;
        for(int tap=-32;tap<=32;++tap){auto index=center+tap;if(index<0||index>=qint64(samples.size()))continue;double x=position-index;
            if(std::abs(x)>32)continue;double argument=2*pi*cutoff*x;double kernel=(std::abs(argument)<1e-10?2*cutoff:std::sin(argument)/(pi*x))*(.5+.5*std::cos(pi*x/32));
            value+=samples[std::size_t(index)]*kernel;weight+=kernel;}
        value=weight!=0?value/(weight*fullScale):0;if(!std::isfinite(value))value=0;if(std::abs(value)>1)++m_impl->clipped;
        const auto sample=qint16(std::round(std::clamp(value,-1.0,1.0)*32767));qToLittleEndian<qint16>(sample,pcm.data()+i*4);qToLittleEndian<qint16>(sample,pcm.data()+i*4+2);
    }
    m_impl->buffer=new QBuffer(this);m_impl->buffer->setData(pcm);m_impl->buffer->open(QIODevice::ReadOnly);
    m_impl->sink=new QAudioSink(device,format,this);m_impl->sink->setVolume(volume);
    connect(m_impl->sink,&QAudioSink::stateChanged,this,[this](QtAudio::State state){
        if(state==QtAudio::IdleState||state==QtAudio::StoppedState){m_impl->playing=false;m_impl->message=m_impl->sink->error()==QtAudio::NoError?"Playback finished":"Audio output failed";emit changed();}
    });
    m_impl->playing=true;m_impl->message=QString("Playing selected analysis window; %1 clipped audio samples").arg(m_impl->clipped);m_impl->sink->start(m_impl->buffer);
#endif
    emit changed();
}
