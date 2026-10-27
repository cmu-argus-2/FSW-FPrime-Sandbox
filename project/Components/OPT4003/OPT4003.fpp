module project {

    @ Threshold and overload flags from the OPT4003 FLAGS register.
    @ (`high` and `low` are reserved words in FPP, hence the longer names.)
    struct LightSensorFlags {
        overload: bool       @< The light level exceeded the full-scale range
        thresholdHigh: bool  @< The result crossed above the high threshold
        thresholdLow: bool   @< The result crossed below the low threshold
    }

    @ Verify the device ID and write the configuration register.
    @ Returns FAILURE if the part does not answer or is not an OPT4003.
    port LightSensorBoot -> Fw.Success

    @ Take one illuminance sample. `lux` is only written on SUCCESS.
    port LightSensorRead(
        ref lux: F32 @< Illuminance in lux
    ) -> Fw.Success

    @ Read the threshold and overload flags. `flags` is only written on
    @ SUCCESS.
    port LightSensorFlagsRead(
        ref flags: LightSensorFlags @< Filled in with the current flags
    ) -> Fw.Success

    @ OPT4003 ambient light sensor device manager.
    @
    @ The OPT4003 is a digital ambient light sensor with a 20-bit mantissa and
    @ a 4-bit exponent, giving roughly 0.5 mlux to 144 klux in auto-range.
    @ Registers are 16 bits wide and transferred most significant byte first.
    @
    @ The spacecraft carries nine of these, one per face plus four angled on
    @ the +Z face, so this component is instantiated once per sensor and each
    @ instance is given its address with configure().
    @
    @ A sensor that fails boot is treated as absent: `read` and `readFlags`
    @ return FAILURE without touching the bus until a later `boot` succeeds.
    passive component OPT4003 {

        # ----------------------------------------------------------------------
        # I2C bus ports
        # ----------------------------------------------------------------------

        @ Write a register address then read that register's 16-bit
        @ big-endian value back, using a repeated start.
        output port busWriteRead: Drv.I2cWriteRead

        @ Write-only transaction. Used to program the configuration register.
        output port busWrite: Drv.I2c

        # ----------------------------------------------------------------------
        # Entry points
        #
        # Passive for the same reason as the other bus drivers: every sensor
        # shares the I2C bus, so a thread per driver buys no concurrency. The
        # ADCS task is the intended caller, and the blocking bus time is spent
        # out of its budget. Guarded so two callers cannot interleave a
        # multi-register read on the same device.
        # ----------------------------------------------------------------------

        @ Verify the device ID and write the configuration register.
        guarded input port boot: project.LightSensorBoot

        @ Wait for a conversion, read the result, publish and return lux.
        guarded input port read: project.LightSensorRead

        @ Read the threshold and overload flags.
        guarded input port readFlags: project.LightSensorFlagsRead

        @ Rate-group entry point for bring-up without a task driving the
        @ sensor: boots the sensor if it has not booted yet, then samples it,
        @ keeping only the telemetry.
        guarded input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Illuminance in lux. lux = (mantissa << exponent) * 535e-6.
        telemetry Lux: F32

        @ Raw FLAGS register. Bit 3 overload, bit 2 conversion ready, bit 1
        @ above high threshold, bit 0 below low threshold.
        telemetry Flags: U16

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ An I2C transaction failed. Throttled because this component samples
        @ every ADCS cycle; the throttle is cleared after any read in which
        @ every transaction succeeded.
        @ Note: the parameter is `regAddr`, not `reg`, because `reg` is a
        @ reserved word in FPP.
        event I2cError(
            regAddr: U8 @< The device register being accessed
            status: Drv.I2cStatus @< The failure reported by the bus driver
        ) \
            severity warning high \
            format "OPT4003 I2C error on register 0x{x}: {}" \
            throttle 5

        @ The DEVICE_ID register did not identify an OPT4003. Either a
        @ different part answers at this address or the read was corrupted.
        @ Throttled because `run` retries boot every tick; cleared by a
        @ successful boot.
        event WrongDeviceId(
            deviceId: U16 @< Raw DEVICE_ID register contents
        ) \
            severity warning high \
            format "OPT4003 device ID mismatch: read 0x{x}, expected 0x0221" \
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
