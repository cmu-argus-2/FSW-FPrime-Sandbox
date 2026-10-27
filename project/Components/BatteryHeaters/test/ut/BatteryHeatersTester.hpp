// ======================================================================
// \title  BatteryHeatersTester.hpp
// \brief  Unit test harness for the BatteryHeaters component
// ======================================================================

#ifndef project_BatteryHeatersTester_HPP
#define project_BatteryHeatersTester_HPP

#include "project/Components/BatteryHeaters/BatteryHeaters.hpp"
#include "project/Components/BatteryHeaters/BatteryHeatersGTestBase.hpp"

namespace project {

class BatteryHeatersTester final : public BatteryHeatersGTestBase {
  public:
    static const FwSizeType MAX_HISTORY_SIZE = 10;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! \param heater1Present  false = leave heater1Write/heater1Read
    //!                        unconnected (single-heater board)
    explicit BatteryHeatersTester(bool heater1Present = true);
    ~BatteryHeatersTester();

    // ----------------------------------------------------------------
    // Tests: dual-heater board
    // ----------------------------------------------------------------

    void testEnableHeater0DrivesMasterAndHeater0();
    void testEnableHeater0TwiceWritesMasterOnce();
    void testDisableHeater0DropsMasterWhenHeater1Off();
    void testEnableHeater1DrivesMasterAndHeater1();
    void testMasterStaysHighUntilBothHeatersOff();
    void testDisableHeater1DropsMasterWhenHeater0Off();

    // ----------------------------------------------------------------
    // Tests: single-heater board (heater 1 unconnected)
    // ----------------------------------------------------------------

    void testEnableHeater1RejectedWhenNotPresent();
    void testDisableHeater1RejectedWhenNotPresent();
    void testDisableHeater0DropsMasterWhenHeater1NotPresent();

  private:
    // ----------------------------------------------------------------
    // GPIO mocks: record the call (base class) and return m_gpioStatus
    // ----------------------------------------------------------------

    Drv::GpioStatus from_enableWrite_handler(FwIndexType portNum, const Fw::Logic& state) override;
    Drv::GpioStatus from_heater0Write_handler(FwIndexType portNum, const Fw::Logic& state) override;
    Drv::GpioStatus from_heater1Write_handler(FwIndexType portNum, const Fw::Logic& state) override;

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Send a command and dispatch it (passive component: runs inline)
    void sendAndCheck(FwOpcodeType opCode, Fw::CmdResponse expected);

  private:
    BatteryHeaters component;
    bool m_heater1Present;
    Drv::GpioStatus m_gpioStatus;  //!< What every mocked GPIO write returns
    U32 m_cmdSeq;
};

}  // namespace project

#endif