// ======================================================================
// \title  I2cScannerTester.cpp
// \brief  Unit test harness for the I2cScanner component.
// ======================================================================

#include "I2cScannerTester.hpp"

#include <cstring>

namespace project {

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference -- which is exactly what the gtest EXPECT_/ASSERT_
// macros do with their arguments -- is odr-used and therefore needs an
// out-of-line definition. Without these the tests fail to link.
constexpr FwSizeType I2cScannerTester::MAX_HISTORY_SIZE;
constexpr U32 I2cScannerTester::ADDRESS_COUNT;
constexpr U32 I2cScannerTester::BUS_COUNT;

namespace {
// The non-reserved 7-bit addresses: 0x08 through 0x77.
constexpr U32 PROBES_PER_BUS = 0x77 - 0x08 + 1;
}  // namespace

I2cScannerTester::I2cScannerTester()
    : I2cScannerGTestBase("I2cScannerTester", MAX_HISTORY_SIZE), component("I2cScanner") {
    this->initComponents();
    this->connectPorts();
}

I2cScannerTester::~I2cScannerTester() {
    // Free what initComponents() allocated. macOS does not detect leaks here;
    // the Linux CI runner does.
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Test cases
// ----------------------------------------------------------------------

void I2cScannerTester::testReportsPresentDevices() {
    this->m_present[0][0x29] = true;
    this->m_present[0][0x68] = true;
    // Both ends of the probed range, to catch an off-by-one.
    this->m_present[1][0x08] = true;
    this->m_present[1][0x36] = true;
    this->m_present[1][0x77] = true;

    this->scan();

    ASSERT_EVENTS_BusScanned_SIZE(2);
    ASSERT_EVENTS_BusScanned(0, 0, 2, "0x29 0x68");
    ASSERT_EVENTS_BusScanned(1, 1, 3, "0x08 0x36 0x77");
}

void I2cScannerTester::testProbesEveryAddressOnce() {
    this->scan();

    for (U32 bus = 0; bus < BUS_COUNT; bus++) {
        ASSERT_EQ(this->m_probeCount[bus], PROBES_PER_BUS) << "bus " << bus;
        for (U32 i = 0; i < PROBES_PER_BUS; i++) {
            EXPECT_EQ(this->m_probes[bus][i], I2cScanner::FIRST_ADDRESS + i) << "bus " << bus << " probe " << i;
        }
    }
    EXPECT_FALSE(this->m_wrongReadSize);
}

void I2cScannerTester::testReportsEmptyBus() {
    this->m_present[1][0x40] = true;

    this->scan();

    ASSERT_EVENTS_BusScanned_SIZE(2);
    ASSERT_EVENTS_BusScanned(0, 0, 0, "none");
    ASSERT_EVENTS_BusScanned(1, 1, 1, "0x40");
}

void I2cScannerTester::testReportsStuckBus() {
    for (U32 address = 0; address < ADDRESS_COUNT; address++) {
        this->m_present[0][address] = true;
    }

    this->scan();

    ASSERT_EVENTS_BusScanned_SIZE(2);
    const auto& entry = this->eventHistory_BusScanned->at(0);
    EXPECT_EQ(entry.bus, 0);
    EXPECT_EQ(entry.count, PROBES_PER_BUS);

    const char* list = entry.addresses.toChar();
    const FwSizeType length = static_cast<FwSizeType>(std::strlen(list));
    EXPECT_LT(length, I2cScanner::LIST_CAPACITY);
    EXPECT_EQ(std::strncmp(list, "0x08 0x09 0x0A", 14), 0) << list;
    ASSERT_GE(length, 4U);
    EXPECT_STREQ(&list[length - 4], " ...") << list;
}

// ----------------------------------------------------------------------
// Fake buses
// ----------------------------------------------------------------------

Drv::I2cStatus I2cScannerTester::from_bus0Read_handler(FwIndexType portNum, U32 address, Fw::Buffer& buffer) {
    return this->answer(0, address, buffer);
}

Drv::I2cStatus I2cScannerTester::from_bus1Read_handler(FwIndexType portNum, U32 address, Fw::Buffer& buffer) {
    return this->answer(1, address, buffer);
}

Drv::I2cStatus I2cScannerTester::answer(U8 bus, U32 address, const Fw::Buffer& buffer) {
    if (buffer.getSize() != 1U) {
        this->m_wrongReadSize = true;
    }
    if (this->m_probeCount[bus] < ADDRESS_COUNT) {
        this->m_probes[bus][this->m_probeCount[bus]] = address;
    }
    this->m_probeCount[bus]++;

    if (address < ADDRESS_COUNT && this->m_present[bus][address]) {
        return Drv::I2cStatus::I2C_OK;
    }
    // What the Zephyr driver reports when nothing acknowledges.
    return Drv::I2cStatus::I2C_READ_ERR;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void I2cScannerTester::scan() {
    this->sendCmd_SCAN(0, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, I2cScanner::OPCODE_SCAN, 0, Fw::CmdResponse::OK);
}

}  // namespace project
