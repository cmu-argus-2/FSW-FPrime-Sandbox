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

  @ OPT4003 light sensor on the -X face, i2c0 address 0x44. Passive: its
  @ I2C transactions run on whichever thread calls it.
  instance lightSensorXm: project.OPT4003 base id 0x10007000

}
