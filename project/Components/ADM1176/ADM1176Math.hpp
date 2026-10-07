#ifndef project_ADM1176_MATH_HPP
#define project_ADM1176_MATH_HPP

#include <stdint.h>

namespace project {
namespace ADM1176Math {

struct Sample {
    uint16_t voltageCode;
    uint16_t currentCode;
};

inline Sample decode(const uint8_t bytes[3]) {
    return {static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 4) | (bytes[2] >> 4)),
            static_cast<uint16_t>((static_cast<uint16_t>(bytes[1]) << 4) | (bytes[2] & 0x0F))};
}

inline uint32_t voltageMillivolts(uint16_t code, bool lowVoltageRange) {
    const uint32_t fullScaleMv = lowVoltageRange ? 6650U : 26350U;
    return static_cast<uint32_t>((static_cast<uint64_t>(code) * fullScaleMv) / 4096U);
}

inline uint32_t currentMilliamps(uint16_t code, uint32_t senseResistanceMilliOhms) {
    if (senseResistanceMilliOhms == 0) {
        return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(code) * 105840U) /
                                  (4096ULL * senseResistanceMilliOhms));
}

}  // namespace ADM1176Math
}  // namespace project

#endif
