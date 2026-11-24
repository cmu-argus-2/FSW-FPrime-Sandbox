module project {

    # ----------------------------------------------------------------------
    # Types
    #
    # Values match AdcsLogic.hpp, which uses the same numbering.
    # ----------------------------------------------------------------------

    @ ADCS mode, from the angular rate.
    enum AdcsMode: U8 {
        TUMBLING = 0     @< Spinning faster than the stable bound
        STABLE = 1       @< Within the stable bound, still being detumbled
        SUN_POINTING = 2 @< Reserved for the sun-pointing controller
        ACS_OFF = 3      @< Detumbled; control is off
        VF_TUMBLING = 4  @< Too fast for the coils to help; control is off
    }

    @ Control law. Sun pointing needs the sun vector, which this task does
    @ not compute yet.
    enum AdcsController: U8 {
        BDOT = 0   @< Oppose the rate of change of the magnetic field
        BCROSS = 1 @< u = -k (B x omega)
    }

    @ Satellite operating state, as the state manager will report it.
    enum SatelliteState: U8 {
        STARTUP = 0
        DETUMBLING = 1
        NOMINAL = 2
        LOW_POWER = 3
        EXPERIMENT = 4
    }

    @ Result of a sensor reading's plausibility check.
    enum AdcsSensorStatus: U8 {
        OK = 0
        GYRO_FAIL = 21 @< Read failed, or faster than 2000 deg/s
        MAG_FAIL = 31  @< Read failed, or outside 1 uT..2.5 mT
    }

    @ Dipole command, before allocation to the coils. Each axis in -1..1.
    array AdcsDipole = [3] F32

    @ Normalized throttle per coil: XP, XM, YP, YM, ZP, ZM. Each in -1..1.
    array CoilThrottles = [6] F32

    # ----------------------------------------------------------------------
    # Component
    # ----------------------------------------------------------------------

    @ ADCS task: detumbling with the B-dot or B-cross control law.
    @
    @ Each 1 Hz run reads the magnetometer and the gyroscope, updates the
    @ mode, then runs one blocking control cycle on this thread:
    @
    @ - B-dot: six magnetometer samples 80 ms apart, the coils on for
    @   500 ms, then off (about 0.9 s).
    @ - B-cross: the gyroscope re-read and the coils updated every 50 ms
    @   for 1 s, then off.
    @
    @ The satellite state gates the run: STARTUP does nothing, LOW_POWER
    @ skips sensing and control, and DETUMBLING and NOMINAL set the
    @ gyroscope range to 2000 and 125 deg/s.
    @
    @ Known limitations, until the parts they need exist:
    @ - The coils are not connected. Every coil is treated as unavailable,
    @   so the allocated throttles are 0; the dipole command is still
    @   computed and published.
    @ - The satellite state and the controller are set by command rather
    @   than by the state manager and persistent storage.
    @ - The magnetometer is used uncalibrated (zero bias, unit scale).
    active component AdcsManager {

        # ----------------------------------------------------------------------
        # Scheduling
        # ----------------------------------------------------------------------

        @ 1 Hz tick from Base.rateGroup_1Hz. `drop`: a B-cross cycle fills
        @ the whole second, so a tick that arrives while one is still running
        @ is discarded instead of queuing up behind it.
        async input port run: Svc.Sched drop

        # ----------------------------------------------------------------------
        # IMU
        # ----------------------------------------------------------------------

        @ Boot the IMU. Retried on every run until it succeeds.
        output port imuBoot: project.ImuBoot

        @ Gyroscope, rad/s
        output port imuReadGyro: project.ImuVectorRead

        @ Magnetometer, uT
        output port imuReadMag: project.ImuVectorRead

        @ Read the gyroscope range back from the part
        output port imuGetGyroRange: project.ImuGyroRangeGet

        @ Change the gyroscope range
        output port imuSetGyroRange: project.ImuGyroRangeSet

        # ----------------------------------------------------------------------
        # Commands
        #
        # Async: each is handled between runs, on this thread, so it can never
        # change the state or the controller partway through a cycle.
        # ----------------------------------------------------------------------

        @ Select the control law. Takes effect on the next run.
        async command SET_CONTROLLER(
            controller: AdcsController @< The control law to use
        ) \
            opcode 0

        @ Set the satellite state, standing in for the state manager.
        @ Takes effect on the next run.
        async command SET_SATELLITE_STATE(
            $state: SatelliteState @< The state to run in
        ) \
            opcode 1

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Satellite state the task is running in
        telemetry SatState: SatelliteState

        @ Control law in use
        telemetry Controller: AdcsController

        @ Mode after this run's update
        telemetry Mode: AdcsMode

        @ Angular rate in rad/s, from the last gyroscope read of the run
        telemetry Gyro: ImuVector

        @ Gyroscope plausibility check result
        telemetry GyroStatus: AdcsSensorStatus

        @ Magnetic field in T, from the last magnetometer read of the run
        telemetry Mag: ImuVector

        @ Magnetometer plausibility check result
        telemetry MagStatus: AdcsSensorStatus

        @ Dipole command of the run's last control step, before allocation
        telemetry Dipole: AdcsDipole

        @ Throttle allocated to each coil by the run's last control step
        telemetry Throttles: CoilThrottles

        @ Time the run took, from the tick to the end of the control cycle,
        @ in microseconds
        telemetry CycleTimeUs: U32

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ The mode changed.
        event ModeChanged(
            previous: AdcsMode @< Mode before this run
            current: AdcsMode @< Mode from this run on
        ) \
            severity activity high \
            format "ADCS mode {} -> {}"

        @ SET_CONTROLLER was accepted.
        event ControllerSet(
            controller: AdcsController @< The new control law
        ) \
            severity activity high \
            format "ADCS controller set to {}"

        @ SET_SATELLITE_STATE was accepted.
        event SatelliteStateSet(
            $state: SatelliteState @< The new state
        ) \
            severity activity high \
            format "ADCS satellite state set to {}"

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
