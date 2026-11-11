module project {

    @ I2C bus scanner: a bench tool, not flight software.
    @
    @ On command, probes every non-reserved 7-bit address (0x08-0x77) on each
    @ connected bus and reports which ones acknowledged. Each probe is a
    @ 1-byte read: a device that is present ACKs its address and the read
    @ succeeds; an empty address NACKs and the read fails.
    passive component I2cScanner {

        @ i2c0 bus driver's read port. Leave unconnected to skip the bus.
        output port bus0Read: Drv.I2c

        @ i2c1 bus driver's read port. Leave unconnected to skip the bus.
        output port bus1Read: Drv.I2c

        @ Scan every connected bus. Runs on the command dispatcher's thread,
        @ which is acceptable for a bench tool: a scan takes tens of
        @ milliseconds per bus.
        sync command SCAN

        @ One bus's result. Event strings are capped at FW_LOG_STRING_MAX_SIZE
        @ (200) characters, which holds about 40 addresses. If more answer --
        @ every address does when SDA is held low -- the list ends in "..."
        @ and the count is still exact.
        event BusScanned(
            bus: U8 @< 0 for i2c0, 1 for i2c1
            count: U32 @< Number of addresses that acknowledged
            addresses: string size 200 @< Addresses that acknowledged, in hex
        ) \
            severity activity high \
            format "i2c{}: {} device(s): {}"

        @ Port for requesting the current time, used to timestamp events.
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event
    }
}
