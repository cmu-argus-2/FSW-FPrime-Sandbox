// ======================================================================
// \title  I2cScanner.cpp
// \brief  I2C bus scanner: a bench tool, not flight software.
// ======================================================================

#include "project/Tools/I2cScanner/I2cScanner.hpp"

#include <cstdio>
#include <cstring>

namespace project {

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference anywhere (the gtest macros do this) needs an
// out-of-line definition, or the link fails.
constexpr U8 I2cScanner::FIRST_ADDRESS;
constexpr U8 I2cScanner::LAST_ADDRESS;
constexpr FwSizeType I2cScanner::LIST_CAPACITY;

namespace {
// Appended when the list is cut short.
constexpr char TRUNCATED[] = " ...";
}  // namespace

I2cScanner::I2cScanner(const char* const compName) : I2cScannerComponentBase(compName) {}

I2cScanner::~I2cScanner() {}

bool I2cScanner::appendAddress(char* list, FwSizeType& used, U8 address) {
    // "0xNN", preceded by a space unless it is the first entry.
    const FwSizeType entry = (used == 0) ? 4 : 5;
    if (used + entry + (sizeof TRUNCATED - 1) >= LIST_CAPACITY) {
        return false;
    }
    (void)std::snprintf(&list[used], LIST_CAPACITY - used, (used == 0) ? "0x%02X" : " 0x%02X", address);
    used += entry;
    return true;
}

bool I2cScanner::probe(U8 bus, U8 address) {
    U8 byte = 0;
    Fw::Buffer buffer(&byte, 1);
    const Drv::I2cStatus status =
        (bus == 0) ? this->bus0Read_out(0, address, buffer) : this->bus1Read_out(0, address, buffer);
    return status == Drv::I2cStatus::I2C_OK;
}

void I2cScanner::scanBus(U8 bus) {
    char list[LIST_CAPACITY] = {};
    FwSizeType used = 0;
    U32 count = 0;
    bool truncated = false;

    for (U32 address = FIRST_ADDRESS; address <= LAST_ADDRESS; address++) {
        if (!this->probe(bus, static_cast<U8>(address))) {
            continue;
        }
        count++;
        if (!truncated && !appendAddress(list, used, static_cast<U8>(address))) {
            truncated = true;
        }
    }

    if (count == 0) {
        (void)std::snprintf(list, LIST_CAPACITY, "none");
    } else if (truncated) {
        (void)std::snprintf(&list[used], LIST_CAPACITY - used, "%s", TRUNCATED);
    }
    this->log_ACTIVITY_HI_BusScanned(bus, count, Fw::String(list));
}

void I2cScanner::SCAN_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // An unconnected output port asserts when invoked, so a bus that is not
    // wired up is skipped rather than probed.
    if (this->isConnected_bus0Read_OutputPort(0)) {
        this->scanBus(0);
    }
    if (this->isConnected_bus1Read_OutputPort(0)) {
        this->scanBus(1);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace project
