module project {

    @ MAX17205 fuel-gauge device manager.
    active component MAX17205 {

        @ Write a register address and read its 16-bit little-endian value.
        output port busWriteRead: Drv.I2cWriteRead

        @ Write a register and value to the fuel-gauge device.
        output port busWrite: Drv.I2c

        @ Read all supported fuel-gauge measurements and publish telemetry.
        async command READ_ALL

        @ Reset the fuel-gauge IC.
        async command RESET

        @ Raw battery pack voltage in mV.
        telemetry Voltage: F32

        @ Battery current in mA.
        telemetry Current: F32

        @ Lowest cell voltage in mV.
        telemetry MidVoltage: F32

        @ Reported state of charge in percent.
        telemetry StateOfCharge: F32

        @ Reported remaining capacity in mAh.
        telemetry RemainingCapacity: F32

        @ Full capacity estimation in mAh.
        telemetry FullCapacity: F32

        @ Battery cycle count.
        telemetry Cycles: U32

        @ Time to empty in seconds.
        telemetry TimeToEmpty: F32

        @ Time to full in seconds.
        telemetry TimeToFull: F32

        @ Time since power-up in seconds.
        telemetry TimeSincePowerUp: U32

        @ Battery temperature in centi-Celsius.
        telemetry Temperature: F32

        @ AIN1 thermistor temperature in centi-Celsius.
        telemetry TemperatureAin1: I32

        @ AIN2 thermistor temperature in centi-Celsius.
        telemetry TemperatureAin2: I32

        @ Internal die temperature in centi-Celsius.
        telemetry TemperatureDie: I32

        @ I2C transaction failed while reading or resetting the device.
        event I2cError(status: Drv.I2cStatus) severity warning high format "MAX17205 I2C error: {}"

        @ Port for event timestamps.
        time get port timeCaller

        import Fw.Command
        import Fw.Event
        import Fw.Channel
    }
}
