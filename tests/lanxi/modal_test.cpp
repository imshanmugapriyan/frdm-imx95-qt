#include "LanXiModal.h"
#include "LanXiAdvanced.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using C=std::complex<double>;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
    constexpr double pi=3.14159265358979323846;std::vector<double> hz;std::vector<std::vector<C>> h(3);
    for(int k=0;k<500;++k){double f=70+.15*k,w=2*pi*f;hz.push_back(f);for(int ch=0;ch<3;++ch){C value=C(2e-7,-1e-8)+C(.002,.001)/(f*f);for(int mode=0;mode<2;++mode){double wn=2*pi*(mode?112:100),z=mode?.035:.018;C residue=std::polar(double((ch+1)*(mode+1)),.2*ch-.3*mode);value+=residue/C(wn*wn-w*w,2*z*wn*w);}h[ch].push_back(value);}}
    auto fit=lanxi::fitModes(hz,h,{98,115},true);
    check(std::abs(fit.modes[0].frequency-100)<.01&&std::abs(fit.modes[1].frequency-112)<.01,"Two-mode natural frequency recovery");check(std::abs(fit.modes[0].damping-.018)<.0001&&std::abs(fit.modes[1].damping-.035)<.0001,"Two-mode damping recovery");
    for(auto error:fit.relativeError)check(error<1e-5,"Joint FRF reconstruction");for(auto frac:fit.frac)check(frac>.999999,"FRAC");
    std::vector<C> expected;for(int ch=0;ch<3;++ch)expected.push_back(std::polar(double(ch+1),.2*ch));check(lanxi::modalAssurance(expected,fit.modes[0].residues)>.999999,"Complex mode residue shape");
    auto noisy=h;for(std::size_t c=0;c<noisy.size();++c)for(std::size_t k=0;k<noisy[c].size();++k)noisy[c][k]+=std::abs(h[c][k])*.002*C(std::sin(k*1.71+c),std::cos(k*.93+c));
    auto noisyFit=lanxi::fitModes(hz,noisy,{98,115},true);check(std::abs(noisyFit.modes[0].frequency-100)<.03&&std::abs(noisyFit.modes[1].frequency-112)<.03,"Multi-mode fit with deterministic measurement noise");check(noisyFit.relativeError[0]<.003,"Noisy fit residual");
    bool bad=false;try{lanxi::fitModes(hz,h,{115,98});}catch(const std::exception&){bad=true;}check(bad,"Unsorted seeds rejected");bad=false;try{lanxi::fitModes(hz,h,{98,115},true,[]{return true;});}catch(const std::exception&){bad=true;}check(bad,"Modal cancellation");
    std::cout<<"Joint multi-mode fit, damping, residuals, FRAC, shape and cancellation checks passed.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
