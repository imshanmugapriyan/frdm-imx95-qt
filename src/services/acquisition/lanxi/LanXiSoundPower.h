#pragma once
#include <vector>
namespace lanxi {
struct PressurePosition {double area=0,sourceAndBackgroundDb=0,backgroundDb=0;};
struct SoundPower {double area=0,measuredDb=0,backgroundDb=0,backgroundCorrection=0,environmentCorrection=0,surfaceDb=0,powerDb=0,separationDb=0;};
// Surface-pressure engineering calculation. Test geometry, stationarity,
// calibration, uncertainty and standards qualification are external inputs.
SoundPower surfaceSoundPower(const std::vector<PressurePosition>&positions,double environmentCorrection,double minimumSeparation=6);
}
