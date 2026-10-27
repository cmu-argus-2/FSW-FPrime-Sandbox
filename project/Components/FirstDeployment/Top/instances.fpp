module Components {

  # ----------------------------------------------------------------------
  # Base ID Convention
  # ----------------------------------------------------------------------
  #
  # All Base IDs follow the 8-digit hex format: 0xDSSCCxxx
  #
  # Where:
  #   D   = Deployment digit (1 for this deployment)
  #   SS  = Subtopology digits (00 for main topology, 01-05 for subtopologies)
  #   CC  = Component digits (00, 01, 02, etc.)
  #   xxx = Reserved for internal component items (events, commands, telemetry)
  #

  # ----------------------------------------------------------------------
  # Defaults
  # ----------------------------------------------------------------------

  module Default {
    constant QUEUE_SIZE = 10
    @ Must match CONFIG_DYNAMIC_THREAD_STACK_SIZE in prj.conf.
    constant STACK_SIZE = 8192
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  # 1Hz rate group (divisor 1 of 1Hz base clock)
  instance rateGroup_1Hz: Svc.ActiveRateGroup base id 0x10001000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 43

  # 0.5Hz rate group (divisor 2 of 1Hz base clock)
  instance rateGroup_0_5Hz: Svc.ActiveRateGroup base id 0x10002000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 42

  # 0.25Hz rate group (divisor 4 of 1Hz base clock)
  instance rateGroup_0_25Hz: Svc.ActiveRateGroup base id 0x10003000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 41

  instance cmdSeq: Svc.CmdSequencer base id 0x10004000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 40

  instance hiCmpntInstance: project.HiComponent base id 0x10005000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 50

  # instance max17205Instance: project.MAX17205 base id 0x10006000 \
  #   queue size Default.QUEUE_SIZE \
  #   stack size Default.STACK_SIZE \
  #   priority 49

  # ----------------------------------------------------------------------
  # Queued component instances
  # ----------------------------------------------------------------------


  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance chronoTime: Zephyr.ZephyrTime base id 0x10010000

  instance rateGroupDriver: Svc.RateGroupDriver base id 0x10011000

  instance systemResources: Svc.SystemResources base id 0x10012000

  # instance timer: Svc.LinuxTimer base id 0x10013000
  # instance timer: Zephyr.ZephyrTime base id 0x10013000
  instance timer: Zephyr.ZephyrRateDriver base id 0x10013000

  # instance comDriver: Drv.TcpClient base id 0x10014000
  instance comDriver: Zephyr.ZephyrUartDriver base id 0x10014000

  @ I2C bus driver for i2c1 (SDA1/SCL1, GPIO46/47): the +X/+Y/+Z faces,
  @ RTC, fuel gauge and power monitors. Passive, with guarded ports, so
  @ devices sharing the bus are serialised by its mutex.
  instance i2cDriver: Zephyr.ZephyrI2cDriver base id 0x10015000

  @ I2C bus driver for i2c0 (SDA0/SCL0, GPIO24/25): the -X/-Y/-Z faces and
  @ the IMU.
  instance i2cDriver0: Zephyr.ZephyrI2cDriver base id 0x10016000

  # OPT4003 light sensors, one per face plus the four angled sensors of the
  # +Z sun sensor. Passive: their I2C transactions run on whichever thread
  # calls them. The bus each is wired to is set in topology.fpp and its
  # address in FirstDeploymentTopology.cpp; the pair identifies the sensor,
  # since the same address is reused on both buses.

  @ +X face light sensor, i2c1 0x44
  instance lightSensorXp: project.OPT4003 base id 0x10007000

  @ -X face light sensor, i2c0 0x44
  instance lightSensorXm: project.OPT4003 base id 0x10008000

  @ +Y face light sensor, i2c1 0x45
  instance lightSensorYp: project.OPT4003 base id 0x10009000

  @ -Y face light sensor, i2c0 0x45
  instance lightSensorYm: project.OPT4003 base id 0x1000A000

  @ +Z sun sensor, +X-facing element, i2c1 0x54
  instance lightSensorZpXp: project.OPT4003 base id 0x1000B000

  @ +Z sun sensor, -Y-facing element, i2c1 0x55
  instance lightSensorZpYm: project.OPT4003 base id 0x1000C000

  @ +Z sun sensor, -X-facing element, i2c1 0x56
  instance lightSensorZpXm: project.OPT4003 base id 0x1000D000

  @ +Z sun sensor, +Y-facing element, i2c1 0x57
  instance lightSensorZpYp: project.OPT4003 base id 0x1000E000

  @ -Z face light sensor, i2c0 0x46
  instance lightSensorZm: project.OPT4003 base id 0x1000F000

}
