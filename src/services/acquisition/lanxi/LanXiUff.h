#pragma once
#include <array>
#include <complex>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace lanxi {
// Original implementation of ASCII dataset 58. No vendor binary SDK is used.
struct UffAxis {
    int type=0, length=0, force=0, temperature=0;
    std::string label="NONE", unit="NONE";
};
struct UffFunction {
    std::array<std::string,5> identification{{"NONE","NONE","NONE","NONE","NONE"}};
    int type=1, id=1, version=1, loadCase=0;
    std::string responseEntity="NONE", referenceEntity="NONE";
    int responseNode=0, responseDirection=0, referenceNode=0, referenceDirection=0;
    bool complex=false, even=true;
    std::uint64_t count=0;
    double start=0, increment=1, z=0;
    std::array<UffAxis,4> axes;
    std::vector<double> x;
    std::vector<std::complex<double>> y;
};
struct UffDocument {
    std::vector<UffFunction> functions;
    std::vector<int> skippedDatasets;
    bool nonSiUnitSystem=false;
    int unitsCode=0;
};
// Bounded parser; accepts all eight ASCII numeric layouts in dataset 58.
// Units/coordinates are preserved as labels, never implicitly converted to SI.
UffDocument readUff(std::istream &input, std::size_t maxValues=1048576,
                    const std::function<bool()> &cancelled={});
// Callback output supports disk streaming without constructing the whole file.
class UffWriter {
public:
    using Sink=std::function<void(const std::string&)>;
    UffWriter(UffFunction header, Sink sink);
    void add(double x, std::complex<double> value);
    void finish();
private:
    UffFunction m_header;
    Sink m_sink;
    std::uint64_t m_count=0;
    int m_columns=0;
    std::string m_row;
    bool m_finished=false;
};
void writeUff(std::ostream &output, const UffDocument &document);
}
