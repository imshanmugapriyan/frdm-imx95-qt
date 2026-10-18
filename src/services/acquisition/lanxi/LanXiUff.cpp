#include "LanXiUff.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace lanxi {
namespace {
void need(bool ok,const char *s){if(!ok)throw std::runtime_error(s);}
std::string trim(const std::string &s){auto a=s.find_first_not_of(" \r\t");return a==s.npos?"":s.substr(a,s.find_last_not_of(" \r\t")-a+1);}
std::string field(const std::string &s,std::size_t a,std::size_t n){need(a<s.size(),"Truncated UFF header.");return trim(s.substr(a,n));}
double real(std::string s){std::replace(s.begin(),s.end(),'D','E');std::replace(s.begin(),s.end(),'d','e');std::istringstream in(s);in.imbue(std::locale::classic());double v=0;in>>v;need(bool(in)&&std::isfinite(v),"Invalid or non-finite UFF number.");in>>std::ws;need(in.eof(),"Invalid UFF numeric field.");return v;}
long long integer(const std::string &s){auto t=trim(s);need(!t.empty(),"Missing UFF integer.");std::size_t end=0;auto n=std::stoll(t,&end);need(end==t.size(),"Invalid UFF integer.");return n;}
int intField(const std::string &s,std::size_t a,std::size_t n){auto v=integer(field(s,a,n));need(v>=-2147483647LL&&v<=2147483647LL,"UFF integer out of range.");return int(v);}
std::string textField(std::string s,int width){if(s.empty())s="NONE";need(s.size()<=std::size_t(width),"UFF text exceeds field width.");for(char c:s)need(c>=32&&c<=126,"UFF export text must be printable ASCII.");return s+std::string(std::size_t(width)-s.size(),' ');}
std::string iField(long long n,int width){auto s=std::to_string(n);need(s.size()<=std::size_t(width),"UFF integer exceeds field width.");return std::string(std::size_t(width)-s.size(),' ')+s;}
std::string eField(double n,int width,int precision){need(std::isfinite(n),"Cannot export a non-finite UFF value.");std::ostringstream o;o.imbue(std::locale::classic());o<<std::uppercase<<std::scientific<<std::setprecision(precision)<<std::setw(width)<<n;auto s=o.str();need(s.size()<=std::size_t(width),"UFF number exceeds field width.");return s;}
std::string axisLine(const UffAxis &a){return iField(a.type,10)+iField(a.length,5)+iField(a.force,5)+iField(a.temperature,5)+" "+textField(a.label,20)+" "+textField(a.unit,20)+"\n";}
class Reader {
public:
    std::istream &in;const std::function<bool()> &cancelled;std::uint64_t bytes=0;
    bool next(std::string &s){s.clear();char c;while(in.get(c)){need(++bytes<=256ULL*1024*1024,"UFF import exceeds 256 MiB.");if(c=='\n')break;need(c=='\r'||c=='\t'||(c>=32&&c<=126),"Binary/non-ASCII UFF is not supported.");s+=c;need(s.size()<=4096,"UFF line exceeds 4096 bytes.");}if(cancelled)need(!cancelled(),"UFF import cancelled.");if(!s.empty()&&s.back()=='\r')s.pop_back();need(!in.bad(),"UFF read failed.");return !s.empty()||bool(in);}
    std::string line(){std::string s;need(next(s),"Unexpected end of UFF file.");return s;}
};
}
UffDocument readUff(std::istream &input,std::size_t maxValues,const std::function<bool()> &cancelled){
    Reader reader{input,cancelled};UffDocument doc;std::string line;std::size_t used=0,datasets=0;
    while(reader.next(line)) {
        if(trim(line).empty())continue;
        need(trim(line)=="-1","Expected UFF dataset delimiter.");
        do {line=reader.line();}while(trim(line)=="-1"||trim(line).empty());
        need(line.find_first_of("bB")==line.npos,"UFF 58b binary files require a separate converter.");
        auto code=integer(line);need(code>0&&code<=2147483647&&++datasets<=4096,"Invalid or excessive UFF datasets.");
        if(code==164) {
            line=reader.line();const int units=intField(line,0,10);line=reader.line();
            const double length=real(field(line,0,25)),force=real(field(line,25,25)),temperature=real(field(line,50,25));
            const double offset=real(reader.line());need(trim(reader.line())=="-1","Invalid UFF unit-system dataset.");
            doc.unitsCode=units;doc.nonSiUnitSystem=doc.nonSiUnitSystem||units!=1||std::abs(length-1)>1e-12||std::abs(force-1)>1e-12||std::abs(temperature-1)>1e-12||std::abs(offset)>1e-12;
            continue;
        }
        if(code!=58) {
            doc.skippedDatasets.push_back(int(code));
            do {line=reader.line();}while(trim(line)!="-1");continue;
        }
        need(doc.functions.size()<256,"UFF import exceeds 256 functions.");UffFunction f;
        for(auto &id:f.identification)id=trim(reader.line());
        line=reader.line();f.type=intField(line,0,5);f.id=intField(line,5,10);f.version=intField(line,15,5);f.loadCase=intField(line,20,10);
        f.responseEntity=field(line,31,10);f.responseNode=intField(line,41,10);f.responseDirection=intField(line,51,4);
        f.referenceEntity=field(line,56,10);f.referenceNode=intField(line,66,10);f.referenceDirection=intField(line,76,4);
        need(std::abs(f.responseDirection)<=6&&std::abs(f.referenceDirection)<=6,"Invalid UFF direction.");
        line=reader.line();const int type=intField(line,0,10);auto count=integer(field(line,10,10));const int spacing=intField(line,20,10);
        need(type==2||type==4||type==5||type==6,"Unsupported UFF ordinate type.");need(spacing==0||spacing==1,"Invalid UFF abscissa spacing.");
        need(count>0&&std::uint64_t(count)<=maxValues-used,"UFF import sample limit exceeded.");used+=std::size_t(count);f.count=std::uint64_t(count);f.complex=type>=5;f.even=spacing==1;
        f.start=real(field(line,30,13));f.increment=real(field(line,43,13));f.z=real(field(line,56,13));
        need(!f.even||f.increment>0,"UFF uniform increment must be positive.");
        for(auto &a:f.axes){line=reader.line();a.type=intField(line,0,10);a.length=intField(line,10,5);a.force=intField(line,15,5);a.temperature=intField(line,20,5);a.label=field(line,26,20);a.unit=field(line,47,20);}
        const bool wide=type==4||type==6;std::vector<int> widths;
        if(!wide)widths.assign(6,13);else if(f.even)widths.assign(4,20);else widths=f.complex?std::vector<int>{13,20,20}:std::vector<int>{13,20,13,20};
        const std::size_t stride=(f.even?0:1)+(f.complex?2:1);std::vector<double> pending;
        while(f.y.size()<f.count) {
            line=reader.line();need(trim(line)!="-1","UFF function data ended before declared count.");std::size_t offset=0;
            for(int w:widths){if(offset>=line.size()||trim(line.substr(offset)).empty())break;
                pending.push_back(real(field(line,offset,std::size_t(w))));offset+=std::size_t(w);
                if(pending.size()==stride){need(f.y.size()<f.count,"UFF function has more values than declared.");std::size_t k=0;double x=f.even?f.start+double(f.y.size())*f.increment:pending[k++];double re=pending[k++],im=f.complex?pending[k]:0;
                    need(std::isfinite(x)&&(f.x.empty()||x>f.x.back()),"UFF abscissas must be finite and increasing.");f.x.push_back(x);f.y.emplace_back(re,im);pending.clear();}
            }
            need(offset>=line.size()||trim(line.substr(offset)).empty(),"Extra UFF numeric fields.");
        }
        need(pending.empty()&&trim(reader.line())=="-1","UFF function has excess data or no closing delimiter.");doc.functions.push_back(std::move(f));
    }
    need(!doc.functions.empty(),"No supported ASCII dataset 58 functions found.");return doc;
}
UffWriter::UffWriter(UffFunction header,Sink sink):m_header(std::move(header)),m_sink(std::move(sink)){
    const auto &f=m_header;need(f.count>0&&f.count<=9999999999ULL,"UFF count must fit its ten-character field.");need(f.increment>0||!f.even,"Invalid UFF increment.");need(std::abs(f.responseDirection)<=6&&std::abs(f.referenceDirection)<=6,"Invalid UFF direction.");
    std::string out="    -1\n    58\n";for(auto &id:f.identification)out+=textField(id,80)+"\n";
    out+=iField(f.type,5)+iField(f.id,10)+iField(f.version,5)+iField(f.loadCase,10)+" "+textField(f.responseEntity,10)+iField(f.responseNode,10)+iField(f.responseDirection,4)+" "+textField(f.referenceEntity,10)+iField(f.referenceNode,10)+iField(f.referenceDirection,4)+"\n";
    out+=iField(f.complex?6:4,10)+iField(static_cast<long long>(f.count),10)+iField(f.even?1:0,10)+eField(f.start,13,5)+eField(f.increment,13,5)+eField(f.z,13,5)+"\n";
    for(auto &a:f.axes)out+=axisLine(a);m_sink(out);
}
void UffWriter::add(double x,std::complex<double> value){
    need(!m_finished&&m_count<m_header.count,"Too many samples written to UFF.");need(m_header.complex||value.imag()==0,"Complex value in a real UFF function.");
    if(!m_header.even){m_row+=eField(x,13,5)+eField(value.real(),20,12);if(m_header.complex)m_row+=eField(value.imag(),20,12);++m_columns;if(m_header.complex||m_columns==2){m_sink(m_row+"\n");m_row.clear();m_columns=0;}}
    else {m_row+=eField(value.real(),20,12);++m_columns;if(m_header.complex){m_row+=eField(value.imag(),20,12);++m_columns;}if(m_columns==4){m_sink(m_row+"\n");m_row.clear();m_columns=0;}}
    ++m_count;
}
void UffWriter::finish(){need(!m_finished&&m_count==m_header.count,"UFF sample count does not match header.");if(!m_row.empty())m_sink(m_row+"\n");m_sink("    -1\n");m_finished=true;}
void writeUff(std::ostream &out,const UffDocument &doc){for(auto f:doc.functions){need(f.x.size()==f.y.size()&&!f.y.empty(),"UFF arrays differ in size.");f.count=f.y.size();UffWriter writer(f,[&](const std::string&s){out<<s;need(bool(out),"UFF output failed.");});for(std::size_t k=0;k<f.y.size();++k)writer.add(f.x[k],f.y[k]);writer.finish();}}
}
