module project {

    @ One three-axis sample in physical units.
    struct ImuVector {
        x: F32 @< X axis
        y: F32 @< Y axis
        z: F32 @< Z axis
    }

    @ Gyroscope full-scale range. The values are the GYR_RANGE (0x43)
    @ register codes, so a value can be written to the part unchanged.
    enum GyroRange: U8 {
        DPS_2000 = 0 @< +/-2000 deg/s
        DPS_1000 = 1 @< +/-1000 deg/s
        DPS_500 = 2  @< +/-500 deg/s
        DPS_250 = 3  @< +/-250 deg/s
        DPS_125 = 4  @< +/-125 deg/s
    }

    @ Error flags from ERR_REG (0x02).
    @ (`fatal` is a reserved word in FPP, hence the longer names.)
    struct ImuErrors {
        dropCommandError: bool @< A write to CMD arrived while it was busy
        fatalError: bool       @< Chip not operable; cleared only by power-on reset
    }

    @ Soft-reset the part, verify its chip ID, and configure it.
    @ Returns FAILURE if the part does not answer, is not a BMX160, or
    @ rejects a setting.
    port ImuBoot -> Fw.Success

    @ Take one three-axis sample. `sample` is only written on SUCCESS.
    port ImuVectorRead(
        ref sample: ImuVector @< Filled in with this sample
    ) -> Fw.Success

    @ Change the gyroscope full-scale range.
    port ImuGyroRangeSet(
        $range: GyroRange @< The new range
    ) -> Fw.Success

    @ Read the gyroscope full-scale range back from the part.
    @ `$range` is only written on SUCCESS.
    port ImuGyroRangeGet(
        ref $range: GyroRange @< Filled in with the current range
    ) -> Fw.Success

    @ Read the error flags. `errors` is only written on SUCCESS.
    port ImuErrorsRead(
        ref errors: ImuErrors @< Filled in with the current flags
    ) -> Fw.Success

    @ BMX160 accelerometer, gyroscope and magnetometer device manager.
    @
    @ The BMX160 packages a BMI160 accelerometer/gyroscope with a BMM150
    @ magnetometer. The BMM150 sits on a private bus behind the BMI160 core
    @ and is reached only through the BMI160's magnetometer interface
    @ registers (0x4B-0x4F), so this one component owns the whole chip on
    @ I2C. After boot the interface polls the BMM150 by itself, and the
    @ latest magnetometer sample is read like the other two sensors.
    @
    @ Data registers are little-endian. Each sensor's data block is read in
    @ one burst, which the part treats as an atomic unit, so the three axes
    @ always come from the same sample.
    @
    @ A part that fails boot is treated as absent: every read port returns
    @ FAILURE without touching the bus until a later `boot` succeeds.
    passive component BMX160 {

        # ----------------------------------------------------------------------
        # I2C bus ports
        # ----------------------------------------------------------------------

        @ Write a register address, then read one or more consecutive
        @ registers back using a repeated start.
        output port busWriteRead: Drv.I2cWriteRead

        @ Write-only transaction: a register address followed by one byte.
        output port busWrite: Drv.I2c

        # ----------------------------------------------------------------------
        # Entry points
        #
        # Passive, like the other bus drivers: every device shares the I2C
        # bus, so a thread per driver buys no concurrency. The ADCS task is
        # the intended caller, and the blocking bus time is spent out of its
        # budget. Guarded so two callers cannot interleave transactions on the
        # same device. Each sensor has its own read port so a caller can
        # sample the gyroscope faster than the others.
        # ----------------------------------------------------------------------

        @ Soft-reset, identify and configure the part.
        guarded input port boot: project.ImuBoot

        @ Read the gyroscope in rad/s and publish it.
        guarded input port readGyro: project.ImuVectorRead

        @ Read the accelerometer in m/s^2 and publish it.
        guarded input port readAccel: project.ImuVectorRead

        @ Read the magnetometer in uT and publish it.
        guarded input port readMag: project.ImuVectorRead

        @ Change the gyroscope full-scale range.
        guarded input port setGyroRange: project.ImuGyroRangeSet

        @ Read the gyroscope full-scale range back from the part.
        guarded input port getGyroRange: project.ImuGyroRangeGet

        @ Read the error flags.
        guarded input port readErrors: project.ImuErrorsRead

        @ Rate-group entry point for bring-up without a task driving the
        @ part: boots it if it has not booted yet, then samples all three
        @ sensors, keeping only the telemetry.
        guarded input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Angular rate in rad/s.
        telemetry Gyro: ImuVector

        @ Acceleration in m/s^2.
        telemetry Accel: ImuVector

        @ Magnetic field in uT, compensated with the part's factory trim.
        telemetry Mag: ImuVector

        @ PMU_STATUS (0x03) read at the end of boot: the power mode each
        @ sensor actually reached. Bits 5:4 accelerometer (01 normal), 3:2
        @ gyroscope (01 normal), 1:0 magnetometer interface (10 low power).
        @ A configured part normally reads 0x12: the gyroscope takes up to
        @ 55 ms to leave suspend, longer than boot waits for it, so its field
        @ still reads 00 here, then 01 once it is running. A sensor whose data
        @ stays all zeros was never woken. Telemetry rather than an event so
        @ it is still visible to a ground station that connects after boot.
        telemetry PmuStatus: U8

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
            format "BMX160 I2C error on register 0x{x}: {}" \
            throttle 5

        @ CHIP_ID (0x00) did not identify a BMX160. Either a different part
        @ answers at this address or the read was corrupted. Throttled
        @ because `run` retries boot every tick; cleared by a successful boot.
        event WrongChipId(
            chipId: U8 @< Raw CHIP_ID register contents
        ) \
            severity warning high \
            format "BMX160 chip ID mismatch: read 0x{x}, expected 0xD8" \
            throttle 5

        @ ERR_REG was not clean after a configuration write, so the part
        @ rejected the setting and boot was abandoned.
        event SettingRejected(
            regAddr: U8 @< The register that was written
            errReg: U8 @< Raw ERR_REG contents read afterwards
        ) \
            severity warning high \
            format "BMX160 rejected the write to register 0x{x}: ERR_REG 0x{x}" \
            throttle 5

        @ A setup-mode transfer to or from the BMM150 was still in progress
        @ when its timeout expired, so boot was abandoned.
        event MagTransferTimeout(
            bmmReg: U8 @< The BMM150 register being accessed
        ) \
            severity warning high \
            format "BMX160 magnetometer transfer for BMM150 register 0x{x} timed out" \
            throttle 5

        @ A range register read back a value that is not a valid range, so
        @ no scale factor can be derived from it and the operation failed.
        event InvalidRangeReadback(
            regAddr: U8 @< The range register that was read
            value: U8 @< The value read back
        ) \
            severity warning high \
            format "BMX160 register 0x{x} read back 0x{x}, which is not a valid range" \
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
