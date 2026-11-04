module project {

    @ EPS task: the electrical power subsystem's flight logic, ported from
    @ flight/tasks/eps.py. Active, so the drivers' blocking I2C and GPIO
    @ calls run on this thread instead of stalling the rate group.
    @
    @ Skeleton: only the fuel gauge is read so far. Power monitors, alerts,
    @ the power flag and heater control come in later steps.
    active component EpsManager {

        # ----------------------------------------------------------------------
        # Scheduling and health
        # ----------------------------------------------------------------------

        @ 1 Hz tick from Base.rateGroup_1Hz. `drop`: if EPS is still busy with
        @ the previous cycle (a slow or wedged I2C bus), the new tick is
        @ discarded instead of overflowing the queue and asserting.
        async input port run: Svc.Sched drop

        @ Health ping in
        async input port pingIn: Svc.Ping

        @ Health ping response
        output port pingOut: Svc.Ping

        # ----------------------------------------------------------------------
        # Drivers
        # ----------------------------------------------------------------------

        @ Power monitors: 0 mainboard, 1 GPS (peripheral), 2 radio, 3 Jetson
        output port powerMonitors: [4] project.PowerRead

        @ Battery fuel gauge
        output port fuelGauge: project.FuelGaugeRead

        @ Battery heater control
        output port heaterSet: project.HeaterSet

        @ Battery heater state
        output port heaterGet: project.HeaterGet

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Run ticks handled since boot
        telemetry RunCount: U32

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Bring-up only: one fuel gauge sample. Replaced by the EpsData struct
        @ channel once the full fuel gauge cycle is implemented.
        event FuelGaugeSampled(
            stateOfCharge: F32 @< Reported state of charge in percent
            failedReads: U32 @< Register reads that failed this sample
        ) \
            severity activity low \
            format "Fuel gauge: SOC {.1f}%, {} failed reads"

        # ----------------------------------------------------------------------
        # Standard AC ports
        # ----------------------------------------------------------------------

        @ Port for requesting the current time
        time get port timeCaller

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channel handling
        import Fw.Channel
    }
}