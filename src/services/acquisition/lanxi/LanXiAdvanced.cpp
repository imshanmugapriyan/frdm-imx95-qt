#include "LanXiAdvanced.h"
#include "LanXiAnalysis.h"
#include "LanXiCore.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace lanxi {
namespace {
constexpr double pi=3.14159265358979323846;
using Complex=std::complex<double>;
void require(bool condition,const char *message) {if(!condition)throw std::invalid_argument(message);}
std::size_t power2(std::size_t n) {std::size_t size=1;while(size<n)size*=2;return size;}
void transform(std::vector<Complex> &v,bool inverse=false) {
    const auto n=v.size();
    for(std::size_t i=1,j=0;i<n;++i){auto b=n>>1;for(;j&b;b>>=1)j^=b;j^=b;if(i<j)std::swap(v[i],v[j]);}
    for(std::size_t size=2;size<=n;size*=2){const auto step=std::polar(1.0,(inverse?2:-2)*pi/double(size));
        for(std::size_t first=0;first<n;first+=size){Complex w=1;for(std::size_t k=0;k<size/2;++k){auto a=v[first+k],b=w*v[first+k+size/2];v[first+k]=a+b;v[first+k+size/2]=a-b;w*=step;}}}
    if(inverse)for(auto &x:v)x/=double(n);
}
void samples(const std::vector<double> &v,double rate) {
    require(v.size()>=16&&v.size()<=1048576,"Use 16 to 1048576 contiguous samples.");
    require(std::isfinite(rate)&&rate>0,"Invalid sample rate.");
    require(std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x);}),"Samples contain invalid values.");
}
double at(const std::vector<double> &v,double position,double increment) {
    const double cutoff=.45*std::min(1.0,1.0/increment);double sum=0,weight=0;auto center=static_cast<long long>(position);
    for(int tap=-32;tap<=32;++tap){const auto index=center+tap;if(index<0||index>=static_cast<long long>(v.size()))continue;
        const double x=position-double(index);if(std::abs(x)>32)continue;
        const double angle=2*pi*cutoff*x;const double kernel=(std::abs(angle)<1e-10?2*cutoff:std::sin(angle)/(pi*x))*(.5+.5*std::cos(pi*x/32));
        sum+=v[std::size_t(index)]*kernel;weight+=kernel;}
    return weight!=0?sum/weight:0;
}
}
std::vector<double> processSignal(const std::vector<double> &input,double rate,const std::vector<ProcessStage> &stages) {
    samples(input,rate);require(stages.size()<=16,"At most 16 processing stages are supported.");auto out=input;
    for(const auto &stage:stages) {
        require(std::isfinite(stage.value),"Invalid stage parameter.");
        if(stage.type=="gain") {require(std::abs(stage.value)<=1e9,"Gain is too large.");for(auto &v:out)v*=stage.value;}
        else if(stage.type=="removeMean") {double mean=std::accumulate(out.begin(),out.end(),0.0)/double(out.size());for(auto &v:out)v-=mean;}
        else if(stage.type=="highPass"||stage.type=="lowPass") {
            require(stage.value>0&&stage.value<rate/2,"Filter cutoff must be inside Nyquist.");
            auto f=Biquad::pass(rate,stage.value,stage.type=="highPass");for(auto &v:out)v=f.process(v);
        } else if(stage.type=="differentiate") {
            auto before=out;out.front()=(before[1]-before[0])*rate;out.back()=(before.back()-before[before.size()-2])*rate;
            for(std::size_t i=1;i+1<out.size();++i)out[i]=(before[i+1]-before[i-1])*rate/2;
        } else if(stage.type=="integrate"||stage.type=="envelope") {
            // Zero-padding reduces circular wrap; integration is band limited and zero-DC.
            if(stage.type=="integrate")require(stage.value>0&&stage.value<rate/2,"Integration needs a positive low-frequency cutoff.");
            std::vector<Complex> bins(power2(out.size()*2));std::copy(out.begin(),out.end(),bins.begin());transform(bins);
            const auto n=bins.size();for(std::size_t k=0;k<n;++k) {
                if(stage.type=="envelope") {if(k>0&&k<n/2)bins[k]*=2;else if(k>n/2)bins[k]=0;}
                else {double f=(k<=n/2?double(k):double(k)-double(n))*rate/double(n);
                    bins[k]=std::abs(f)<stage.value||k==n/2?Complex{}:bins[k]/Complex(0,2*pi*f);}
            }
            transform(bins,true);for(std::size_t i=0;i<out.size();++i)out[i]=stage.type=="envelope"?std::abs(bins[i]):bins[i].real();
        } else throw std::invalid_argument("Unknown processing stage.");
        require(std::all_of(out.begin(),out.end(),[](double x){return std::isfinite(x);}),"Processing overflow.");
    }
    return out;
}
Correlation correlate(const std::vector<double> &reference,const std::vector<double> &response,double rate) {
    samples(reference,rate);samples(response,rate);require(reference.size()==response.size(),"Correlation requires aligned equal-length channels.");
    auto a=processSignal(reference,rate,{{"removeMean",0}}),b=processSignal(response,rate,{{"removeMean",0}});
    const double norm=std::sqrt(std::inner_product(a.begin(),a.end(),a.begin(),0.0)*std::inner_product(b.begin(),b.end(),b.begin(),0.0));
    require(norm>1e-30,"Correlation requires non-zero AC energy.");
    const auto n=power2(2*a.size());std::vector<Complex>x(n),y(n);std::copy(a.begin(),a.end(),x.begin());std::copy(b.begin(),b.end(),y.begin());
    transform(x);transform(y);for(std::size_t i=0;i<n;++i)y[i]*=std::conj(x[i]);transform(y,true);
    Correlation r;r.firstLag=-int(a.size()/2);double best=-1;
    for(int lag=r.firstLag;lag<=-r.firstLag;++lag) {double value=y[lag<0?n-std::size_t(-lag):std::size_t(lag)].real()/norm;r.values.push_back(value);
        if(std::abs(value)>best){best=std::abs(value);r.delaySeconds=lag/rate;r.coefficient=value;}}
    return r;
}
OrderResult trackOrders(const std::vector<double>& signal,const std::vector<double>& tach,double rate,const OrderSettings &s) {
    samples(signal,rate);samples(tach,rate);require(signal.size()==tach.size(),"Tachometer and response must share a sample grid.");
    require(s.pulsesPerRevolution>=1&&s.pulsesPerRevolution<=1024,"Pulses per revolution must be 1..1024.");
    require(s.samplesPerRevolution>=32&&s.samplesPerRevolution<=4096&&(s.samplesPerRevolution&(s.samplesPerRevolution-1))==0,"Angular samples must be a power of two, 32..4096.");
    require(s.revolutionsPerBlock>=1&&s.revolutionsPerBlock<=32&&(s.revolutionsPerBlock&(s.revolutionsPerBlock-1))==0,"Block revolutions must be 1, 2, 4, 8, 16 or 32.");
    require(std::isfinite(s.threshold)&&std::isfinite(s.hysteresis)&&s.hysteresis>0,"Positive tachometer hysteresis required.");
    require(std::isfinite(s.maximumOrder)&&s.maximumOrder>0&&s.maximumOrder<s.samplesPerRevolution/2.56,"Maximum order exceeds angular bandwidth.");
    std::vector<double> edges;bool armed=false;
    for(std::size_t i=1;i<tach.size();++i){if(tach[i-1]<=s.threshold-s.hysteresis)armed=true;
        if(armed&&tach[i-1]<s.threshold&&tach[i]>=s.threshold){edges.push_back(double(i-1)+(s.threshold-tach[i-1])/(tach[i]-tach[i-1]));armed=false;}}
    require(edges.size()>=std::size_t(s.pulsesPerRevolution*s.revolutionsPerBlock+1),"Not enough tachometer pulses for a complete order block.");
    const double pointsPerPulse=double(s.samplesPerRevolution)/s.pulsesPerRevolution;
    const auto total=std::size_t(std::floor((edges.size()-1)*pointsPerPulse));require(total<=4194304,"Angular resampling exceeds memory limit.");
    // Guard against interpolating high-order content above the input Nyquist limit.
    for(std::size_t i=1;i<edges.size();++i)require(edges[i]-edges[i-1]>=2.56*s.maximumOrder/s.pulsesPerRevolution,"Tachometer speed exceeds sampled order bandwidth.");
    const auto length=std::size_t(s.samplesPerRevolution*s.revolutionsPerBlock);OrderResult result;
    result.pulses=int(edges.size());result.orderStep=1.0/s.revolutionsPerBlock;result.minimumRpm=std::numeric_limits<double>::infinity();
    const auto bins=std::size_t(std::floor(s.maximumOrder/result.orderStep))+1;result.rms.assign(bins,0);
    for(std::size_t first=0;first+length<=total;first+=length) {
        std::vector<double> block(length);double t0=0,t1=0;
        for(std::size_t k=0;k<=length;++k){double phase=double(first+k)/pointsPerPulse;auto edge=std::min(edges.size()-2,std::size_t(phase));
            const double interval=edges[edge+1]-edges[edge];double position=edges[edge]+(phase-double(edge))*interval;
            if(k==0)t0=position;if(k==length)t1=position;else block[k]=at(signal,position,interval/pointsPerPulse);}
        SpectralAverage spectrum(length,"hann",0,true);spectrum.add(block,block,s.samplesPerRevolution);auto data=spectrum.spectrum();
        data.rmsBins.resize(bins);for(std::size_t k=0;k<bins;++k)result.rms[k]+=data.rmsBins[k]*data.rmsBins[k];result.map.push_back(data.rmsBins);
        double rpm=60*rate*s.revolutionsPerBlock/(t1-t0);result.rpm.push_back(rpm);result.seconds.push_back((t0+t1)/(2*rate));
        result.minimumRpm=std::min(result.minimumRpm,rpm);result.maximumRpm=std::max(result.maximumRpm,rpm);++result.blocks;
    }
    require(result.blocks>0,"No complete angular analysis block.");
    for(auto &v:result.rms)v=std::sqrt(v/result.blocks);return result;
}
double modalAssurance(const std::vector<Complex> &a,const std::vector<Complex> &b) {
    require(!a.empty()&&a.size()==b.size(),"Shape vectors must have the same non-zero length.");Complex cross{};double aa=0,bb=0;
    for(std::size_t i=0;i<a.size();++i){cross+=std::conj(a[i])*b[i];aa+=std::norm(a[i]);bb+=std::norm(b[i]);}
    require(aa>0&&bb>0,"Shape vectors must have non-zero energy.");return std::clamp(std::norm(cross)/(aa*bb),0.0,1.0);
}
ModalFit fitReceptance(const std::vector<double> &frequencies,const std::vector<Complex> &response) {
    require(frequencies.size()>=8&&frequencies.size()==response.size()&&frequencies.size()<=32769,"Select at least eight valid FRF bins for fitting.");
    for(std::size_t i=0;i<frequencies.size();++i)require(std::isfinite(frequencies[i])&&frequencies[i]>0&&(i==0||frequencies[i]>frequencies[i-1])&&std::isfinite(std::abs(response[i])),"Invalid FRF data.");
    const double energy=std::accumulate(response.begin(),response.end(),0.0,[](double v,Complex x){return v+std::norm(x);});require(energy>1e-30,"FRF has no usable energy.");
    ModalFit best;double error=std::numeric_limits<double>::infinity(),lo=frequencies.front(),hi=frequencies.back(),zlo=.0005,zhi=.3;
    for(int pass=0;pass<5;++pass) {
        const double df=(hi-lo)/40, dz=(std::log(zhi)-std::log(zlo))/32;
        for(int fi=0;fi<=40;++fi)for(int zi=0;zi<=32;++zi){double f=lo+fi*df,z=std::exp(std::log(zlo)+zi*dz),wn=2*pi*f;
            Complex numerator{};double denominator=0;
            for(std::size_t k=0;k<frequencies.size();++k){double w=2*pi*frequencies[k];Complex h=1.0/Complex(wn*wn-w*w,2*z*wn*w);numerator+=std::conj(h)*response[k];denominator+=std::norm(h);}
            Complex residue=numerator/denominator;double residual=0;
            for(std::size_t k=0;k<frequencies.size();++k){double w=2*pi*frequencies[k];residual+=std::norm(response[k]-residue/Complex(wn*wn-w*w,2*z*wn*w));}
            if(residual<error){error=residual;best.frequency=f;best.damping=z;best.residue=residue;}}
        lo=std::max(frequencies.front(),best.frequency-df);hi=std::min(frequencies.back(),best.frequency+df);
        zlo=std::max(.00001,best.damping*std::exp(-dz));zhi=std::min(.5,best.damping*std::exp(dz));
    }
    double wn=2*pi*best.frequency;for(double f:frequencies){double w=2*pi*f;best.fitted.push_back(best.residue/Complex(wn*wn-w*w,2*best.damping*wn*w));}
    best.relativeError=std::sqrt(error/energy);best.assurance=modalAssurance(response,best.fitted);return best;
}
LimitResult checkLimits(const std::vector<double> &values,double start,double step,const std::vector<LimitPoint> &curve) {
    require(std::isfinite(start)&&std::isfinite(step)&&step>0&&curve.size()>=2,"Limit curves require a valid grid and at least two points.");
    for(std::size_t i=0;i<curve.size();++i)require(std::isfinite(curve[i].x)&&std::isfinite(curve[i].lower)&&std::isfinite(curve[i].upper)&&curve[i].lower<=curve[i].upper&&(i==0||curve[i].x>curve[i-1].x),"Limit points must be ordered, finite and lower <= upper.");
    LimitResult r;r.worstMargin=std::numeric_limits<double>::infinity();std::size_t c=0;
    for(std::size_t i=0;i<values.size();++i){double x=start+double(i)*step;if(x<curve.front().x||x>curve.back().x)continue;
        while(c+2<curve.size()&&x>curve[c+1].x)++c;double fraction=(x-curve[c].x)/(curve[c+1].x-curve[c].x);
        double lower=curve[c].lower+fraction*(curve[c+1].lower-curve[c].lower),upper=curve[c].upper+fraction*(curve[c+1].upper-curve[c].upper);
        if(!std::isfinite(values[i])){++r.missing;continue;}++r.evaluated;double margin=std::min(values[i]-lower,upper-values[i]);r.worstMargin=std::min(r.worstMargin,margin);if(margin<0)++r.failed;
    }
    // Insufficient frequency coverage must never produce a passing verdict.
    if(values.empty()||start>curve.front().x||start+double(values.size()-1)*step<curve.back().x)++r.missing;
    r.pass=r.evaluated>0&&r.failed==0&&r.missing==0;return r;
}
}
