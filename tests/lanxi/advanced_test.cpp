#include "LanXiAdvanced.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
constexpr double pi=3.14159265358979323846;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    const double rate=8192;std::vector<double> x(32768),tach(x.size());
    for(std::size_t i=0;i<x.size();++i){double t=i/rate;x[i]=2*std::sin(2*pi*64*t);tach[i]=std::sin(2*pi*16*t);}
    auto differentiated=lanxi::processSignal(x,rate,{{"differentiate",0}});
    check(std::abs(differentiated[4096]-2*2*pi*64)<1,"Central differentiation amplitude");
    auto integrated=lanxi::processSignal(x,rate,{{"integrate",8}});
    std::cout<<"Integration centre: "<<integrated[16384]<<" expected "<<-2/(2*pi*64)<<'\n';
    check(std::abs(integrated[16384]+2/(2*pi*64))<5e-5,"Band-limited integration (finite-window edge error < 1%)");
    auto envelope=lanxi::processSignal(x,rate,{{"envelope",0}});
    check(std::abs(envelope[16384]-2)<.005,"Hilbert envelope amplitude");
    auto ordered=lanxi::trackOrders(x,tach,rate,{1,256,4,0,.2,20});
    auto peak=std::max_element(ordered.rms.begin(),ordered.rms.end());
    check(std::abs((peak-ordered.rms.begin())*ordered.orderStep-4)<.01,"Fourth order location");
    check(std::abs(*peak-std::sqrt(2.0))<.01,"Order RMS scaling");
    check(std::abs(ordered.minimumRpm-960)<.01&&ordered.blocks>10,"Tachometer RPM and multiple blocks");
    for(std::size_t i=0;i<x.size();++i)x[i]=2*std::sin(2*pi*4032*i/rate);
    auto alias=lanxi::trackOrders(x,tach,rate,{1,256,4,0,.2,20});
    check(alias.rms[16]<.002,"Angular resampling suppresses high-frequency alias into fourth order");
    std::vector<double> shortSignal(2560),shortTach(2560);
    for(std::size_t i=0;i<shortSignal.size();++i){shortSignal[i]=std::sin(2*pi*64*i/rate);shortTach[i]=std::sin(2*pi*16*i/rate-pi/2);}
    check(lanxi::trackOrders(shortSignal,shortTach,rate,{1,256,4,0,.2,20}).blocks==1,"Exactly one complete angular block is accepted");
    // A run-up whose response follows shaft angle, rather than a fixed-frequency tone.
    for(std::size_t i=0;i<x.size();++i){double t=i/rate,phase=2*pi*(10*t+3*t*t);tach[i]=std::sin(phase);x[i]=std::sin(3*phase);}
    ordered=lanxi::trackOrders(x,tach,rate,{1,256,4,0,.2,12});
    check(ordered.maximumRpm>ordered.minimumRpm+700,"Run-up RPM range");
    for(const auto &row:ordered.map)check(std::max_element(row.begin(),row.end())-row.begin()==12,"Angular resampling preserves third order during run-up");
    std::mt19937 rng(4);std::normal_distribution<double> noise;std::vector<double>a(4096),b(a.size());
    for(auto &v:a)v=noise(rng);for(std::size_t i=37;i<b.size();++i)b[i]=a[i-37];
    auto corr=lanxi::correlate(a,b,rate);check(std::abs(corr.delaySeconds-37/rate)<1e-10&&corr.coefficient>.98,"Correlation delay and sign");
    std::vector<double> f;std::vector<std::complex<double>> h;
    for(int k=0;k<101;++k){double hz=80+.4*k,w=2*pi*hz,wn=2*pi*100;f.push_back(hz);h.push_back(std::complex<double>(2,.3)/std::complex<double>(wn*wn-w*w,2*.025*wn*w));}
    auto modal=lanxi::fitReceptance(f,h);check(std::abs(modal.frequency-100)<.01&&std::abs(modal.damping-.025)<1e-4&&modal.relativeError<.001&&modal.assurance>.999,"Complex SDOF fit");
    check(std::abs(lanxi::modalAssurance({1,2},{2,4})-1)<1e-12,"MAC scale invariance");
    check(lanxi::modalAssurance({1,0},{0,1})==0,"MAC orthogonal shapes");
    check(lanxi::checkLimits({1,1.5,2},0,1,{{0,0,2},{2,1,3}}).pass,"Interpolated envelope pass");
    check(!lanxi::checkLimits({1,9,2},0,1,{{0,0,2},{2,1,3}}).pass,"Envelope exceedance");
    check(!lanxi::checkLimits({1,2},0,1,{{0,0,2},{2,1,3}}).pass,"Incomplete coverage must not pass");
    check(!lanxi::checkLimits({1,NAN,2},0,1,{{0,0,2},{2,1,3}}).pass,"Missing result must not pass");
    bool rejected=false;try{lanxi::processSignal(x,rate,{{"integrate",0}});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Reject unbounded integration");
    rejected=false;try{lanxi::trackOrders(x,std::vector<double>(x.size(),0),rate,{});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Reject missing tachometer");
    std::cout<<"Advanced processing, run-up orders, modal fit, correlation and limit checks passed.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
