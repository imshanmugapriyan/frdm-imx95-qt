#include "LanXiCore.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace lanxi {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr std::size_t maxPayload = 1024 * 1024;
void require(bool yes, const char *message) { if (!yes) throw std::runtime_error(message); }
struct Reader {
    const std::uint8_t *p;
    std::size_t n, pos = 0;
    void need(std::size_t count) const { require(count <= n - pos, "Truncated Web-XI message."); }
    std::uint64_t integer(std::size_t count) {
        need(count); std::uint64_t value = 0;
        for (std::size_t i = 0; i < count; ++i) value |= std::uint64_t(p[pos++]) << (8 * i);
        return value;
    }
    double real() {
        const auto bits = integer(8); double value; std::memcpy(&value, &bits, sizeof value);
        require(std::isfinite(value), "Non-finite Web-XI scale/offset."); return value;
    }
    Time time() {
        Time t;
        for (auto &exponent : t.family) exponent = static_cast<std::uint8_t>(integer(1));
        t.count = integer(8); t.frequency(); return t;
    }
};
void fft(std::vector<std::complex<double>> &a) {
    const auto n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit; if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1) {
        const auto step = std::polar(1.0, -2 * pi / length);
        for (std::size_t i = 0; i < n; i += length) {
            std::complex<double> w(1, 0);
            for (std::size_t j = 0; j < length / 2; ++j) {
                const auto u = a[i+j], v = a[i+j+length/2] * w;
                a[i+j] = u+v; a[i+j+length/2] = u-v; w *= step;
            }
        }
    }
}
}
long double Time::frequency() const {
    const int primes[] = {2, 3, 5, 7}; long double result = 1;
    for (std::size_t i=0; i<family.size(); ++i) {
        require(family[i] <= 63, "Unsupported timestamp exponent.");
        result *= std::pow(static_cast<long double>(primes[i]), family[i]);
    }
    require(std::isfinite(result) && result <= 1e30L, "Unsupported timestamp family.");
    return result;
}
long double Time::seconds() const { return count / frequency(); }
long double Time::since(const Time &other) const {
    require(family == other.family, "Timestamp family changed during acquisition.");
    // Subtract integer clocks before conversion, preserving small differences
    // even when the epoch counter exceeds the exact range of a JS/double number.
    return (count >= other.count ? static_cast<long double>(count-other.count)
                                : -static_cast<long double>(other.count-count)) / frequency();
}
bool Descriptor::ready() const { return (mask & 31) == 31 && dataType == 3 && scale > 0 && period.count > 0; }
double Descriptor::sampleRate() const {
    require(ready(), "Incomplete or unsupported signal interpretation.");
    const double rate = static_cast<double>(1 / period.seconds());
    require(std::isfinite(rate) && rate >= 1 && rate <= 524288, "Unsupported sample rate.");
    return rate;
}
void Decoder::reset() { m_bytes.clear(); m_consumed=0; m_default={}; m_descriptors.clear(); }
void Decoder::finish() const { require(pending() == 0, "Stream ended inside a Web-XI message."); }
void Decoder::feed(const char *data, std::size_t size, const Sink &sink) {
    require(size <= maxPayload && pending()+size <= 2*maxPayload, "Web-XI receive buffer limit exceeded.");
    if (m_consumed) { m_bytes.erase(m_bytes.begin(), m_bytes.begin()+m_consumed); m_consumed=0; }
    m_bytes.insert(m_bytes.end(), data, data+size);
    while (pending() >= 4) {
        const auto *p = m_bytes.data()+m_consumed;
        require(p[0] == 'B' && p[1] == 'K', "Invalid Web-XI magic.");
        const unsigned headerLength = unsigned(p[2]) | (unsigned(p[3]) << 8);
        require(headerLength >= 20 && headerLength <= 256, "Unsupported Web-XI header length.");
        const std::size_t headerSize = headerLength+8; // prefix + header + payload-size field
        if (pending() < headerSize) return;
        Reader header{p, headerSize, 4};
        const auto type = static_cast<unsigned>(header.integer(2));
        header.integer(2); header.integer(4); const Time time = header.time();
        header.pos = headerSize-4; const auto sizeOfBody = static_cast<std::size_t>(header.integer(4));
        require(sizeOfBody > 0 && sizeOfBody <= maxPayload, "Invalid Web-XI payload length.");
        if (pending() < headerSize+sizeOfBody) return;
        message(p+headerSize, sizeOfBody, type, time, sink);
        m_consumed += headerSize+sizeOfBody;
    }
}
void Decoder::message(const std::uint8_t *data, std::size_t size, unsigned type,
                      const Time &time, const Sink &sink) {
    Reader r{data, size}; std::vector<Event> events;
    if (type == 8) {
        while (r.pos < r.n) {
            const int id = static_cast<int>(r.integer(2)); const auto kind = r.integer(2);
            r.integer(2); const auto length = static_cast<std::size_t>(r.integer(2)); r.need(length);
            require(id >= 0 && id <= 4, "Unexpected analog signal ID.");
            if (id && !m_descriptors.count(id)) m_descriptors[id] = m_default;
            auto &d = id ? m_descriptors[id] : m_default;
            Reader value{data+r.pos, length};
            const auto apply = [&](Descriptor &target) {
                value.pos=0;
                switch (kind) {
                case 1: require(length==2, "Invalid data type descriptor."); target.dataType=static_cast<std::uint16_t>(value.integer(2));
                    require(target.dataType==3, "Only signed 24-bit analog samples are supported."); target.mask|=1; break;
                case 2: require(length==8, "Invalid scale descriptor."); target.scale=value.real();
                    require(target.scale>0, "Signal scale must be positive."); target.mask|=2; break;
                case 3: require(length==8, "Invalid offset descriptor."); target.offset=value.real(); break;
                case 4: require(length==12, "Invalid sample period descriptor."); target.period=value.time();
                    require(target.period.count>0, "Zero sample period."); target.mask|=4; break;
                case 5: {
                    const auto bytes=static_cast<std::size_t>(value.integer(2));
                    require(bytes<=128 && bytes+2==length, "Invalid signal unit.");
                    target.unit.assign(reinterpret_cast<const char *>(value.p+2), bytes); target.mask|=8; break;
                }
                case 6: require(length==2 && value.integer(2)==0, "Vector data is not supported."); break;
                case 7: require(length==2 && value.integer(2)==1, "Only analog input channels are supported."); break;
                default: break; // Future descriptors are length-delimited.
                }
                // Offset is optional and defaults to zero in the protocol.
                target.mask |= 16;
            };
            apply(d);
            if (!id) for (auto &pair : m_descriptors) apply(pair.second);
            const auto padding = (4-(length%4))%4; r.need(length+padding); r.pos += length+padding;
            if (id) events.push_back({Event::Kind::Interpretation,time,id,d,{},0});
            else for (const auto &pair : m_descriptors) events.push_back({Event::Kind::Interpretation,time,pair.first,pair.second,{},0});
        }
    } else if (type == 1 || type == 2) {
        const auto count = r.integer(2); require(count>0 && count<=4, "Invalid analog signal count.");
        if (type == 1) r.integer(2);
        for (unsigned i=0; i<count; ++i) {
            const int id=static_cast<int>(r.integer(2)); require(id>0 && id<=4, "Unexpected analog signal ID.");
            if (!m_descriptors.count(id)) m_descriptors[id]=m_default;
            Event e{type==1 ? Event::Kind::Samples : Event::Kind::Quality,time,id,m_descriptors[id],{},0};
            if (type==1) {
                const auto n=r.integer(2); require(n>0 && n<=32767, "Invalid sample count.");
                require(e.descriptor.ready(), "Samples arrived without complete interpretation.");
                e.descriptor.sampleRate(); r.need(static_cast<std::size_t>(n)*3); e.samples.reserve(static_cast<std::size_t>(n));
                for (unsigned k=0; k<n; ++k) {
                    const auto bits=static_cast<std::uint32_t>(r.integer(3));
                    const auto raw=static_cast<std::int32_t>(bits)-(bits & 0x800000 ? 0x1000000 : 0);
                    const double corrected=raw/8388608.0*e.descriptor.scale+e.descriptor.offset;
                    require(std::isfinite(corrected), "Sample scaling overflow."); e.samples.push_back(corrected);
                }
            } else { e.quality=static_cast<std::uint16_t>(r.integer(2)); r.integer(2); }
            events.push_back(std::move(e));
        }
    } else throw std::runtime_error("Unsupported Web-XI message type; refusing to discard measurement data.");
    require(r.pos==r.n, "Unexpected bytes after Web-XI message.");
    for (const auto &event : events) sink(event);
}
double rms(const std::vector<double> &values) {
    require(!values.empty(), "RMS requires samples.");
    double scale=0, sum=0;
    for (double v:values) { require(std::isfinite(v), "Non-finite sample."); scale=std::max(scale,std::abs(v)); }
    if (!scale) return 0;
    for (double v:values) sum+=(v/scale)*(v/scale);
    return scale*std::sqrt(sum/values.size());
}
double spl(double pressureRms) {
    require(std::isfinite(pressureRms) && pressureRms>=0, "Invalid pressure.");
    return pressureRms>0 ? 20*std::log10(pressureRms/0.00002) : -std::numeric_limits<double>::infinity();
}
SpectralAverage::SpectralAverage(std::size_t size,const std::string &window,int exponentialAverages,bool removeMean)
    : m_size(size),m_exponentialAverages(exponentialAverages),m_removeMean(removeMean) {
    require(size>=64 && size<=65536 && !(size & (size-1)), "FFT size must be a power of two from 64 to 65536.");
    require(window=="hann"||window=="hamming"||window=="blackman"||window=="flat-top"||window=="rectangular","Unknown FFT window.");
    require(exponentialAverages==0||(exponentialAverages>=2&&exponentialAverages<=10000),"Invalid exponential average count.");
    m_window.resize(size);
    for (std::size_t i=0;i<size;++i) {
        const double t=2*pi*i/size;
        double w=1;
        if(window=="hann")w=0.5-0.5*std::cos(t);
        else if(window=="hamming")w=0.54-0.46*std::cos(t);
        else if(window=="blackman")w=0.42-0.5*std::cos(t)+0.08*std::cos(2*t);
        else if(window=="flat-top")w=0.21557895-0.41663158*std::cos(t)+0.277263158*std::cos(2*t)-0.083578947*std::cos(3*t)+0.006947368*std::cos(4*t);
        m_window[i]=w; m_windowSum+=w; m_windowSquareSum+=w*w;
    }
    reset();
}
void SpectralAverage::reset() {
    m_count=0; m_rate=0; m_rmsSquare=0; m_peak=0; m_weight=0;
    m_xx.assign(m_size/2+1,0); m_yy=m_xx; m_yx.assign(m_xx.size(),{});
}
void SpectralAverage::add(const std::vector<double> &reference,const std::vector<double> &response,double rate) {
    require(reference.size()==m_size && response.size()==m_size, "Incorrect FFT block size.");
    require(std::isfinite(rate) && rate>0 && (!m_count || rate==m_rate), "Sample rate changed in spectral average.");
    std::vector<std::complex<double>> x(m_size),y(m_size);
    const double responseRms=rms(response); rms(reference);
    double xm=0,ym=0;
    if(m_removeMean){for(std::size_t i=0;i<m_size;++i){xm+=reference[i];ym+=response[i];}xm/=m_size;ym/=m_size;}
    for (std::size_t i=0;i<m_size;++i) { x[i]=(reference[i]-xm)*m_window[i]; y[i]=(response[i]-ym)*m_window[i]; }
    fft(x); fft(y); m_rate=rate; ++m_count;
    const double decay=m_exponentialAverages?1.0-1.0/m_exponentialAverages:1.0;
    m_weight=m_weight*decay+1;m_rmsSquare=m_rmsSquare*decay+responseRms*responseRms;
    for (double v:response) m_peak=std::max(m_peak,std::abs(v));
    for (std::size_t k=0;k<m_xx.size();++k) { m_xx[k]=m_xx[k]*decay+std::norm(x[k]); m_yy[k]=m_yy[k]*decay+std::norm(y[k]); m_yx[k]=m_yx[k]*decay+y[k]*std::conj(x[k]); }
}
Spectrum SpectralAverage::spectrum() const {
    Spectrum result; if (!m_count) return result;
    result.sampleRate=m_rate; result.rms=std::sqrt(m_rmsSquare/m_weight); result.peak=m_peak;
    double maxPower=0;
    for (std::size_t k=0;k<m_yy.size();++k) {
        const double sided=k==0 || k==m_size/2 ? 1 : 2;
        const double power=m_yy[k]/m_weight;
        result.rmsBins.push_back(std::sqrt(sided*power)/m_windowSum);
        result.psd.push_back(sided*power/(m_rate*m_windowSquareSum));
        if (k && power>maxPower) { maxPower=power; result.peakHz=k*m_rate/m_size; }
    }
    return result;
}
Response SpectralAverage::response(double minimumReferenceRms) const {
    require(std::isfinite(minimumReferenceRms) && minimumReferenceRms>0, "Reference floor must be positive.");
    Response result;
    for (std::size_t k=0;k<m_xx.size();++k) {
        const double sided=k==0 || k==m_size/2 ? 1 : 2;
        const double referenceRms=m_count ? std::sqrt(sided*m_xx[k]/m_weight)/m_windowSum : 0;
        const bool valid=m_count>=2 && referenceRms>=minimumReferenceRms && m_yy[k]>0;
        const auto h=valid ? m_yx[k]/m_xx[k] : std::complex<double>{};
        result.valid.push_back(valid); result.magnitude.push_back(valid ? std::abs(h) : 0);
        result.phase.push_back(valid ? std::arg(h)*180/pi : 0);
        result.coherence.push_back(valid ? std::clamp(std::norm(m_yx[k])/(m_xx[k]*m_yy[k]),0.0,1.0) : 0);
    }
    return result;
}
Tone fitTone(const std::vector<double> &values,double rate,double frequency) {
    require(values.size()>=16 && std::isfinite(rate) && rate>0 && std::isfinite(frequency)
            && frequency>0 && frequency<rate/2 && values.size()*frequency/rate>=2, "Insufficient cycles for tone fit.");
    double a[3][4]{};
    for (std::size_t i=0;i<values.size();++i) {
        require(std::isfinite(values[i]), "Invalid tone sample.");
        const double basis[]={std::cos(2*pi*frequency*i/rate),std::sin(2*pi*frequency*i/rate),1};
        for (int row=0;row<3;++row) { for (int col=0;col<3;++col) a[row][col]+=basis[row]*basis[col]; a[row][3]+=basis[row]*values[i]; }
    }
    for (int col=0;col<3;++col) {
        int pivot=col; for (int row=col+1;row<3;++row) if (std::abs(a[row][col])>std::abs(a[pivot][col])) pivot=row;
        for (int k=0;k<4;++k) std::swap(a[pivot][k],a[col][k]);
        const double divisor=a[col][col]; require(std::abs(divisor)>1e-12,"Singular tone fit.");
        for (int k=col;k<4;++k) a[col][k]/=divisor;
        for (int row=0;row<3;++row) if (row!=col) { const double f=a[row][col]; for(int k=col;k<4;++k) a[row][k]-=f*a[col][k]; }
    }
    Tone result; result.phasor={a[0][3],-a[1][3]}; result.rms=std::abs(result.phasor)/std::sqrt(2.0); result.phase=std::arg(result.phasor)*180/pi;
    double residual=0;
    for (std::size_t i=0;i<values.size();++i) { const double e=values[i]-a[0][3]*std::cos(2*pi*frequency*i/rate)-a[1][3]*std::sin(2*pi*frequency*i/rate)-a[2][3]; residual+=e*e; }
    result.residualRms=std::sqrt(residual/values.size()); return result;
}
std::vector<double> logarithmicSteps(double first,double last,int count) {
    require(std::isfinite(first)&&std::isfinite(last)&&first>0&&last>first&&count>=2&&count<=256,"Invalid stepped-sine range.");
    std::vector<double> result;
    for (int i=0;i<count;++i) result.push_back(first*std::pow(last/first,double(i)/(count-1)));
    result.front()=first; result.back()=last; return result;
}
bool StopGate::released() const {
    return (!touched[0]||idle[0]) && (!touched[1]||idle[1]) && (!outputTouched||outputOff);
}
}
