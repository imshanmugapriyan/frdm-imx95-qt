#pragma once
#include <complex>
#include <string>
#include <vector>

namespace lanxi {
// Offline operations on a finite, contiguous, calibrated sample window.
struct ProcessStage { std::string type; double value=0; };
std::vector<double> processSignal(const std::vector<double> &input, double rate,
                                 const std::vector<ProcessStage> &stages);
struct Correlation { std::vector<double> values; int firstLag=0; double delaySeconds=0, coefficient=0; };
Correlation correlate(const std::vector<double> &reference, const std::vector<double> &response, double rate);
struct OrderSettings {
    int pulsesPerRevolution=1, samplesPerRevolution=256, revolutionsPerBlock=4;
    double threshold=0, hysteresis=0.1, maximumOrder=20;
};
struct OrderResult {
    std::vector<double> rms, rpm, seconds;
    std::vector<std::vector<double>> map;
    double orderStep=0, minimumRpm=0, maximumRpm=0;
    int pulses=0, blocks=0;
};
OrderResult trackOrders(const std::vector<double> &signal,const std::vector<double> &tach,
                       double rate,const OrderSettings &settings);
struct ModalFit {
    double frequency=0, damping=0, relativeError=0, assurance=0;
    std::complex<double> residue{};
    std::vector<std::complex<double>> fitted;
};
// A single receptance pole fitted to complex FRF data; no MDOF/OMA inference.
ModalFit fitReceptance(const std::vector<double> &frequencies,
                      const std::vector<std::complex<double>> &response);
double modalAssurance(const std::vector<std::complex<double>> &a,
                      const std::vector<std::complex<double>> &b);
struct LimitPoint { double x=0, lower=0, upper=0; };
struct LimitResult { bool pass=false; int evaluated=0, failed=0, missing=0; double worstMargin=0; };
LimitResult checkLimits(const std::vector<double> &values,double start,double step,
                        const std::vector<LimitPoint> &curve);
}
