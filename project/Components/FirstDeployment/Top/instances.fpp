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

  @ I2C bus driver. Passive on purpose: its handlers are meant to run on
  @ whichever component's thread called them, and its ports are guarded, so
  @ two components sharing the bus are serialised by its mutex.
  instance i2cDriver: Zephyr.ZephyrI2cDriver base id 0x10015000

  @ MAX17205 fuel gauge. Passive: it owns no thread, so its I2C transactions
  @ run on whichever task calls it (the EPS task, once that exists). Every
  @ peripheral shares one I2C bus, so a thread per driver would buy no
  @ concurrency while costing a stack each; the threads belong at the task
  @ layer instead.
  instance max17205Instance: project.MAX17205 base id 0x10006000

}
