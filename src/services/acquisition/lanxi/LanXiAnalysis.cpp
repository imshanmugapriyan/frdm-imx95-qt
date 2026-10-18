#include "LanXiAnalysis.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace lanxi {
namespace {
constexpr double pi=3.14159265358979323846;
void require(bool ok,const char *message) { if(!ok)throw std::runtime_error(message); }
int weightIndex(char w) { require(w=='Z'||w=='A'||w=='C',"Weighting must be Z, A or C.");return w=='A'?1:w=='C'?2:0; }
Biquad section(double rate,double p1,double p2,double z1,double z2) {
    const double a=(2*rate-2*pi*p1)/(2*rate+2*pi*p1);
    const double b=(2*rate-2*pi*p2)/(2*rate+2*pi*p2);
    return {1,-z1-z2,z1*z2,-a-b,a*b};
}
}
double Biquad::process(double value) {
    const double y=b0*value+z1;z1=b1*value-a1*y+z2;z2=b2*value-a2*y;return y;
}
double Biquad::gain(double radians) const {
    const auto z=std::polar(1.0,-radians);
    return std::abs((b0+b1*z+b2*z*z)/(1.0+a1*z+a2*z*z));
}
Biquad Biquad::pass(double rate,double cutoff,bool high) {
    require(std::isfinite(rate)&&rate>0&&std::isfinite(cutoff)&&cutoff>0&&cutoff<rate/2,"Invalid filter cutoff.");
    const double c=std::cos(2*pi*cutoff/rate),a=std::sin(2*pi*cutoff/rate)/std::sqrt(2.0),d=1+a;
    return high?Biquad{(1+c)/(2*d),-(1+c)/d,(1+c)/(2*d),-2*c/d,(1-a)/d}
               :Biquad{(1-c)/(2*d),(1-c)/d,(1-c)/(2*d),-2*c/d,(1-a)/d};
}
double weightingGain(double f,char w) {
    weightIndex(w);require(std::isfinite(f)&&f>=0,"Invalid weighting frequency.");
    if(w=='Z')return 1;if(f==0)return 0;
    const auto response=[w](double hz) {
        const double f2=hz*hz,lo=20.598997*20.598997,hi=12194.217*12194.217;
        double r=hi*f2/((f2+lo)*(f2+hi));
        if(w=='A')r*=f2/std::sqrt((f2+107.65265*107.65265)*(f2+737.86223*737.86223));
        return r;
    };
    return response(f)/response(1000);
}
void LevelMeter::configure(double rate) {
    require(std::isfinite(rate)&&rate>2000&&rate<=262144,"Weighted levels require a sample rate above 2 kHz.");
    m_rate=rate;m_count=0;m_levels={};m_square={};m_filters={};
    m_fastAlpha=-std::expm1(-1/(rate*0.125));m_slowAlpha=-std::expm1(-1/rate);
    for(int index=1;index<=2;++index) {
        auto &filters=m_filters[index];
        filters.push_back(section(rate,20.598997,20.598997,1,1));
        filters.push_back(section(rate,12194.217,12194.217,-1,-1));
        if(index==1)filters.push_back(section(rate,107.65265,737.86223,1,1));
        double g=1;for(const auto &f:filters)g*=f.gain(2*pi*1000/rate);
        filters[0].b0/=g;filters[0].b1/=g;filters[0].b2/=g;
    }
}
void LevelMeter::add(const std::vector<double> &samples) {
    require(m_rate>0,"Level meter is not configured.");
    for(double sample:samples) {
        require(std::isfinite(sample),"Invalid level sample.");++m_count;
        for(int i=0;i<3;++i) {
            double v=sample;for(auto &f:m_filters[i])v=f.process(v);
            const double square=v*v;auto &l=m_levels[i];
            l.fast+=m_fastAlpha*(square-l.fast);l.slow+=m_slowAlpha*(square-l.slow);
            l.maximumFast=std::max(l.maximumFast,l.fast);l.peak=std::max(l.peak,std::abs(v));m_square[i]+=square;
        }
    }
}
Level LevelMeter::level(char w) const {
    const int i=weightIndex(w);auto l=m_levels[i];
    l.fast=std::sqrt(l.fast);l.slow=std::sqrt(l.slow);l.maximumFast=std::sqrt(l.maximumFast);
    l.equivalent=m_count?std::sqrt(double(m_square[i]/m_count)):0;return l;
}
std::vector<Band> fractionalBands(const Spectrum &s,int fraction,char weighting) {
    require(fraction==1||fraction==3,"Choose octave or third-octave bands.");weightIndex(weighting);
    if(s.psd.size()<2||s.sampleRate<=0)return {};
    const double df=s.sampleRate/(2*(s.psd.size()-1));std::vector<Band> out;
    // Base-ten preferred-frequency definition, anchored at 1 kHz.
    for(int band=-6*fraction;band<=5*fraction;++band) {
        Band b;b.center=1000*std::pow(10.0,0.3*band/fraction);
        b.low=b.center/std::pow(10.0,0.15/fraction);b.high=b.center*std::pow(10.0,0.15/fraction);
        if(b.center<20||b.center>20000)continue;
        b.resolved=b.high<=s.sampleRate/2.56 && b.high-b.low>=3*df;
        double power=0;
        if(b.resolved)for(std::size_t k=1;k<s.psd.size();++k) {
            const double overlap=std::max(0.0,std::min(b.high,(k+0.5)*df)-std::max(b.low,(k-0.5)*df));
            const double gain=weightingGain(k*df,weighting);power+=s.psd[k]*overlap*gain*gain;
        }
        b.rms=std::sqrt(std::max(0.0,power));out.push_back(b);
    }
    return out;
}
Distortion distortion(const std::vector<double> &v,double rate,double hz,int count) {
    require(v.size()>=32&&v.size()<=65536&&std::isfinite(rate)&&rate>0&&std::isfinite(hz)&&hz>0
        &&v.size()*hz/rate>=4&&hz<rate/2.56&&count>=2&&count<=10,"Invalid harmonic analysis range or insufficient cycles.");
    count=std::min(count,int(std::floor(rate/2.56/hz)));require(count>=2,"No harmonics fit the measurement bandwidth.");
    const int n=2*count+1;std::vector<std::vector<double>> matrix(n,std::vector<double>(n+1));
    std::vector<double> basis(n);basis[0]=1;
    auto fill=[&](std::size_t i){for(int h=1;h<=count;++h){basis[2*h-1]=std::cos(2*pi*hz*h*i/rate);basis[2*h]=std::sin(2*pi*hz*h*i/rate);}};
    for(std::size_t i=0;i<v.size();++i) {
        require(std::isfinite(v[i]),"Invalid harmonic sample.");fill(i);
        for(int r=0;r<n;++r){for(int c=0;c<n;++c)matrix[r][c]+=basis[r]*basis[c];matrix[r][n]+=basis[r]*v[i];}
    }
    for(int c=0;c<n;++c) {
        int pivot=c;for(int r=c+1;r<n;++r)if(std::abs(matrix[r][c])>std::abs(matrix[pivot][c]))pivot=r;
        std::swap(matrix[pivot],matrix[c]);const double d=matrix[c][c];require(std::abs(d)>1e-10,"Singular harmonic fit.");
        for(int k=c;k<=n;++k)matrix[c][k]/=d;
        for(int r=0;r<n;++r)if(r!=c){const double f=matrix[r][c];for(int k=c;k<=n;++k)matrix[r][k]-=f*matrix[c][k];}
    }
    Distortion out;out.harmonics=count;double harmonic=0,residual=0,notched=0;
    for(int h=1;h<=count;++h){const double square=(matrix[2*h-1][n]*matrix[2*h-1][n]+matrix[2*h][n]*matrix[2*h][n])/2;
        out.harmonicRms.push_back(std::sqrt(square));if(h>1)harmonic+=square;}
    out.fundamental=out.harmonicRms.front();require(out.fundamental>1e-12,"Fundamental is below the analysis floor.");
    for(std::size_t i=0;i<v.size();++i){fill(i);double fitted=0;for(int k=0;k<n;++k)fitted+=matrix[k][n]*basis[k];
        residual+=(v[i]-fitted)*(v[i]-fitted);const double noise=v[i]-matrix[0][n]-matrix[1][n]*basis[1]-matrix[2][n]*basis[2];notched+=noise*noise;}
    out.residual=std::sqrt(residual/v.size());out.thd=std::sqrt(harmonic)/out.fundamental;
    out.thdn=std::sqrt(notched/v.size())/out.fundamental;
    out.sinadDb=out.thdn>0?-20*std::log10(out.thdn):std::numeric_limits<double>::infinity();return out;
}
void TriggerCapture::configure(const TriggerSettings &s,double rate) {
    if(!s.enabled){*this=TriggerCapture{};return;}
    require(std::isfinite(rate)&&rate>0&&std::isfinite(s.level)&&std::isfinite(s.hysteresis)&&s.hysteresis>=0
        &&std::isfinite(s.preSeconds)&&std::isfinite(s.postSeconds)&&s.preSeconds>=0&&s.postSeconds>0
        &&(s.preSeconds+s.postSeconds)*rate<=1048576,"Trigger capture exceeds 1048576 samples or has invalid settings.");
    *this=TriggerCapture{};m_settings=s;m_pre=std::size_t(std::llround(s.preSeconds*rate));m_post=std::max<std::size_t>(1,std::size_t(std::llround(s.postSeconds*rate)));
}
void TriggerCapture::add(const std::vector<double> &samples) {
    if(!m_settings.enabled||m_complete)return;
    for(double v:samples) {
        require(std::isfinite(v),"Invalid trigger sample.");
        if(!m_triggered) {
            const bool crossing=m_settings.rising?v>=m_settings.level:v<=m_settings.level;
            if(m_armed&&crossing) {
                if(m_history.size()>=m_pre){m_triggered=true;m_triggerIndex=m_count;m_preActual=m_history.size();m_capture.assign(m_history.begin(),m_history.end());}
                m_armed=false;
            }else if(m_settings.rising?v<m_settings.level-m_settings.hysteresis:v>m_settings.level+m_settings.hysteresis)m_armed=true;
        }
        if(m_triggered) {m_capture.push_back(v);if(m_capture.size()>=m_preActual+m_post){m_complete=true;break;}}
        else {m_history.push_back(v);while(m_history.size()>m_pre)m_history.pop_front();}
        ++m_count;
    }
}
}
