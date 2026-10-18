#pragma once

// Original, dependency-free implementation of the documented Web-XI wire
// format and signal calculations. No vendor SDK or generated parser is used.
#include <array>
#include <complex>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace lanxi {
struct Time {
    std::array<std::uint8_t, 4> family{};
    std::uint64_t count = 0;
    long double frequency() const;
    long double seconds() const;
    long double since(const Time &other) const;
};
struct Descriptor {
    unsigned mask = 0;
    std::uint16_t dataType = 0;
    double scale = 1, offset = 0;
    Time period;
    std::string unit;
    bool ready() const;
    double sampleRate() const;
};
struct Event {
    enum class Kind { Interpretation, Samples, Quality } kind;
    Time time;
    int signal = 0;
    Descriptor descriptor;
    std::vector<double> samples;
    std::uint16_t quality = 0;
};
class Decoder {
public:
    using Sink = std::function<void(const Event &)>;
    void feed(const char *data, std::size_t size, const Sink &sink);
    void finish() const;
    void reset();
    std::size_t pending() const { return m_bytes.size() - m_consumed; }
private:
    void message(const std::uint8_t *data, std::size_t size, unsigned type,
                 const Time &time, const Sink &sink);
    std::vector<std::uint8_t> m_bytes;
    std::size_t m_consumed = 0;
    Descriptor m_default;
    std::map<int, Descriptor> m_descriptors;
};

struct Spectrum {
    double sampleRate = 0, rms = 0, peak = 0, peakHz = 0;
    std::vector<double> rmsBins, psd;
};
struct Response {
    std::vector<double> magnitude, phase, coherence;
    std::vector<bool> valid;
};
class SpectralAverage {
public:
    explicit SpectralAverage(std::size_t size = 4096, const std::string &window = "hann",
                             int exponentialAverages = 0, bool removeMean = false);
    void reset();
    void add(const std::vector<double> &reference, const std::vector<double> &response,
             double sampleRate);
    Response response(double minimumReferenceRms) const;
    Spectrum spectrum() const;
    std::uint64_t averages() const { return m_count; }
private:
    std::size_t m_size;
    std::uint64_t m_count = 0;
    double m_rate = 0, m_windowSum = 0, m_windowSquareSum = 0;
    double m_rmsSquare = 0, m_peak = 0, m_weight = 0;
    int m_exponentialAverages = 0;
    bool m_removeMean = false;
    std::vector<double> m_window, m_xx, m_yy;
    std::vector<std::complex<double>> m_yx;
};
struct Tone {
    double rms = 0, phase = 0, residualRms = 0;
    std::complex<double> phasor{};
};
Tone fitTone(const std::vector<double> &values, double rate, double frequency);
double rms(const std::vector<double> &values);
double spl(double pressureRms);
std::vector<double> logarithmicSteps(double first, double last, int count);

// Only this gate may release the composite acquisition owner after Stop.
// A timeout is not an acknowledgement; unresolved modules keep ownership.
struct StopGate {
    std::array<bool, 2> touched{false, false};
    std::array<bool, 2> idle{false, false};
    bool outputTouched = false, outputOff = false;
    bool released() const;
};
}
