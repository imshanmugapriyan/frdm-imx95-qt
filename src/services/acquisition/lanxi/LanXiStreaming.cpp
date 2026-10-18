#include "LanXiStreaming.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lanxi {
namespace {void need(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}}
void RunningStatistics::add(double v){need(std::isfinite(v),"Non-finite recording sample.");++count;mean+=(v-mean)/count;squares+=static_cast<long double>(v)*v;need(std::isfinite(mean)&&std::isfinite(squares),"Recording statistics overflow; reduce processing gain.");peak=std::max(peak,std::abs(v));}
double RunningStatistics::rms() const{return count?std::sqrt(double(squares/count)):0;}
StreamingProcessor::StreamingProcessor(double rate,const std::vector<ProcessStage>&stages,double mean):m_rate(rate){
    need(std::isfinite(rate)&&rate>0&&std::isfinite(mean)&&stages.size()<=16,"Invalid streaming recipe.");
    for(std::size_t i=0;i<stages.size();++i){const auto &s=stages[i];need(std::isfinite(s.value),"Invalid stage value.");Stage stage;stage.type=s.type;stage.value=s.value;
        if(s.type=="removeMean"){need(i==0,"Full-record mean removal must be the first stage.");stage.value=mean;}
        else if(s.type=="highPass"||s.type=="lowPass")stage.filter=Biquad::pass(rate,s.value,s.type=="highPass");
        else if(s.type=="integrate"){need(s.value>0&&s.value<rate/2.56,"Leaky integration cutoff must be positive and below bandwidth.");const double w=2*3.14159265358979323846*s.value;stage.a=(2*rate-w)/(2*rate+w);stage.b=1/(2*rate+w);}
        else need(s.type=="gain"||s.type=="differentiate","This recipe contains a finite-window-only operation. Envelope is not available for full recordings.");
        m_stages.push_back(stage);
    }
}
double StreamingProcessor::process(double v){need(std::isfinite(v),"Non-finite recording sample.");for(auto &s:m_stages){const double x=v;
    if(s.type=="removeMean")v-=s.value;else if(s.type=="gain")v*=s.value;
    else if(s.type=="highPass"||s.type=="lowPass")v=s.filter.process(v);
    else if(s.type=="differentiate")v=s.first?0:(x-s.previous)*m_rate;
    else if(s.type=="integrate")v=s.a*s.output+s.b*(x+s.previous);
    s.previous=x;s.output=v;s.first=false;need(std::isfinite(v),"Processing overflow; reduce gain or revise recipe.");
}return v;}
}
