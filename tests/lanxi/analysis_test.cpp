#include "LanXiAnalysis.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
constexpr double pi=3.14159265358979323846;
void check(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void near(double actual,double expected,double tolerance,const char *message) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance){std::cerr<<message<<": "<<actual<<" expected "<<expected<<'\n';throw std::runtime_error(message);}
}
std::vector<double> tone(std::size_t size,double rate,double hz,double peak=1) {
    std::vector<double> out(size);for(std::size_t i=0;i<size;++i)out[i]=peak*std::sin(2*pi*hz*i/rate);return out;
}
}
int main() {
    try {
        near(20*std::log10(lanxi::weightingGain(100,'A')),-19.14,0.03,"A weighting at 100 Hz");
        near(20*std::log10(lanxi::weightingGain(31.5,'C')),-3.03,0.03,"C weighting at 31.5 Hz");
        near(lanxi::weightingGain(1000,'A'),1,1e-12,"1 kHz weighting normalization");
        lanxi::LevelMeter meter;meter.configure(8000);meter.add(std::vector<double>(1000,1));
        near(meter.level('Z').fast,std::sqrt(1-std::exp(-1)),1e-10,"Fast 125 ms step response");
        meter.add(std::vector<double>(7000,1));
        near(meter.level('Z').slow,std::sqrt(1-std::exp(-1)),1e-10,"Slow one second step response");
        near(meter.level('Z').equivalent,1,1e-12,"Leq integration");
        meter.configure(65536);meter.add(tone(131072,65536,1000,std::sqrt(2.0)));
        near(meter.level('A').fast,1,0.001,"Digital A weighting 1 kHz tone");
        near(meter.level('C').fast,1,0.001,"Digital C weighting 1 kHz tone");
        auto block=tone(16384,65536,1000);
        for(const auto &window:{"hann","hamming","blackman","flat-top","rectangular"}) {
            lanxi::SpectralAverage fft(block.size(),window);fft.add(block,block,65536);const auto spectrum=fft.spectrum();
            near(spectrum.rmsBins[250],std::sqrt(0.5),1e-7,"Window coherent gain");
            double power=0;for(double v:spectrum.psd)power+=v*4;
            near(power,0.5,1e-7,"Window PSD energy");
        }
        lanxi::SpectralAverage fft(block.size());fft.add(block,block,65536);
        const auto bands=lanxi::fractionalBands(fft.spectrum(),3,'Z');bool found=false;
        for(const auto &b:bands)if(std::abs(b.center-1000)<1){check(b.resolved,"1 kHz band resolution");near(b.rms,std::sqrt(0.5),0.001,"Third-octave integrated tone energy");found=true;}
        check(found,"1 kHz band exists");check(!bands.front().resolved,"Unresolved low bands must be marked");
        lanxi::SpectralAverage exponential(1024,"hann",2);auto low=tone(1024,8192,1000),high=tone(1024,8192,1000,2);
        exponential.add(low,low,8192);exponential.add(high,high,8192);
        near(exponential.spectrum().rms,std::sqrt(1.5),1e-9,"Exponential power average normalization");
        auto nonBin=tone(8192,32768,997.3);const auto harmonic=tone(8192,32768,1994.6,0.1);
        for(std::size_t i=0;i<nonBin.size();++i)nonBin[i]+=harmonic[i]+0.3;
        const auto distortion=lanxi::distortion(nonBin,32768,997.3,5);
        near(distortion.thd,0.1,1e-9,"Joint non-bin harmonic THD fit");
        near(distortion.residual,0,1e-9,"Harmonic fit removes DC and harmonics");
        near(distortion.thdn,0.1,0.001,"THD+N excludes fundamental and DC");
        lanxi::TriggerCapture trigger;trigger.configure({true,true,1,0.1,0.003,0.003},1000);
        trigger.add({0,0,0,0.5});check(!trigger.triggered(),"Trigger stays armed below threshold");trigger.add({1,2});check(!trigger.complete(),"Capture waits for full post-trigger");trigger.add({3,4});
        check(trigger.complete()&&trigger.index()==4&&trigger.values().size()==6&&trigger.preSamples()==3,"Fragmented pre/post trigger capture");
        near(trigger.values()[0],0,0,"Pretrigger start");near(trigger.values().back(),3,0,"Posttrigger end");
        trigger.configure({true,true,1,0,0.005,0.001},1000);trigger.add({0,2,2,2,2,2,2});check(!trigger.triggered(),"Early crossing cannot trigger late without another edge");
        trigger.add({0,2});check(trigger.complete(),"Re-arm after early crossing");
        auto filter=lanxi::Biquad::pass(48000,1000,false);
        near(filter.gain(2*pi*1000/48000),std::sqrt(0.5),1e-10,"Butterworth cutoff");
        std::cout<<"Analysis checks passed: weighting, time response, Leq, windows, PSD, bands, exponential averaging, harmonic fit, trigger and filtering.\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
