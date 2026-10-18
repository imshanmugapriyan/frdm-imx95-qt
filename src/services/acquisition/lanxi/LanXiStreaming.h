#pragma once
#include "LanXiAdvanced.h"
#include "LanXiAnalysis.h"
#include <cstdint>

namespace lanxi {
struct RunningStatistics {
    std::uint64_t count=0;
    long double mean=0, squares=0;
    double peak=0;
    void add(double value);
    double rms() const;
};
// Stateful, causal processing. State survives arbitrary input chunk boundaries.
// removeMean may appear only first and uses the measured whole-record mean.
// integrate is a bilinear leaky integrator, NOT the finite-window FFT operator.
class StreamingProcessor {
public:
    StreamingProcessor(double rate,const std::vector<ProcessStage>&stages,double mean);
    double process(double value);
private:
    struct Stage {std::string type;double value=0,previous=0,output=0,a=0,b=0;bool first=true;Biquad filter;};
    std::vector<Stage> m_stages;
    double m_rate;
};
}
