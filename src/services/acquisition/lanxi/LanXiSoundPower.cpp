#include "LanXiSoundPower.h"
#include <cmath>
#include <stdexcept>
namespace lanxi {
SoundPower surfaceSoundPower(const std::vector<PressurePosition>&p,double k2,double minimum){
    auto need=[](bool ok,const char*s){if(!ok)throw std::runtime_error(s);};
    need(!p.empty()&&p.size()<=256,"Sound power needs 1..256 surface positions.");need(std::isfinite(k2)&&k2>=0&&k2<=30&&std::isfinite(minimum)&&minimum>=6&&minimum<=30,"Invalid environmental correction or background-separation policy.");
    long double measured=0,background=0;SoundPower r;
    for(auto v:p){need(std::isfinite(v.area)&&v.area>0&&v.area<=1e8&&std::isfinite(v.sourceAndBackgroundDb)&&std::isfinite(v.backgroundDb)&&v.sourceAndBackgroundDb>=-100&&v.sourceAndBackgroundDb<=250&&v.backgroundDb>=-100&&v.backgroundDb<=250,"Invalid area or pressure level.");
        need(v.sourceAndBackgroundDb>v.backgroundDb,"Source-on level must exceed background at every position.");r.area+=v.area;measured+=v.area*std::pow(10.0,v.sourceAndBackgroundDb/10);background+=v.area*std::pow(10.0,v.backgroundDb/10);}
    r.measuredDb=10*std::log10(double(measured/r.area));r.backgroundDb=10*std::log10(double(background/r.area));r.separationDb=r.measuredDb-r.backgroundDb;
    need(r.separationDb+1e-10>=minimum,"Background separation is below the selected engineering policy; no sound-power result issued.");
    r.backgroundCorrection=-10*std::log10(1-double(background/measured));r.environmentCorrection=k2;r.surfaceDb=r.measuredDb-r.backgroundCorrection-k2;r.powerDb=r.surfaceDb+10*std::log10(r.area);return r;
}
}
