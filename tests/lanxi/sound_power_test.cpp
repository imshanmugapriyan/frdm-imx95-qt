#include "LanXiSoundPower.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("Sound-power check failed");}
int main(){try{
    auto r=lanxi::surfaceSoundPower({{1,80,60},{1,80,60}},2);check(std::abs(r.powerDb-(80+10*std::log10(.99)+10*std::log10(2.)-2))<1e-10);check(std::abs(r.backgroundCorrection-.043648054)<1e-9);
    auto split=lanxi::surfaceSoundPower({{.5,80,60},{.5,80,60},{1,80,60}},2);check(std::abs(split.powerDb-r.powerDb)<1e-12);
    auto uneven=lanxi::surfaceSoundPower({{1,70,50},{3,80,60}},0);check(std::abs(uneven.measuredDb-10*std::log10((1e7+3e8)/4))<1e-10);
    bool failed=false;try{lanxi::surfaceSoundPower({{1,64,60}},0);}catch(const std::exception&){failed=true;}check(failed);failed=false;try{lanxi::surfaceSoundPower({{0,80,60}},0);}catch(const std::exception&){failed=true;}check(failed);
    std::cout<<"Area-weighted sound power, background subtraction, subdivision and invalid-condition checks passed.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
