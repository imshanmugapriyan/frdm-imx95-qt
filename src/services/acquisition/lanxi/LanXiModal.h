#pragma once
#include <complex>
#include <functional>
#include <vector>

namespace lanxi {
struct Mode {double frequency=0,damping=0;std::vector<std::complex<double>> residues;};
struct ModalModel {
    std::vector<Mode> modes;
    std::vector<std::vector<std::complex<double>>> fitted;
    std::vector<double> relativeError,frac;
    double objective=0;
    int iterations=0;
    bool converged=false;
};
// Common frequency/damping across calibrated receptance FRFs. Complex residues
// are solved by scaled, re-orthogonalized QR; bounded nonlinear variable
// projection refines the supplied frequency seeds. This is not polyreference EMA.
ModalModel fitModes(const std::vector<double> &hz,
                   const std::vector<std::vector<std::complex<double>>> &frfs,
                   const std::vector<double> &seeds,bool residualTerms=true,
                   const std::function<bool()> &cancelled={});
}
