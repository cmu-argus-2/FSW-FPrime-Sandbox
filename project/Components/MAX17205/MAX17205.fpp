module project {

    @ One complete sample of the MAX17205, decoded into physical units.
    @
    @ Handed back by the `read` port so a calling task gets the values and the
    @ telemetry from a single bus transaction. The driver keeps no copy of
    @ this: a caller that wants the last reading without touching the bus
    @ should hold on to the struct it was given.
    struct BatteryState {
        voltage: F32             @< Pack voltage in mV
        current: F32             @< Current in mA, negative while discharging
        midVoltage: F32          @< Cell/midpoint voltage in mV
        stateOfCharge: F32       @< Reported state of charge in percent
        remainingCapacity: F32   @< Reported remaining capacity in mAh
        fullCapacity: F32        @< Full capacity estimate in mAh
        cycles: U16              @< Charge/discharge cycle count, raw counts
        timeToEmpty: F32         @< Time to empty in seconds
        timeToFull: F32          @< Time to full in seconds
        timeSincePowerUp: U16    @< TimerH, raw counts (3.2 h per count)
        temperature: F32         @< Pack temperature in centi-Celsius, signed
        temperatureAin1: I32     @< AIN1 thermistor in centi-Celsius
        temperatureAin2: I32     @< AIN2 thermistor in centi-Celsius
        temperatureDie: I32      @< Internal die temperature in centi-Celsius
    }

    @ Sample the fuel gauge.
    @
    @ Fields of `state` whose register read failed are left at whatever the
    @ caller passed in, so a caller reusing one struct keeps its previous value
    @ for those fields rather than seeing a zero. The return value says how
    @ many reads failed, and the I2cError event names which registers.
    port FuelGaugeRead(
        ref sample: BatteryState @< Filled in with this sample
    ) -> U32

    @ Request a power-on reset of the fuel gauge.
    port FuelGaugeReset -> Drv.I2cStatus

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
    passive component MAX17205 {

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
        # Entry points
        #
        # This component is passive: it owns no thread, so every handler below
        # runs on the thread of whoever invokes it, and the blocking I2C
        # traffic is spent out of that caller's time budget. The EPS task is
        # the intended caller.
        #
        # Passive is the right shape here because every peripheral shares one
        # I2C bus. Only one transaction can be in flight at a time no matter
        # how many threads exist, so giving each driver its own thread would
        # buy no concurrency while costing 8 KB of stack apiece. The threads
        # belong one level up, at the task layer, where work really can
        # overlap. Mutual exclusion on the bus is already handled: the I2C
        # driver's ports are guarded.
        #
        # The ports here are guarded rather than sync so that two callers --
        # the EPS task and a rate group, say -- cannot interleave register
        # reads on the same device.
        # ----------------------------------------------------------------------

        @ Read every register, publish telemetry, and hand the decoded values
        @ back to the caller. Returns the number of register reads that failed,
        @ so 0 means a complete snapshot. Reads are independent: one bad
        @ register does not abort the rest.
        guarded input port read: project.FuelGaugeRead

        @ Request a power-on reset of the gauge.
        @ Destructive: this discards the learned capacity model.
        guarded input port reset: project.FuelGaugeReset

        @ Rate-group entry point for sampling without a task driving it:
        @ performs the same work as `read` and discards the values, leaving
        @ only the telemetry. Useful for bring-up and standalone operation.
        guarded input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Commands
        #
        # There are none, deliberately. A passive component may not have async
        # input, so a command here would have to be sync, and would then run
        # its I2C inside the command dispatcher's thread and block every other
        # command in the system for the duration. The ground-facing commands
        # for the fuel gauge belong on the task that owns it.
        # ----------------------------------------------------------------------

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

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channel handling
        import Fw.Channel
    }
}
