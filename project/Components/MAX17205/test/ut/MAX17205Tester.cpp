// ======================================================================
// \title  MAX17205Tester.cpp
// \brief  Unit test harness for the MAX17205 component.
// ======================================================================

#include "MAX17205Tester.hpp"

namespace project {

namespace {

// The two I2C addresses the part answers on.
constexpr U32 MAIN_ADDR = 0x36;
constexpr U32 SHADOW_ADDR = 0x0B;

// Register addresses, repeated here rather than reached into the component,
// so that a typo in the component's private register map is caught by the
// tests instead of being mirrored by them.
constexpr U8 REG_STATUS = 0x00;
constexpr U8 REG_REPCAP = 0x05;
constexpr U8 REG_REPSOC = 0x06;
constexpr U8 REG_TEMP = 0x08;
constexpr U8 REG_VCELL = 0x09;
constexpr U8 REG_CURRENT = 0x0A;
constexpr U8 REG_CAPACITY = 0x10;
constexpr U8 REG_TTE = 0x11;
constexpr U8 REG_CYCLES = 0x17;
constexpr U8 REG_TTF = 0x20;
constexpr U8 REG_TEMP1 = 0x34;
constexpr U8 REG_INTTEMP = 0x35;
constexpr U8 REG_TEMP2 = 0x3B;
constexpr U8 REG_CONFIG2 = 0xBB;
constexpr U8 REG_TIMERH = 0xBE;
constexpr U8 REG_VBAT = 0xDA;

// Raw register values the fake gauge reports. Each was chosen so that its
// decoded value is exactly representable as an F32, which lets the telemetry
// assertions below compare for exact equality rather than a tolerance.
constexpr U16 RAW_STATUS = 0x0002;   // POR bit set; reported raw as a bitfield
constexpr U16 RAW_VBAT = 6400;       // -> 8000.0 mV
constexpr U16 RAW_CURRENT = 0xFEC0;  // -320 as I16 -> -50.0 mA (discharging)
constexpr U16 RAW_VCELL = 51200;     // -> 4000.0 mV
constexpr U16 RAW_REPSOC = 19968;    // -> 78.0 %
constexpr U16 RAW_REPCAP = 3000;     // -> 1500.0 mAh
constexpr U16 RAW_CAPACITY = 4000;   // -> 2000.0 mAh
constexpr U16 RAW_CYCLES = 123;      // reported raw
constexpr U16 RAW_TTE = 1600;        // -> 9000.0 s
constexpr U16 RAW_TTF = 800;         // -> 4500.0 s
constexpr U16 RAW_TIMERH = 77;       // reported raw
constexpr U16 RAW_TEMP = 5632;       // -> 2200.0 centi-C
constexpr U16 RAW_TEMP1 = 3000;      // -> 2690 centi-C
constexpr U16 RAW_TEMP2 = 2731;      // -> 0 centi-C (exactly 273.1 K)
constexpr U16 RAW_INTTEMP = 3050;    // -> 3190 centi-C

}  // namespace

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference -- which is exactly what the gtest EXPECT_/ASSERT_
// macros do with their arguments -- is odr-used and therefore needs an
// out-of-line definition. Without these the tests fail to link.
constexpr FwSizeType MAX17205Tester::MAX_HISTORY_SIZE;
constexpr U32 MAX17205Tester::READS_PER_SAMPLE;
constexpr U32 MAX17205Tester::MAX_TRANSACTIONS;
constexpr U32 MAX17205Tester::MAX_WRITE_PAYLOAD;

MAX17205Tester::MAX17205Tester() : MAX17205GTestBase("MAX17205Tester", MAX_HISTORY_SIZE), component("MAX17205") {
    this->initComponents();
    this->connectPorts();
}

MAX17205Tester::~MAX17205Tester() {}

// ----------------------------------------------------------------------
// Fake bus
// ----------------------------------------------------------------------

U16 MAX17205Tester::valueForRegister(U32 address, U8 reg) const {
    if (this->m_overrideActive && address == this->m_overrideAddress && reg == this->m_overrideRegister) {
        return this->m_overrideValue;
    }

    if (address == MAIN_ADDR) {
        switch (reg) {
            case REG_STATUS:
                return RAW_STATUS;
            case REG_VBAT:
                return RAW_VBAT;
            case REG_CURRENT:
                return RAW_CURRENT;
            case REG_VCELL:
                return RAW_VCELL;
            case REG_REPSOC:
                return RAW_REPSOC;
            case REG_REPCAP:
                return RAW_REPCAP;
            case REG_CAPACITY:
                return RAW_CAPACITY;
            case REG_CYCLES:
                return RAW_CYCLES;
            case REG_TTE:
                return RAW_TTE;
            case REG_TTF:
                return RAW_TTF;
            case REG_TIMERH:
                return RAW_TIMERH;
            case REG_TEMP:
                return RAW_TEMP;
            default:
                return 0;
        }
    }

    // Shadow RAM.
    switch (reg) {
        case REG_TEMP1:
            return RAW_TEMP1;
        case REG_TEMP2:
            return RAW_TEMP2;
        case REG_INTTEMP:
            return RAW_INTTEMP;
        default:
            return 0;
    }
}

Drv::I2cStatus MAX17205Tester::from_busWriteRead_handler(FwIndexType portNum,
                                                         U32 address,
                                                         Fw::Buffer& writeBuffer,
                                                         Fw::Buffer& readBuffer) {
    // The component must always ask for exactly one register byte out and two
    // data bytes back. Check that before anything else, because a malformed
    // transaction would make the rest of the test meaningless.
    // These are EXPECT rather than ASSERT because gtest's ASSERT_* expands to
    // a bare `return`, which will not compile in a handler that must return a
    // status. The early return below covers the case where continuing would
    // dereference a null buffer.
    EXPECT_NE(writeBuffer.getData(), nullptr);
    EXPECT_NE(readBuffer.getData(), nullptr);
    EXPECT_EQ(writeBuffer.getSize(), 1U);
    EXPECT_EQ(readBuffer.getSize(), 2U);
    if (writeBuffer.getData() == nullptr || readBuffer.getData() == nullptr || writeBuffer.getSize() < 1 ||
        readBuffer.getSize() < 2) {
        return Drv::I2cStatus::I2C_OTHER_ERR;
    }

    const U8 reg = writeBuffer.getData()[0];

    // Record the transaction so tests can assert on the address/register
    // sequence, including the transactions that are about to fail.
    EXPECT_LT(this->m_transactionCount, MAX_TRANSACTIONS);
    if (this->m_transactionCount < MAX_TRANSACTIONS) {
        this->m_transactions[this->m_transactionCount].address = address;
        this->m_transactions[this->m_transactionCount].reg = reg;
        ++this->m_transactionCount;
    }

    this->pushFromPortEntry_busWriteRead(address, writeBuffer, readBuffer);

    // Injected failure: leave the read buffer alone, as a real driver would
    // on a failed transaction.
    if (this->m_failEverything || (address == this->m_failAddress && reg == this->m_failRegister)) {
        return this->m_forcedStatus;
    }

    // Answer little-endian, low byte first, as the part does.
    const U16 value = valueForRegister(address, reg);
    readBuffer.getData()[0] = static_cast<U8>(value & 0xFFU);
    readBuffer.getData()[1] = static_cast<U8>(value >> 8U);
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus MAX17205Tester::from_busWrite_handler(FwIndexType portNum, U32 address, Fw::Buffer& buffer) {
    ++this->m_writeCount;
    this->pushFromPortEntry_busWrite(address, buffer);

    // Copy the payload out now, while the component's buffer is still in
    // scope. See m_lastWrite in the header.
    this->m_lastWriteSize = static_cast<U32>(buffer.getSize());
    EXPECT_LE(this->m_lastWriteSize, MAX_WRITE_PAYLOAD);
    if (buffer.getData() != nullptr && this->m_lastWriteSize <= MAX_WRITE_PAYLOAD) {
        for (U32 i = 0; i < this->m_lastWriteSize; ++i) {
            this->m_lastWrite[i] = buffer.getData()[i];
        }
    }

    if (this->m_failEverything) {
        return this->m_forcedStatus;
    }
    return Drv::I2cStatus::I2C_OK;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void MAX17205Tester::assertTransaction(U32 index, U32 address, U8 reg) const {
    ASSERT_LT(index, this->m_transactionCount) << "no transaction recorded at index " << index;
    EXPECT_EQ(this->m_transactions[index].address, address) << "wrong I2C address at transaction " << index;
    EXPECT_EQ(this->m_transactions[index].reg, reg) << "wrong register at transaction " << index;
}

void MAX17205Tester::failEverything(Drv::I2cStatus status) {
    this->m_failEverything = true;
    this->m_forcedStatus = status;
}

void MAX17205Tester::failOnly(U32 address, U8 reg, Drv::I2cStatus status) {
    this->m_failEverything = false;
    this->m_failAddress = address;
    this->m_failRegister = reg;
    this->m_forcedStatus = status;
}

void MAX17205Tester::overrideRegister(U32 address, U8 reg, U16 value) {
    this->m_overrideActive = true;
    this->m_overrideAddress = address;
    this->m_overrideRegister = reg;
    this->m_overrideValue = value;
}

void MAX17205Tester::clearOverride() {
    this->m_overrideActive = false;
}

void MAX17205Tester::failNothing() {
    this->m_failEverything = false;
    // 0x00 is not a register this driver ever reads, and pairing it with an
    // address the part does not use makes the match impossible.
    this->m_failAddress = 0xFFFFFFFF;
    this->m_failRegister = 0x00;
    this->m_forcedStatus = Drv::I2cStatus::I2C_OK;
}

void MAX17205Tester::resetRecording() {
    this->clearHistory();
    this->m_transactionCount = 0;
    this->m_writeCount = 0;
    this->m_lastWriteSize = 0;
}

void MAX17205Tester::assertCleanSampleTelemetry() {
    // Every channel is written exactly once per sample, with the scaling
    // applied. The raw values were picked so these are exact.
    // Status is a bitfield, so it is asserted as the exact register contents
    // rather than a scaled quantity.
    ASSERT_TLM_Status_SIZE(1);
    ASSERT_TLM_Status(0, 0x0002);
    ASSERT_TLM_Voltage_SIZE(1);
    ASSERT_TLM_Voltage(0, 8000.0F);
    ASSERT_TLM_Current_SIZE(1);
    ASSERT_TLM_Current(0, -50.0F);
    ASSERT_TLM_MidVoltage_SIZE(1);
    ASSERT_TLM_MidVoltage(0, 4000.0F);
    ASSERT_TLM_StateOfCharge_SIZE(1);
    ASSERT_TLM_StateOfCharge(0, 78.0F);
    ASSERT_TLM_RemainingCapacity_SIZE(1);
    ASSERT_TLM_RemainingCapacity(0, 1500.0F);
    ASSERT_TLM_FullCapacity_SIZE(1);
    ASSERT_TLM_FullCapacity(0, 2000.0F);
    ASSERT_TLM_Cycles_SIZE(1);
    ASSERT_TLM_Cycles(0, 123);
    ASSERT_TLM_TimeToEmpty_SIZE(1);
    ASSERT_TLM_TimeToEmpty(0, 9000.0F);
    ASSERT_TLM_TimeToFull_SIZE(1);
    ASSERT_TLM_TimeToFull(0, 4500.0F);
    ASSERT_TLM_TimeSincePowerUp_SIZE(1);
    ASSERT_TLM_TimeSincePowerUp(0, 77);
    ASSERT_TLM_Temperature_SIZE(1);
    ASSERT_TLM_Temperature(0, 2200.0F);
    ASSERT_TLM_TemperatureAin1_SIZE(1);
    ASSERT_TLM_TemperatureAin1(0, 2690);
    ASSERT_TLM_TemperatureAin2_SIZE(1);
    ASSERT_TLM_TemperatureAin2(0, 0);
    ASSERT_TLM_TemperatureDie_SIZE(1);
    ASSERT_TLM_TemperatureDie(0, 3190);
}

// ----------------------------------------------------------------------
// Test cases
// ----------------------------------------------------------------------

void MAX17205Tester::testReadAll() {
    this->failNothing();
    this->resetRecording();

    this->sendCmd_READ_ALL(0, 42);
    this->component.doDispatch();

    // Every register is read once, and the three shadow-RAM temperatures go
    // to 0x0B while everything else goes to 0x36. Getting an address wrong is
    // the kind of bug that produces plausible-looking garbage on orbit, so the
    // sequence is pinned exactly.
    ASSERT_EQ(this->m_transactionCount, READS_PER_SAMPLE);
    this->assertTransaction(0, MAIN_ADDR, REG_STATUS);
    this->assertTransaction(1, MAIN_ADDR, REG_VBAT);
    this->assertTransaction(2, MAIN_ADDR, REG_CURRENT);
    this->assertTransaction(3, MAIN_ADDR, REG_VCELL);
    this->assertTransaction(4, MAIN_ADDR, REG_REPSOC);
    this->assertTransaction(5, MAIN_ADDR, REG_REPCAP);
    this->assertTransaction(6, MAIN_ADDR, REG_CAPACITY);
    this->assertTransaction(7, MAIN_ADDR, REG_CYCLES);
    this->assertTransaction(8, MAIN_ADDR, REG_TTE);
    this->assertTransaction(9, MAIN_ADDR, REG_TTF);
    this->assertTransaction(10, MAIN_ADDR, REG_TIMERH);
    this->assertTransaction(11, MAIN_ADDR, REG_TEMP);
    this->assertTransaction(12, SHADOW_ADDR, REG_TEMP1);
    this->assertTransaction(13, SHADOW_ADDR, REG_TEMP2);
    this->assertTransaction(14, SHADOW_ADDR, REG_INTTEMP);

    this->assertCleanSampleTelemetry();

    // No bus errors, so no events at all.
    ASSERT_EVENTS_SIZE(0);

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_READ_ALL, 42, Fw::CmdResponse::OK);
}

void MAX17205Tester::testReadAllPartialFailure() {
    // Start from a good sample so the telemetry database holds a known value
    // for the channel that is about to break.
    this->failNothing();
    this->resetRecording();
    this->sendCmd_READ_ALL(0, 1);
    this->component.doDispatch();
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_READ_ALL, 1, Fw::CmdResponse::OK);

    // Now break exactly one register.
    this->failOnly(MAIN_ADDR, REG_VBAT, Drv::I2cStatus::I2C_READ_ERR);
    this->resetRecording();

    this->sendCmd_READ_ALL(0, 43);
    this->component.doDispatch();

    // The failure is on the very first read. All 14 are still attempted: one
    // bad register must not cost the whole battery snapshot.
    ASSERT_EQ(this->m_transactionCount, READS_PER_SAMPLE);

    // The broken channel publishes nothing at all this sample. That is what
    // keeps the last good value on the ground intact: a failed read must never
    // push a zero or a fault value onto a channel that was previously valid.
    ASSERT_TLM_Voltage_SIZE(0);

    // Everything else is published as normal.
    ASSERT_TLM_Current_SIZE(1);
    ASSERT_TLM_Current(0, -50.0F);
    ASSERT_TLM_StateOfCharge_SIZE(1);
    ASSERT_TLM_StateOfCharge(0, 78.0F);
    ASSERT_TLM_TemperatureDie_SIZE(1);
    ASSERT_TLM_TemperatureDie(0, 3190);

    // Exactly one error event, naming the register that actually failed.
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_VBAT, Drv::I2cStatus::I2C_READ_ERR);

    // The command still fails, so an operator knows the snapshot is partial.
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_READ_ALL, 43, Fw::CmdResponse::EXECUTION_ERROR);
}

void MAX17205Tester::testScheduledSample() {
    this->failNothing();
    this->resetRecording();

    // Rate group tick. The port is async, so the work happens on dispatch.
    this->invoke_to_run(0, 0);
    this->component.doDispatch();

    ASSERT_EQ(this->m_transactionCount, READS_PER_SAMPLE);
    this->assertCleanSampleTelemetry();
    ASSERT_EVENTS_SIZE(0);

    // Scheduled sampling is not a command, so nothing should be responding to
    // the command dispatcher.
    ASSERT_CMD_RESPONSE_SIZE(0);
}

void MAX17205Tester::testReset() {
    this->failNothing();
    this->resetRecording();

    this->sendCmd_RESET(0, 44);
    this->component.doDispatch();

    // Reset is a write-only transaction, so it must not touch the write-read
    // port at all.
    ASSERT_EQ(this->m_transactionCount, 0U);
    ASSERT_EQ(this->m_writeCount, 1U);

    // CONFIG2 = 0x0001, low byte first, on the main address.
    ASSERT_from_busWrite_SIZE(1);
    EXPECT_EQ(this->fromPortHistory_busWrite->at(0).addr, MAIN_ADDR);
    ASSERT_EQ(this->m_lastWriteSize, 3U);
    EXPECT_EQ(this->m_lastWrite[0], REG_CONFIG2);
    EXPECT_EQ(this->m_lastWrite[1], 0x01);
    EXPECT_EQ(this->m_lastWrite[2], 0x00);

    ASSERT_EVENTS_SIZE(0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_RESET, 44, Fw::CmdResponse::OK);
}

void MAX17205Tester::testResetFailure() {
    this->failEverything(Drv::I2cStatus::I2C_WRITE_ERR);
    this->resetRecording();

    this->sendCmd_RESET(0, 45);
    this->component.doDispatch();

    ASSERT_EQ(this->m_writeCount, 1U);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_CONFIG2, Drv::I2cStatus::I2C_WRITE_ERR);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_RESET, 45, Fw::CmdResponse::EXECUTION_ERROR);
}

void MAX17205Tester::testConfigureAddresses() {
    // A board variant could strap the part elsewhere; configure() must move
    // both banks, not just the main one.
    constexpr U32 ALT_MAIN = 0x40;
    constexpr U32 ALT_SHADOW = 0x41;
    this->component.configure(ALT_MAIN, ALT_SHADOW);

    this->failNothing();
    this->resetRecording();

    this->sendCmd_READ_ALL(0, 46);
    this->component.doDispatch();

    ASSERT_EQ(this->m_transactionCount, READS_PER_SAMPLE);
    // First read goes to the reconfigured main bank...
    this->assertTransaction(0, ALT_MAIN, REG_STATUS);
    // ...and the thermistor reads to the reconfigured shadow bank.
    this->assertTransaction(12, ALT_SHADOW, REG_TEMP1);
    this->assertTransaction(13, ALT_SHADOW, REG_TEMP2);
    this->assertTransaction(14, ALT_SHADOW, REG_INTTEMP);

    // Put it back so a later sample in the same fixture is unaffected.
    this->component.configure(MAX17205::DEFAULT_DEVICE_ADDRESS, MAX17205::DEFAULT_SHADOW_ADDRESS);
}

void MAX17205Tester::testErrorThrottle() {
    // With the bus fully down, one sample produces 14 failures but the event
    // is throttled to 5 so a dead bus cannot swamp the downlink.
    constexpr U32 THROTTLE_LIMIT = 5;

    this->failEverything(Drv::I2cStatus::I2C_ADDRESS_ERR);
    this->resetRecording();
    this->invoke_to_run(0, 0);
    this->component.doDispatch();

    ASSERT_EQ(this->m_transactionCount, READS_PER_SAMPLE);
    ASSERT_EVENTS_I2cError_SIZE(THROTTLE_LIMIT);

    // Still down: the throttle is latched, so no further events.
    this->resetRecording();
    this->invoke_to_run(0, 0);
    this->component.doDispatch();
    ASSERT_EVENTS_I2cError_SIZE(0);

    // Bus recovers. A sample with zero failures clears the throttle.
    this->failNothing();
    this->resetRecording();
    this->invoke_to_run(0, 0);
    this->component.doDispatch();
    ASSERT_EVENTS_SIZE(0);
    this->assertCleanSampleTelemetry();

    // Bus drops again: reporting must have been re-armed by the clean sample.
    this->failEverything(Drv::I2cStatus::I2C_ADDRESS_ERR);
    this->resetRecording();
    this->invoke_to_run(0, 0);
    this->component.doDispatch();
    ASSERT_EVENTS_I2cError_SIZE(THROTTLE_LIMIT);
}

void MAX17205Tester::testSubZeroTemperature() {
    // 0xFF00 is -256 counts, i.e. -1 C. Decoded as unsigned it would be
    // 25500 centi-Celsius -- a freezing pack reported as scalding. This
    // exercises the whole path (bus bytes -> reassembly -> decode -> channel)
    // rather than the decode function alone, so it fails if readTemperature
    // ever stops routing the register through toSigned.
    this->failNothing();
    this->overrideRegister(MAIN_ADDR, REG_TEMP, 0xFF00);
    this->resetRecording();

    this->invoke_to_run(0, 0);
    this->component.doDispatch();

    ASSERT_TLM_Temperature_SIZE(1);
    ASSERT_TLM_Temperature(0, -100.0F);

    // -20 C, a plausible eclipse reading.
    this->overrideRegister(MAIN_ADDR, REG_TEMP, 0xEC00);
    this->resetRecording();
    this->invoke_to_run(0, 0);
    this->component.doDispatch();

    ASSERT_TLM_Temperature_SIZE(1);
    ASSERT_TLM_Temperature(0, -2000.0F);

    // The thermistor channels are a separate, genuinely unsigned encoding and
    // must be unaffected by any of this.
    ASSERT_TLM_TemperatureAin1(0, 2690);

    this->clearOverride();
}

}  // namespace project
