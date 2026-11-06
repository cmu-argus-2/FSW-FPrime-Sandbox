// ======================================================================
// \title  ADM1176Tester.hpp
// \brief  Unit test harness for the ADM1176 component
// ======================================================================

#ifndef project_ADM1176Tester_HPP
#define project_ADM1176Tester_HPP

#include "project/Drivers/ADM1176/ADM1176.hpp"
#include "project/Drivers/ADM1176/ADM1176GTestBase.hpp"

namespace project {

class ADM1176Tester final : public ADM1176GTestBase {
  public:
    static const FwSizeType MAX_HISTORY_SIZE = 10;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    ADM1176Tester();
    ~ADM1176Tester();

    // ----------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------

    void testConfigureSendsContinuousHighRangeCommand();
    void testReadReturnsDecodedSample();
    void testReadBeforeConfigureTouchesNoBus();
    void testReadBusFailureLeavesSampleUnchanged();
    void testPollPublishesTelemetry();

  private:
    // ----------------------------------------------------------------
    // I2C mocks: record the call (base class), then act like the bus
    // ----------------------------------------------------------------

    Drv::I2cStatus from_i2cWrite_handler(FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) override;
    Drv::I2cStatus from_i2cRead_handler(FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) override;

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Configure the component the way the EPS spec requires
    bool configureForEps();

  private:
    ADM1176 component;
    Drv::I2cStatus m_busStatus;  //!< What both mocked bus ports return
    U8 m_readBytes[3];           //!< What the mocked read hands back
    U8 m_lastCommand;            //!< First byte of the last write
};

}  // namespace project

#endif