module project {

    @ One power monitor sample in engineering units.
    @ Signed so the same type can carry a bidirectional reading: the DRV8235
    @ torque coil driver reports negative values while driving in reverse.
    struct PowerSample {
        voltageMv: I32 @< Voltage in mV
        currentMa: I32 @< Current in mA
    }

    @ Take one sample. On I2C_OK, `sample` holds the new values. On any
    @ failure `sample` is left as the caller passed it, so a caller that
    @ zeroes it first sees 0 for a device that is absent or failing.
    @ I2C_OPEN_ERR means the device was never configured successfully.
    port PowerRead(ref sample: PowerSample) -> Drv.I2cStatus

    @ ADM1176 voltage and current monitor driver.
    passive component ADM1176 {

        @ EPS task entry point: sample the monitor and hand the values back.
        @ Publishes no telemetry; the caller owns reporting.
        guarded input port read: project.PowerRead

        @ Rate group entry point for bench bring-up: sample and publish
        @ telemetry. Not connected in flight, where EPS calls `read`.
        guarded input port poll: Svc.Sched

        output port i2cWrite: Drv.I2c
        output port i2cRead: Drv.I2c
        time get port timeCaller

        telemetry VoltageCode: U16 update always
        telemetry CurrentCode: U16 update always
        telemetry VoltageMv: U32 update always
        telemetry CurrentMa: U32 update always

        event ConfigurationError(address: U32, senseResistanceMilliOhms: U32) \
            severity warning high format "Invalid ADM1176 config: addr={} sense_mOhm={}"

        @ Throttled because both entry points run every cycle: a dead bus would
        @ otherwise emit one event per read. A successful read re-arms it.
        event I2cError(operation: U8, status: Drv.I2cStatus) \
            severity warning high format "ADM1176 I2C operation {} failed: {}" \
            throttle 10

        import Fw.Channel
        import Fw.Event
    }
}