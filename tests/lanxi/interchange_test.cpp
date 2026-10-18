#include "LanXiUff.h"
#include "LanXiStreaming.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool ok,const char*name){if(!ok)throw std::runtime_error(name);}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Malformed input was accepted");}
std::string fixture(int type,bool even) {
    // Independently assembled records from UFF 58's published field layouts.
    std::ostringstream o;o<<"    -1\n    58\nKnown response\nRun 17\n18-OCT-26 12:00:00\nNONE\nNONE\n";
    o<<std::setw(5)<<4<<std::setw(10)<<17<<std::setw(5)<<1<<std::setw(10)<<0<<" "<<std::setw(10)<<"BEAM"<<std::setw(10)<<12<<std::setw(4)<<-3<<" "<<std::setw(10)<<"HAMMER"<<std::setw(10)<<1<<std::setw(4)<<2<<"\n";
    o<<std::setw(10)<<type<<std::setw(10)<<2<<std::setw(10)<<int(even)<<"  1.00000E+01  5.00000E+00  0.00000E+00\n";
    for(int a=0;a<4;++a)o<<std::setw(10)<<(a==0?18:0)<<std::setw(5)<<0<<std::setw(5)<<0<<std::setw(5)<<0<<" "<<std::setw(20)<<"Axis"<<" "<<std::setw(20)<<(a==0?"Hz":"NONE")<<"\n";
    const bool wide=type==4||type==6, complex=type>=5;
    std::vector<int> widths=wide?(even?std::vector<int>(4,20):complex?std::vector<int>{13,20,20}:std::vector<int>{13,20,13,20}):std::vector<int>(6,13);
    std::vector<double> v;for(int i=0;i<2;++i){if(!even)v.push_back(10+5*i);v.push_back(i?-.125:2.5);if(complex)v.push_back(i?.75:-1.25);}
    for(std::size_t i=0;i<v.size();++i){int w=widths[i%widths.size()];o<<std::uppercase<<std::scientific<<std::setw(w)<<std::setprecision(w==20?12:5)<<v[i];if((i+1)%widths.size()==0||i+1==v.size())o<<"\n";}
    o<<"    -1\n";return o.str();
}
}
int main(){try{
    for(int t:{2,4,5,6})for(bool even:{false,true}){std::istringstream in(fixture(t,even));auto d=lanxi::readUff(in);const auto&f=d.functions[0];check(f.x==std::vector<double>({10,15})&&f.y[0].real()==2.5&&f.y[1].real()==-.125,"ASCII layout decode");check(f.responseNode==12&&f.responseDirection==-3&&f.referenceEntity=="HAMMER"&&f.axes[0].unit=="Hz","Metadata decode");check(!f.complex||f.y[0].imag()==-1.25,"Complex component");std::ostringstream out;lanxi::writeUff(out,d);std::istringstream back(out.str());check(lanxi::readUff(back).functions[0].y==f.y,"UFF round trip");}
    auto dtext=fixture(6,true);for(char &c:dtext)if(c=='E')c='D';std::istringstream din(dtext);check(lanxi::readUff(din).functions[0].y.size()==2,"Fortran D exponent");
    rejects([]{std::istringstream in(fixture(6,true));lanxi::readUff(in,1);});
    rejects([]{std::istringstream in(fixture(6,true));lanxi::readUff(in,100,[]{return true;});});
    rejects([]{std::istringstream in("    -1\n    58b\n");lanxi::readUff(in);});
    rejects([]{auto s=fixture(4,true);s.resize(s.size()-35);std::istringstream in(s);lanxi::readUff(in);});
    rejects([]{std::string s=fixture(4,true);s.replace(s.find("  5.00000E+00"),13,"  0.00000E+00");std::istringstream in(s);lanxi::readUff(in);});
    std::istringstream withSkipped("    -1\n   151\nMetadata\n    -1\n"+fixture(2,true));check(lanxi::readUff(withSkipped).skippedDatasets==std::vector<int>{151},"Unsupported dataset reported");
    for(int units:{1,5}){std::ostringstream unitRecord;unitRecord<<"    -1\n   164\n"<<std::setw(10)<<units<<std::setw(20)<<"Units"<<std::setw(10)<<1<<'\n';for(int i=0;i<3;++i)unitRecord<<std::setw(25)<<std::scientific<<std::setprecision(16)<<1.;unitRecord<<'\n'<<std::setw(25)<<0.<<"\n    -1\n";std::istringstream in(unitRecord.str()+fixture(4,true));auto d=lanxi::readUff(in);check(d.unitsCode==units&&d.nonSiUnitSystem==(units!=1),"Global unit-system guard");}
    // More samples than the workbench history; statistics include the first half.
    lanxi::RunningStatistics stats;for(int i=0;i<1200000;++i)stats.add(i<600000?1:3);check(stats.count==1200000&&std::abs(double(stats.mean)-2)<1e-10&&std::abs(stats.rms()-std::sqrt(5.0))<1e-12,"Whole-record statistics");
    lanxi::StreamingProcessor longProcessor(48000,{{"removeMean",0},{"gain",2}},double(stats.mean));lanxi::RunningStatistics processed;lanxi::UffFunction header;header.count=stats.count;header.increment=1./48000;std::uint64_t bytes=0;
    lanxi::UffWriter longWriter(header,[&](const std::string&s){bytes+=s.size();});for(int i=0;i<1200000;++i){double v=longProcessor.process(i<600000?1:3);processed.add(v);longWriter.add(i/48000.,v);}longWriter.finish();check(processed.count==1200000&&std::abs(processed.rms()-2)<1e-10&&bytes>24000000,"Full-record output exceeds preview length with bounded buffers");
    const std::vector<lanxi::ProcessStage> stages{{"removeMean",0},{"gain",2},{"highPass",10},{"lowPass",2000}};
    lanxi::StreamingProcessor one(48000,stages,2),chunks(48000,stages,2);std::vector<double> input(40000);for(std::size_t i=0;i<input.size();++i)input[i]=2+std::sin(i*.3);
    std::vector<double> expected;for(double v:input)expected.push_back(one.process(v));std::size_t index=0;for(std::size_t first=0;first<input.size();first+=137)for(std::size_t k=first;k<std::min(first+137,input.size());++k)check(chunks.process(input[k])==expected[index++],"Filter state crosses chunk boundaries");
    lanxi::StreamingProcessor integration(48000,{{"integrate",10}},0);double y=0;for(int i=0;i<48000;++i)y=integration.process(1);check(std::abs(y-1/(2*3.14159265358979323846*10))<1e-12,"Leaky integration DC response");
    lanxi::StreamingProcessor derivative(100,{{"differentiate",0}},0);check(derivative.process(3)==0&&std::abs(derivative.process(3.01)-1)<1e-12,"Causal derivative");
    rejects([]{lanxi::StreamingProcessor p(48000,{{"gain",1},{"removeMean",0}},0);});
    rejects([]{lanxi::StreamingProcessor p(48000,{{"envelope",0}},0);});
    rejects([]{lanxi::RunningStatistics statistics;statistics.add(INFINITY);});
    std::cout<<"UFF eight-layout, metadata, limits, malformed input and full-record streaming checks passed.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
