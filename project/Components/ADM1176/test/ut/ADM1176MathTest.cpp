#include "project/Components/ADM1176/ADM1176Math.hpp"

#include <cassert>
#include <cstdint>

int main() {
    // Each high byte carries ADC bits 11:4; the third byte holds both low nibbles.
    const uint8_t packed[3] = {0xAB, 0xCD, 0xEF};
    const project::ADM1176Math::Sample sample = project::ADM1176Math::decode(packed);
    assert(sample.voltageCode == 0xABE);
    assert(sample.currentCode == 0xCDF);

    // Zero, full scale, and both divider ranges.
    assert(project::ADM1176Math::voltageMillivolts(0, true) == 0);
    assert(project::ADM1176Math::voltageMillivolts(4095, true) == 6648);
    assert(project::ADM1176Math::voltageMillivolts(4095, false) == 26343);

    // Current conversion scales by the configured resistor (10 mOhm example).
    assert(project::ADM1176Math::currentMilliamps(0, 10) == 0);
    assert(project::ADM1176Math::currentMilliamps(4095, 10) == 10581);
    assert(project::ADM1176Math::currentMilliamps(4095, 20) == 5290);
    assert(project::ADM1176Math::currentMilliamps(4095, 0) == 0);
    return 0;
}
