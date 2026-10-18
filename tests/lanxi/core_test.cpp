#include "LanXiCore.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

using Bytes=std::vector<std::uint8_t>;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
void near(double actual,double expected,double tolerance=1e-9){check(std::abs(actual-expected)<=tolerance,"Numerical result outside tolerance.");}
void integer(Bytes &b,std::uint64_t n,int width){for(int i=0;i<width;++i)b.push_back(std::uint8_t(n>>(8*i)));}
void real(Bytes &b,double value){std::uint64_t bits;std::memcpy(&bits,&value,8);integer(b,bits,8);}
Bytes message(unsigned type,const Bytes &payload,std::uint64_t ticks=0,int extension=0){
    Bytes out{'B','K'};integer(out,20+extension,2);integer(out,type,2);integer(out,0,2);integer(out,0,4);
    out.insert(out.end(),{16,0,0,0});integer(out,ticks,8);for(int i=0;i<extension;++i)out.push_back(0);
    integer(out,payload.size(),4);out.insert(out.end(),payload.begin(),payload.end());return out;
}
void descriptor(Bytes &payload,int id,int kind,Bytes data){integer(payload,id,2);integer(payload,kind,2);integer(payload,0,2);integer(payload,data.size(),2);payload.insert(payload.end(),data.begin(),data.end());while(payload.size()%4)payload.push_back(0);}
Bytes interpretation(int id=1){
    Bytes b,v;integer(v,3,2);descriptor(b,id,1,v);v.clear();real(v,10);descriptor(b,id,2,v);
    v.clear();real(v,0.25);descriptor(b,id,3,v);v={16,0,0,0};integer(v,1,8);descriptor(b,id,4,v);
    v={1,0,'V'};descriptor(b,id,5,v);return message(8,b);
}
Bytes samples(){Bytes p;integer(p,1,2);integer(p,0,2);integer(p,1,2);integer(p,4,2);integer(p,0,3);integer(p,0x7fffff,3);integer(p,0x800000,3);integer(p,0xffffff,3);return message(1,p,9007199254741000ULL);}
template<class F>void throws(F f){bool raised=false;try{f();}catch(const std::exception &){raised=true;}check(raised,"Expected rejection.");}
int main(){
    try {
        int tests=0;const auto test=[&](const char *name,const auto &f){f();++tests;std::cout<<"PASS "<<name<<'\n';};
        test("fragmented stream preserves signed 24-bit extremes, offset and scaling",[]{
            auto bytes=interpretation();const auto raw=samples();bytes.insert(bytes.end(),raw.begin(),raw.end());lanxi::Decoder decoder;std::vector<lanxi::Event> events;
            for(auto b:bytes)decoder.feed(reinterpret_cast<const char *>(&b),1,[&](const auto &e){events.push_back(e);});decoder.finish();
            const auto &data=events.back();check(data.time.count==9007199254741000ULL,"Timestamp precision lost.");check(data.samples.size()==4,"Sample count mismatch.");
            near(data.samples[0],0.25);near(data.samples[1],10*8388607.0/8388608+0.25);near(data.samples[2],-9.75);near(data.samples[3],0.25-10.0/8388608);
        });
        test("all-signal interpretations, extended headers and quality flags",[]{
            lanxi::Decoder decoder;std::vector<lanxi::Event> events;const auto sink=[&](const auto &e){events.push_back(e);};
            auto b=interpretation(0);decoder.feed(reinterpret_cast<const char *>(b.data()),b.size(),sink);
            b=samples();decoder.feed(reinterpret_cast<const char *>(b.data()),b.size(),sink);check(events.back().descriptor.ready(),"Global descriptor not inherited.");
            Bytes quality;integer(quality,1,2);integer(quality,1,2);integer(quality,26,2);integer(quality,4,2);b=message(2,quality,0,4);
            decoder.feed(reinterpret_cast<const char *>(b.data()),b.size(),sink);check(events.back().quality==26,"Quality flags lost.");decoder.finish();
        });
        test("incomplete, untyped, malformed and oversized wire messages are rejected",[]{
            const auto sink=[](const auto &){};auto b=samples();lanxi::Decoder a;
            throws([&]{a.feed(reinterpret_cast<const char *>(b.data()),b.size(),sink);});
            lanxi::Decoder c;b=interpretation();c.feed(reinterpret_cast<const char *>(b.data()),b.size()-1,sink);throws([&]{c.finish();});
            lanxi::Decoder d;Bytes bad{'B','X',20,0};throws([&]{d.feed(reinterpret_cast<const char *>(bad.data()),bad.size(),sink);});
            lanxi::Decoder e;bad=message(1,{0});bad[24]=0xff;bad[25]=0xff;bad[26]=0xff;bad[27]=0x7f;
            throws([&]{e.feed(reinterpret_cast<const char *>(bad.data()),bad.size(),sink);});
            lanxi::Decoder f;bad=message(99,{0});throws([&]{f.feed(reinterpret_cast<const char *>(bad.data()),bad.size(),sink);});
        });
        test("64-bit time differences retain one-tick resolution",[]{
            lanxi::Time a{{16,0,0,0},(1ULL<<60)},b=a;++b.count;near(double(b.since(a)),1.0/65536);near(double(a.since(b)),-1.0/65536);
            b.family[1]=1;throws([&]{b.since(a);});
        });
        test("RMS, SPL and zero-level handling",[]{near(lanxi::rms({3,-3,3,-3}),3);near(lanxi::spl(1),93.97940008672037);check(std::isinf(lanxi::spl(0)),"Silence is not finite SPL.");throws([]{lanxi::rms({NAN});});});
        test("Hann spectrum amplitude, PSD and transfer gain/phase",[]{
            constexpr double pi=3.14159265358979323846;std::vector<double> x(1024),y(1024);
            for(int i=0;i<1024;++i){x[i]=std::cos(2*pi*64*i/1024);y[i]=2*std::cos(2*pi*64*i/1024+0.3);}
            lanxi::SpectralAverage average(1024);average.add(x,y,16384);check(!average.response(1e-6).valid[64],"A single FFT must not claim coherence.");
            average.add(x,y,16384);const auto response=average.response(1e-6);near(response.magnitude[64],2,1e-8);near(response.phase[64],0.3*180/pi,1e-8);near(response.coherence[64],1);
            const auto spectrum=average.spectrum();near(spectrum.rmsBins[64],std::sqrt(2.0),1e-8);near(spectrum.peakHz,1024);double power=0;for(double p:spectrum.psd)power+=p*16;near(power,2,1e-8);
            check(!response.valid[300],"An unexcited bin must be invalid.");average.reset();check(average.spectrum().rmsBins.empty(),"Reset left old data.");
        });
        test("uncorrelated signals do not produce high coherence",[]{
            std::mt19937 random(73);std::normal_distribution<double> noise;lanxi::SpectralAverage average(256);
            for(int block=0;block<100;++block){std::vector<double>x(256),y(256);for(int i=0;i<256;++i){x[i]=noise(random);y[i]=noise(random);}average.add(x,y,8192);}
            const auto response=average.response(1e-6);double sum=0;for(std::size_t i=1;i<response.coherence.size()-1;++i)sum+=response.coherence[i];check(sum/(response.coherence.size()-2)<0.04,"Coherence normalization error.");
        });
        test("silence cannot yield a valid frequency response",[]{lanxi::SpectralAverage a(256);std::vector<double>zero(256);a.add(zero,zero,8192);a.add(zero,zero,8192);for(bool valid:a.response(1e-6).valid)check(!valid,"Silent reference was accepted.");});
        test("non-bin-centred tone fit separates DC and recovers phase",[]{
            constexpr double pi=3.14159265358979323846;std::vector<double> values(4096);
            for(std::size_t i=0;i<values.size();++i)values[i]=0.7*std::cos(2*pi*1003*i/65536+0.4)+0.8;
            const auto tone=lanxi::fitTone(values,65536,1003);near(tone.rms,0.7/std::sqrt(2.0),1e-10);near(tone.phase,0.4*180/pi,1e-8);near(tone.residualRms,0,1e-10);
            throws([&]{lanxi::fitTone(values,65536,1);});
        });
        test("logarithmic sweep includes exact endpoints and validates bounds",[]{const auto f=lanxi::logarithmicSteps(100,10000,3);near(f[0],100);near(f[1],1000);near(f[2],10000);throws([]{lanxi::logarithmicSteps(100,1,3);});});
        test("paired ownership remains held until both stops and output-off are confirmed",[]{
            lanxi::StopGate gate;check(gate.released(),"Untouched hardware needs no recovery.");gate.touched={true,true};gate.outputTouched=true;
            for(int flags=0;flags<8;++flags){gate.idle={bool(flags&1),bool(flags&2)};gate.outputOff=bool(flags&4);check(gate.released()==(flags==7),"Premature ownership release.");}
            gate={};gate.touched[0]=true;check(!gate.released(),"Partial-start failure dropped ownership.");gate.idle[0]=true;check(gate.released(),"Stopped peer did not release.");
        });
        std::cout<<tests<<" native LAN-XI checks passed; no Qt runtime used.\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
