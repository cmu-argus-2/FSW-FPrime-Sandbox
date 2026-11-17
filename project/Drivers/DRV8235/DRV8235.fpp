module project {

    @ DRV8235 H-bridge driver for one magnetorquer coil.
    @
    @ The chip is unpowered until COIL_EN is high, so configure() only stores
    @ the address. The register setup runs on the first read, and runs again
    @ after any bus failure, so a coil rail that is switched off and back on
    @ is set up again once it answers.
    passive component DRV8235 {

        @ EPS task entry point: coil voltage and current. Both are negative
        @ while the bridge drives in reverse. Publishes no telemetry; the
        @ caller owns reporting.
        guarded input port read: project.PowerRead

        @ Rate group entry point for bench bring-up: sample, read the fault
        @ register, and publish telemetry. Not connected in flight.
        guarded input port poll: Svc.Sched

        @ Register read: write the register index, read one byte back
        output port busWriteRead: Drv.I2cWriteRead

        @ Register write: [register index, value]
        output port busWrite: Drv.I2c

        time get port timeCaller

        telemetry VoltageCode: U8 update always
        telemetry CurrentCode: U8 update always
        telemetry VoltageMv: I32 update always
        telemetry CurrentMa: I32 update always

        @ Raw FAULT_STATUS register (bench poll only)
        telemetry FaultStatus: U8

        event ConfigurationError(address: U32) \
            severity warning high format "Invalid DRV8235 config: addr={}"

        @ Throttled because both entry points run every cycle: a dead bus would
        @ otherwise emit one event per read. A successful read re-arms it.
        event I2cError(regAddr: U8, status: Drv.I2cStatus) \
            severity warning high format "DRV8235 I2C access to register {} failed: {}" \
            throttle 10

        @ The chip reported a fault. Stall, overcurrent and overtemperature
        @ disable the outputs until the flags are cleared. This driver does
        @ not clear them on its own; clearing is a deliberate action for
        @ whoever owns the coils.
        event DeviceFault(faultStatus: U8) \
            severity warning high format "DRV8235 fault, FAULT_STATUS=0x{x}" \
            throttle 5

        @ Register setup finished; the outputs are enabled and coasting at 0 V
        event Initialized(address: U32) \
            severity activity low format "DRV8235 at addr {} initialized"

        import Fw.Channel
        import Fw.Event
    }
}
