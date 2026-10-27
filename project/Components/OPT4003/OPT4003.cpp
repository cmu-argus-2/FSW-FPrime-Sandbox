// ======================================================================
// \title  OPT4003.cpp
// \brief  OPT4003 ambient light sensor device manager implementation.
// ======================================================================

#include "project/Components/OPT4003/OPT4003.hpp"

#include "Os/RawTime.hpp"
#include "Os/Task.hpp"

namespace project {

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference anywhere (the gtest macros do this) needs an
// out-of-line definition, or the link fails.
constexpr U32 OPT4003::DEFAULT_DEVICE_ADDRESS;
constexpr U16 OPT4003::CONFIGURATION;
constexpr U32 OPT4003::CONVERSION_TIMEOUT_US;
constexpr U32 OPT4003::CONVERSION_POLL_US;

// ----------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------

OPT4003::OPT4003(const char* const compName)
    : OPT4003ComponentBase(compName), m_deviceAddress(DEFAULT_DEVICE_ADDRESS), m_booted(false) {}

OPT4003::~OPT4003() {}

void OPT4003::configure(U32 deviceAddress) {
    this->m_deviceAddress = deviceAddress;
}

// ----------------------------------------------------------------------
// Register decoding
// ----------------------------------------------------------------------

bool OPT4003::isExpectedDeviceId(U16 raw) {
    const U16 didl = static_cast<U16>((raw >> 12) & 0x3U);
    const U16 didh = static_cast<U16>(raw & 0x0FFFU);
    return (didl == 0U) && (didh == 0x221U);
}

F32 OPT4003::decodeLux(U16 msb, U16 lsb) {
    const U32 exponent = (msb >> 12) & 0xFU;
    const U32 mantissa = (static_cast<U32>(msb & 0x0FFFU) << 8) | ((lsb >> 8) & 0xFFU);

    // The datasheet only defines exponents 0-8, which keep the ADC code
    // within 28 bits. The field is 4 bits wide, though, and a corrupted read
    // can carry up to 15; a 64-bit shift keeps that from wrapping into a
    // small, plausible-looking value.
    const U64 adcCodes = static_cast<U64>(mantissa) << exponent;
    return static_cast<F32>(adcCodes) * 535e-6F;
}

project::LightSensorFlags OPT4003::decodeFlags(U16 raw) {
    return project::LightSensorFlags((raw & FLAG_OVERLOAD) != 0U, (raw & FLAG_HIGH) != 0U, (raw & FLAG_LOW) != 0U);
}

// ----------------------------------------------------------------------
// Bus helpers
// ----------------------------------------------------------------------

Drv::I2cStatus OPT4003::readRegister(Register reg, U16& value) {
    // One write-read transaction: the register index out, two bytes back.
    // The buffers are stack locals, which is safe because busWriteRead_out
    // is synchronous.
    U8 registerAddress = static_cast<U8>(reg);
    U8 response[2] = {0, 0};
    Fw::Buffer writeBuffer(&registerAddress, static_cast<Fw::Buffer::SizeType>(sizeof registerAddress));
    Fw::Buffer readBuffer(response, static_cast<Fw::Buffer::SizeType>(sizeof response));

    const Drv::I2cStatus status = this->busWriteRead_out(0, this->m_deviceAddress, writeBuffer, readBuffer);
    if (status == Drv::I2cStatus::I2C_OK) {
        // Most significant byte first.
        value = static_cast<U16>((static_cast<U16>(response[0]) << 8) | static_cast<U16>(response[1]));
    } else {
        this->log_WARNING_HI_I2cError(static_cast<U8>(reg), status);
    }
    return status;
}

Drv::I2cStatus OPT4003::writeRegister(Register reg, U16 value) {
    U8 request[3] = {static_cast<U8>(reg), static_cast<U8>(value >> 8), static_cast<U8>(value & 0xFFU)};
    Fw::Buffer writeBuffer(request, static_cast<Fw::Buffer::SizeType>(sizeof request));

    const Drv::I2cStatus status = this->busWrite_out(0, this->m_deviceAddress, writeBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(static_cast<U8>(reg), status);
    }
    return status;
}

// ----------------------------------------------------------------------
// Operations
// ----------------------------------------------------------------------

Fw::Success OPT4003::bootDevice() {
    this->m_booted = false;

    // Identify the part before writing to it, so a misaddressed or
    // unexpected device is never reconfigured.
    U16 deviceId = 0;
    if (this->readRegister(DEVICE_ID_ADDR, deviceId) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (!isExpectedDeviceId(deviceId)) {
        this->log_WARNING_HI_WrongDeviceId(deviceId);
        return Fw::Success::FAILURE;
    }

    // The whole register is written in one transaction; every field is set
    // explicitly, including the reserved bit 14, which must be 0.
    if (this->writeRegister(CONFIGURATION_ADDR, CONFIGURATION) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }

    this->m_booted = true;
    this->log_WARNING_HI_WrongDeviceId_ThrottleClear();
    return Fw::Success::SUCCESS;
}

Fw::Success OPT4003::sampleLux(F32& lux) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }

    // Wait for CONVERSION_READY_FLAG. In continuous mode at 600 us this is
    // normally set on the first poll; the timeout only matters if the part
    // has stalled, in which case the last completed result is read anyway.
    Os::RawTime start;
    (void)start.now();
    while (true) {
        U16 flags = 0;
        if (this->readRegister(FLAGS_ADDR, flags) != Drv::I2cStatus::I2C_OK) {
            return Fw::Success::FAILURE;
        }
        if ((flags & FLAG_CONVERSION_READY) != 0U) {
            break;
        }

        Os::RawTime now;
        (void)now.now();
        U32 elapsedUs = 0;
        if (now.getDiffUsec(start, elapsedUs) != Os::RawTime::Status::OP_OK || elapsedUs >= CONVERSION_TIMEOUT_US) {
            break;
        }
        (void)Os::Task::delay(Fw::TimeInterval(0, CONVERSION_POLL_US));
    }

    // The two halves of the result are separate transactions. At a 600 us
    // conversion time a new result can land between them, mixing the
    // exponent and upper bits of one conversion with the lower bits of the
    // next. The COUNTER and CRC fields in RESULT_LSB_CH0 would detect this
    // but are not checked; a torn read is off by at most the lower 8
    // mantissa bits unless the exponent changed between conversions.
    U16 msb = 0;
    U16 lsb = 0;
    if (this->readRegister(RESULT_MSB_CH0_ADDR, msb) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->readRegister(RESULT_LSB_CH0_ADDR, lsb) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }

    lux = decodeLux(msb, lsb);
    this->tlmWrite_Lux(lux);

    // Every transaction in this read succeeded, so the bus is healthy: re-arm
    // the error event so a later fault is reported again.
    this->log_WARNING_HI_I2cError_ThrottleClear();
    return Fw::Success::SUCCESS;
}

Fw::Success OPT4003::sampleFlags(project::LightSensorFlags& flags) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }

    U16 raw = 0;
    if (this->readRegister(FLAGS_ADDR, raw) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    flags = decodeFlags(raw);
    this->tlmWrite_Flags(raw);
    return Fw::Success::SUCCESS;
}

// ----------------------------------------------------------------------
// Handlers
// ----------------------------------------------------------------------

Fw::Success OPT4003::boot_handler(FwIndexType portNum) {
    return this->bootDevice();
}

Fw::Success OPT4003::read_handler(FwIndexType portNum, F32& lux) {
    return this->sampleLux(lux);
}

Fw::Success OPT4003::readFlags_handler(FwIndexType portNum, project::LightSensorFlags& flags) {
    return this->sampleFlags(flags);
}

void OPT4003::run_handler(FwIndexType portNum, U32 context) {
    // Bring-up path: with no task to call boot, retry it every tick until the
    // sensor answers. Failures are already reported through events.
    if (!this->m_booted && this->bootDevice() != Fw::Success::SUCCESS) {
        return;
    }
    F32 lux = 0.0F;
    (void)this->sampleLux(lux);
}

}  // namespace project
