module Board {

  constant BASE_ID = 0x10100000

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  # The I2C bus drivers are shared by more than one subsystem (EPS and ADCS
  # both have parts on i2c1), so neither subsystem owns them. Each deployment
  # connects a subsystem's drivers to these buses.
  #
  # The peripheral 3.3 V rail that powers every I2C device is a
  # regulator-fixed with regulator-boot-on in the board devicetree, so Zephyr
  # has already raised it and waited out its startup delay by the time these
  # are opened. A bus that fails to open is reported on the console rather
  # than asserted on, so one bad bus does not stop the deployment booting.

  @ I2C bus driver for i2c1 (SDA1/SCL1, GPIO46/47): the +X/+Y/+Z faces,
  @ RTC, fuel gauge and power monitors. Passive, with guarded ports, so
  @ devices sharing the bus are serialised by its mutex.
  instance i2c1: Zephyr.ZephyrI2cDriver base id BASE_ID + 0x01000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    if (Board::i2c1.open(DEVICE_DT_GET(DT_NODELABEL(i2c1))) != Drv::I2cStatus::I2C_OK) {
        // Nothing is up yet to carry an event, so say it on the console.
        Fw::Logger::log("[ERROR] Failed to open i2c1\\n");
    }
    """
  }

  @ I2C bus driver for i2c0 (SDA0/SCL0, GPIO24/25): the -X/-Y/-Z faces and
  @ the IMU.
  instance i2c0: Zephyr.ZephyrI2cDriver base id BASE_ID + 0x02000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    if (Board::i2c0.open(DEVICE_DT_GET(DT_NODELABEL(i2c0))) != Drv::I2cStatus::I2C_OK) {
        Fw::Logger::log("[ERROR] Failed to open i2c0\\n");
    }
    """
  }

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Hardware shared between subsystems. Connect a driver to a bus in the
  @ deployment, e.g.
  @   Eps.fuelGauge.busWriteRead -> Board.i2c1.writeRead
  topology Subtopology {
    instance i2c1
    instance i2c0
  }

}
