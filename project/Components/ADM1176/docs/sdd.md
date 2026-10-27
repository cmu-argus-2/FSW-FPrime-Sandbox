# project::ADM1176

Passive F´ component for polling voltage and current from an ADM1176 over the
`Drv.I2c` interface. Connect `i2cWrite` and `i2cRead` to the bus driver and
connect `poll` to a rate group. Call `configure(address, senseResistanceMilliOhms,
lowVoltageRange)` before polling. The address is the 7-bit address; the range
selects the ADM1176 7:2 (true) or 14:1 (false) voltage divider.

Configuration starts continuous voltage and current conversion. Every poll
reads the three-byte ADC result and publishes 12-bit raw codes, millivolts,
and milliamps. Current scaling uses the configured sense resistance. I²C
failures are reported in `I2cError`; no new telemetry sample is published for
that poll.

Conversion scales and byte layout follow the [ADM1176 data sheet, Rev. C](https://www.analog.com/media/en/technical-documentation/data-sheets/adm1176.pdf).
