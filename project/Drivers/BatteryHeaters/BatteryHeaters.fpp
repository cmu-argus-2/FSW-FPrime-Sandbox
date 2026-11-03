module project {

  @ Enable/disable control for spacecraft battery heaters.
  @ Supports one or two heater channels sharing a common master enable line.
  @ Leave the heater1Write/heater1Read ports unconnected in the topology
  @ for single-heater board configurations.
  passive component BatteryHeaters {

    # ----------------------------------------------------------------------
    # Commands
    # ----------------------------------------------------------------------

    @ Enable heater 0
    sync command EnableHeater0 opcode 0

    @ Enable heater 1 (rejected if heater 1 is not connected on this board)
    sync command EnableHeater1 opcode 1

    @ Disable heater 0
    sync command DisableHeater0 opcode 2

    @ Disable heater 1 (rejected if heater 1 is not connected on this board)
    sync command DisableHeater1 opcode 3

    # ----------------------------------------------------------------------
    # GPIO ports
    # ----------------------------------------------------------------------

    @ Master heater enable line
    output port enableWrite: Drv.GpioWrite

    @ Heater 0 enable line
    output port heater0Write: Drv.GpioWrite

    @ Heater 1 enable line (leave unconnected for single-heater boards)
    output port heater1Write: Drv.GpioWrite

    @ Readback of master enable line, for error checking
    output port enableRead: Drv.GpioRead

    @ Readback of heater 0 enable line
    output port heater0Read: Drv.GpioRead

    @ Readback of heater 1 enable line (leave unconnected for single-heater boards)
    output port heater1Read: Drv.GpioRead

    # ----------------------------------------------------------------------
    # Telemetry
    # ----------------------------------------------------------------------

    @ Heater 0 enabled state
    telemetry Heater0Enabled: bool id 0

    @ Heater 1 enabled state
    telemetry Heater1Enabled: bool id 1

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    @ Heater 0 was enabled
    event Heater0Enabled \
      severity activity high \
      id 0 \
      format "Heater 0 enabled"

    @ Heater 0 was disabled
    event Heater0Disabled \
      severity activity high \
      id 1 \
      format "Heater 0 disabled"

    @ Heater 1 was enabled
    event Heater1Enabled \
      severity activity high \
      id 2 \
      format "Heater 1 enabled"

    @ Heater 1 was disabled
    event Heater1Disabled \
      severity activity high \
      id 3 \
      format "Heater 1 disabled"

    @ Heater 1 was commanded but is not connected on this board configuration
    event Heater1NotPresent \
      severity warning low \
      id 4 \
      format "Heater 1 command rejected: not present on this configuration"

    @ GPIO readback did not match the last commanded state
    event GpioMismatch(
                         line: string size 16 @< which line mismatched
                       ) \
      severity warning high \
      id 5 \
      format "GPIO mismatch on {}" \
      throttle 10

    # ----------------------------------------------------------------------
    # Standard component ports
    # ----------------------------------------------------------------------

    command recv port cmdIn
    command reg port cmdRegOut
    command resp port cmdResponseOut

    telemetry port tlmOut
    event port eventOut
    text event port textEventOut
    time get port timeCaller

  }
}
