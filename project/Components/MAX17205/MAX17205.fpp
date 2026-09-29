module project {

    @ MAX17205 fuel-gauge device manager.
    @
    @ The MAX17205 is a multi-cell battery fuel gauge. It tracks pack voltage,
    @ current, state of charge, remaining and full capacity, cycle count, time
    @ to empty/full, and four temperatures (pack, two external thermistors and
    @ its own die).
    @
    @ Every measurement is a 16-bit little-endian register read over I2C. The
    @ part answers on two 7-bit addresses: 0x36 for the fuel-gauge register
    @ bank, and 0x0B for shadow RAM, which is where the thermistor and die
    @ temperatures live. Both are driven through the same bus ports here, with
    @ the address selected per transaction.
    @
    @ The component is active because every I2C transaction blocks. Giving it
    @ its own thread keeps that blocking off the rate group and off the command
    @ dispatcher.
    active component MAX17205 {

        # ----------------------------------------------------------------------
        # I2C bus ports
        # ----------------------------------------------------------------------

        @ Write a register address then read that register's 16-bit
        @ little-endian value back, using a repeated start. This is how every
        @ measurement is taken.
        output port busWriteRead: Drv.I2cWriteRead

        @ Write-only transaction. Used only by RESET, which pushes
        @ CONFIG2 = 0x0001 and gets nothing back.
        output port busWrite: Drv.I2c

        # ----------------------------------------------------------------------
        # Scheduled sampling
        # ----------------------------------------------------------------------

        @ Rate group input. Each call samples every register and publishes
        @ telemetry, the same work READ_ALL does on demand. It is async so the
        @ blocking I2C traffic happens on this component's own thread rather
        @ than stalling the rate group, and `drop` means a slow or wedged bus
        @ backs up harmlessly instead of overflowing the queue and asserting.
        async input port run: Svc.Sched drop

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Read every supported measurement and publish telemetry.
        @ Reads are independent: a failure on one register does not stop the
        @ others, so a single flaky register cannot cost an entire snapshot.
        @ The command responds EXECUTION_ERROR if any read failed.
        async command READ_ALL

        @ Reset the fuel-gauge IC by writing CONFIG2 = 0x0001.
        async command RESET

        # ----------------------------------------------------------------------
        # Telemetry
        #
        # Scaling constants are per the MAX17205 datasheet with the board's
        # 10 mOhm sense resistor. Channels reported raw are noted as such.
        # ----------------------------------------------------------------------

        @ Alert status and chip status
        telemetry Status: U16

        @ Battery pack voltage in mV. Register VBat, 1.25 mV per count.
        telemetry Voltage: F32

        @ Battery current in mA. Signed; negative means discharging.
        @ 1.5625 uV per count across a 10 mOhm sense resistor.
        telemetry Current: F32

        @ Cell voltage in mV. Register VCell, 0.078125 mV per count.
        @ On a 2S pack this register reads the midpoint tap between the cells,
        @ which is why it is reported separately from pack voltage.
        telemetry MidVoltage: F32

        @ Reported state of charge in percent. Register RepSOC, 1/256 % per count.
        telemetry StateOfCharge: F32

        @ Reported remaining capacity in mAh. Register RepCap, 0.5 mAh per count.
        telemetry RemainingCapacity: F32

        @ Full capacity estimate in mAh. Register FullCapRep, 0.5 mAh per count.
        telemetry FullCapacity: F32

        @ Charge/discharge cycle count, raw register counts, unscaled.
        telemetry Cycles: U16

        @ Time to empty in seconds. Register TTE, 5.625 s per count.
        telemetry TimeToEmpty: F32

        @ Time since power-up, raw TimerH register counts, unscaled.
        @ One count is 3.2 hours if real units are wanted on the ground.
        telemetry TimeSincePowerUp: U16

        @ Time to full in seconds. Register TTF, 5.625 s per count.
        telemetry TimeToFull: F32

        @ Battery pack temperature in centi-Celsius. Register Temp,
        @ 0.390625 centi-Celsius per count.
        telemetry Temperature: F32

        @ AIN1 thermistor temperature in centi-Celsius, from shadow RAM.
        telemetry TemperatureAin1: I32

        @ AIN2 thermistor temperature in centi-Celsius, from shadow RAM.
        telemetry TemperatureAin2: I32

        @ MAX17205 internal die temperature in centi-Celsius, from shadow RAM.
        telemetry TemperatureDie: I32

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ An I2C transaction failed. Carries the register being accessed so a
        @ fault can be traced to a specific read.
        @ Throttled because this component samples on a rate group: an
        @ unthrottled event would flood the downlink if the bus goes away.
        @ The throttle is cleared after any sample in which every read
        @ succeeded, so a bus that recovers starts reporting again.
        @ Note: the parameter is `regAddr`, not `reg`, because `reg` is a
        @ reserved word in FPP.
        event I2cError(
            regAddr: U8 @< The device register being accessed
            status: Drv.I2cStatus @< The failure reported by the bus driver
        ) \
            severity warning high \
            format "MAX17205 I2C error on register 0x{x}: {}" \
            throttle 5

        # ----------------------------------------------------------------------
        # Standard AC ports
        # ----------------------------------------------------------------------

        @ Port for requesting the current time, used to timestamp events and
        @ telemetry.
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channel handling
        import Fw.Channel
    }
}
