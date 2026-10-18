#pragma once
#include "LanXiCore.h"
#include <deque>
#include <limits>

namespace lanxi {
// Original engineering analysis. IEC instrument/filter conformance is not claimed.
struct Biquad {
    double b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
    double process(double value);
    double gain(double radians) const;
    static Biquad pass(double rate, double cutoff, bool highPass);
};
double weightingGain(double frequency, char weighting);
struct Level {
    double fast=0, slow=0, equivalent=0, maximumFast=0, peak=0;
};
class LevelMeter {
public:
    void configure(double rate);
    void add(const std::vector<double> &samples);
    Level level(char weighting) const;
    double seconds() const { return m_rate ? double(m_count)/m_rate : 0; }
private:
    double m_rate=0, m_fastAlpha=0, m_slowAlpha=0;
    std::uint64_t m_count=0;
    std::array<std::vector<Biquad>,3> m_filters;
    std::array<Level,3> m_levels;
    std::array<long double,3> m_square{};
};
struct Band { double center=0, low=0, high=0, rms=0; bool resolved=false; };
std::vector<Band> fractionalBands(const Spectrum &spectrum, int fraction, char weighting);
struct Distortion {
    double fundamental=0, thd=0, thdn=0, residual=0, sinadDb=0;
    int harmonics=0;
    std::vector<double> harmonicRms;
};
Distortion distortion(const std::vector<double> &values, double rate, double frequency, int harmonics);

// Bounded sample-triggered capture in one module's clock domain.
// Trigger coordinates are sample indices, never network arrival timestamps.
struct TriggerSettings {
    bool enabled=false, rising=true;
    double level=0, hysteresis=0, preSeconds=0.1, postSeconds=0.5;
};
class TriggerCapture {
public:
    void configure(const TriggerSettings &settings, double rate);
    void add(const std::vector<double> &samples);
    bool complete() const { return m_complete; }
    bool triggered() const { return m_triggered; }
    std::uint64_t index() const { return m_triggerIndex; }
    std::size_t preSamples() const { return m_preActual; }
    const std::vector<double> &values() const { return m_capture; }
private:
    TriggerSettings m_settings;
    std::size_t m_pre=0, m_post=0, m_preActual=0;
    std::uint64_t m_count=0, m_triggerIndex=0;
    bool m_armed=false, m_triggered=false, m_complete=false;
    std::deque<double> m_history;
    std::vector<double> m_capture;
};
}
