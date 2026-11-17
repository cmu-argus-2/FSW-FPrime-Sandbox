// ======================================================================
// \title  DRV8235Tester.hpp
// \brief  Unit test harness for the DRV8235 component
// ======================================================================

#ifndef project_DRV8235Tester_HPP
#define project_DRV8235Tester_HPP

#include <vector>

#include "project/Drivers/DRV8235/DRV8235.hpp"
#include "project/Drivers/DRV8235/DRV8235GTestBase.hpp"

namespace project {

class DRV8235Tester final : public DRV8235GTestBase {
  public:
    // Large enough to hold every bus call of a first read (11 reads, 10 writes)
    static const FwSizeType MAX_HISTORY_SIZE = 32;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    DRV8235Tester();
    ~DRV8235Tester();

    // ----------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------

    void testConfigureTouchesNoBus();
    void testConfigureRejectsBadAddress();
    void testFirstReadRunsInitSequence();
    void testLaterReadsSkipInit();
    void testReverseBridgeNegatesSample();
    void testReadBeforeConfigureTouchesNoBus();
    void testInitFailureRetriesOnNextRead();
    void testBusFailureAfterInitReinitializes();
    void testPollPublishesTelemetryAndFault();
    void testPollWithoutFaultEmitsNoFaultEvent();

  private:
    // ----------------------------------------------------------------
    // I2C mocks: a 32-register chip that records every write
    // ----------------------------------------------------------------

    Drv::I2cStatus from_busWriteRead_handler(FwIndexType portNum,
                                             U32 addr,
                                             Fw::Buffer& writeBuffer,
                                             Fw::Buffer& readBuffer) override;
    Drv::I2cStatus from_busWrite_handler(FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) override;

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Configure and run one read so the chip is initialized
    void configureAndInitialize();

  private:
    struct Write {
        U8 reg;
        U8 value;
    };

    DRV8235 component;
    Drv::I2cStatus m_busStatus;   //!< What both mocked bus ports return
    U8 m_regs[32];                //!< The simulated chip's registers
    std::vector<Write> m_writes;  //!< Every successful write, in order
};

}  // namespace project

#endif
