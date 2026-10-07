module project {
    @ ADM1176 voltage and current monitor driver.
    passive component ADM1176 {
        sync input port poll: Svc.Sched
        output port i2cWrite: Drv.I2c
        output port i2cRead: Drv.I2c
        time get port timeCaller

        telemetry VoltageCode: U16 update always
        telemetry CurrentCode: U16 update always
        telemetry VoltageMv: U32 update always
        telemetry CurrentMa: U32 update always

        event ConfigurationError(address: U32, senseResistanceMilliOhms: U32) \
            severity warning high format "Invalid ADM1176 config: addr={} sense_mOhm={}"
        event I2cError(operation: U8, status: Drv.I2cStatus) \
            severity warning high format "ADM1176 I2C operation {} failed: {}"

        import Drv.I2c
        import Fw.Channel
        import Fw.Event
    }
}
