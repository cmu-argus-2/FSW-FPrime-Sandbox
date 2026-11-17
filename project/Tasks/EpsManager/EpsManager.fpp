module project {

    # ----------------------------------------------------------------------
    # Types
    # ----------------------------------------------------------------------

    @ EPS power flag (EPS_POWER_FLAG in apps/eps/eps.py)
    enum EpsPowerFlag : U8 {
        NONE = 0
        LOW_POWER = 1
        NOMINAL = 2
    }

    @ The loads EPS raises power alerts for, in the spec's field order
    enum EpsLoad : U8 {
        MAINBOARD = 0
        PERIPHERAL = 1
        RADIO = 2
        JETSON = 3
        COIL_XP = 4
        COIL_XM = 5
        COIL_YP = 6
        COIL_YM = 7
        COIL_ZP = 8
        COIL_ZM = 9
    }

    @ How a fuel gauge cycle proceeded (EpsLogic::FuelGaugeMode)
    enum EpsFuelGaugeMode : U8 {
        USE_FUEL_GAUGE = 0
        SKIP_CYCLE = 1
        USE_CPU_FALLBACK = 2
    }

    @ One voltage and current pair
    struct EpsVi {
        voltageMv: I32 @< Voltage in mV
        currentMa: I32 @< Current in mA
    }

    @ The spec's 11-field line, fields 1-10. Field 0 (time) is the telemetry
    @ timestamp. Each field is 1 while that load's power is at or above its
    @ threshold.
    struct EpsAlerts {
        mainboard: U8 @< 1
        peripheral: U8 @< 2: GPS monitor
        radio: U8 @< 3
        jetson: U8 @< 4
        coilXp: U8 @< 5
        coilXm: U8 @< 6
        coilYp: U8 @< 7
        coilYm: U8 @< 8
        coilZp: U8 @< 9
        coilZm: U8 @< 10
    }

    @ The spec's 44-field line, fields 1-43. Field 0 (time) is the telemetry
    @ timestamp. Voltages in mV, currents in mA, temperatures in
    @ centi-degrees C, all truncated toward zero.
    struct EpsData {
        powerFlag: EpsPowerFlag @< 1
        mainboardTempCc: I32 @< 2: CPU temperature
        mainboard: EpsVi @< 3-4
        packTempCc: I32 @< 5
        ain1TempCc: I32 @< 6
        ain2TempCc: I32 @< 7
        dieTempCc: I32 @< 8
        socPercent: I32 @< 9
        capacityMah: I32 @< 10
        batteryCurrentMa: I32 @< 11: negative while discharging
        batteryVoltageMv: I32 @< 12
        midpointMv: I32 @< 13
        tteS: U32 @< 14: time to empty, seconds
        ttfS: U32 @< 15: time to full, seconds
        coilXp: EpsVi @< 16-17
        coilXm: EpsVi @< 18-19
        coilYp: EpsVi @< 20-21
        coilYm: EpsVi @< 22-23
        coilZp: EpsVi @< 24-25
        coilZm: EpsVi @< 26-27
        jetson: EpsVi @< 28-29
        radio: EpsVi @< 30-31
        gps: EpsVi @< 32-33
        solarXp: EpsVi @< 34-35: always 0, not in the device list
        solarXm: EpsVi @< 36-37: always 0
        solarYp: EpsVi @< 38-39: always 0
        solarYm: EpsVi @< 40-41: always 0
        heater0: U8 @< 42
        heater1: U8 @< 43
    }

    @ Read a temperature in centi-degrees C. Returns false if unavailable.
    port TemperatureRead(ref centiC: I32) -> bool

    # ----------------------------------------------------------------------
    # Component
    # ----------------------------------------------------------------------

    @ EPS task: the electrical power subsystem's flight logic, ported from
    @ flight/tasks/eps.py. Active, so the drivers' blocking I2C and GPIO
    @ calls run on this thread instead of stalling the rate group. The
    @ decision rules are in EpsLogic.hpp.
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

        @ CPU (mainboard) temperature. Left unconnected until the CPU
        @ temperature component exists.
        output port cpuTemp: project.TemperatureRead

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Run ticks handled since boot
        telemetry RunCount: U32

        @ Power alerts, published every run
        telemetry Alerts: EpsAlerts

        @ Full EPS record, published every fuel gauge cycle
        telemetry Data: EpsData

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ A load's power reached its threshold
        event PowerAlertRaised(
            load: EpsLoad @< Which load
            powerMw: I32 @< Measured power in mW
            thresholdMw: I32 @< Its threshold in mW
        ) \
            severity warning low \
            format "{} power {} mW is at or above its {} mW threshold"

        @ A load's power dropped back below its threshold
        event PowerAlertCleared(
            load: EpsLoad @< Which load
            powerMw: I32 @< Measured power in mW
        ) \
            severity activity low \
            format "{} power back below threshold: {} mW"

        @ The EPS power flag changed
        event PowerFlagChanged(
            oldFlag: EpsPowerFlag @< Flag before this cycle
            newFlag: EpsPowerFlag @< Flag after this cycle
        ) \
            severity activity high \
            format "EPS power flag {} -> {}"

        @ The fuel gauge reported an SOC outside 0-100; the flag is unchanged
        event InvalidStateOfCharge(
            socPercent: I32 @< Reported SOC
        ) \
            severity warning high \
            format "Fuel gauge SOC {}% is outside 0-100; EPS power flag unchanged" \
            throttle 5

        @ A fuel gauge cycle could not use the fuel gauge
        event FuelGaugeDegraded(
            fgMode: EpsFuelGaugeMode @< What the cycle did instead
            failedReads: U32 @< Register reads that failed
        ) \
            severity warning low \
            format "Fuel gauge cycle {}: {} failed reads" \
            throttle 10

        @ EPS switched a battery heater
        event HeaterSwitched(
            heater: project.HeaterId @< Which heater
            enabled: bool @< New state
            temperatureCc: I32 @< Temperature the decision used
        ) \
            severity activity high \
            format "Battery {} -> {} at {} cC"

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