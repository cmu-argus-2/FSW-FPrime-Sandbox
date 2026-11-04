# Telemetry packets for the Eps subtopology. Included by the packet set of
# every deployment that imports Eps. Packet ids 200-299.

  packet BatteryHeaters id 200 group 1 {
    Eps.batteryHeaters.Heater0Enabled
    Eps.batteryHeaters.Heater1Enabled
  }

  packet EpsTask id 201 group 1 {
    Eps.epsManager.RunCount
  }

  # Written by MAX17205's read port, so it updates every fuel gauge cycle.
  packet FuelGauge id 202 group 1 {
    Eps.fuelGauge.Status
    Eps.fuelGauge.Voltage
    Eps.fuelGauge.Current
    Eps.fuelGauge.MidVoltage
    Eps.fuelGauge.StateOfCharge
    Eps.fuelGauge.RemainingCapacity
    Eps.fuelGauge.FullCapacity
    Eps.fuelGauge.Cycles
    Eps.fuelGauge.TimeToEmpty
    Eps.fuelGauge.TimeSincePowerUp
    Eps.fuelGauge.TimeToFull
    Eps.fuelGauge.Temperature
    Eps.fuelGauge.TemperatureAin1
    Eps.fuelGauge.TemperatureAin2
    Eps.fuelGauge.TemperatureDie
  }

  # Written only by ADM1176's poll port (bench bring-up). EPS calls read,
  # which publishes nothing, so in flight this packet never updates and is
  # never sent.
  packet PowerMonitorsBench id 203 group 2 {
    Eps.pmMainboard.VoltageCode
    Eps.pmMainboard.CurrentCode
    Eps.pmMainboard.VoltageMv
    Eps.pmMainboard.CurrentMa
    Eps.pmGps.VoltageCode
    Eps.pmGps.CurrentCode
    Eps.pmGps.VoltageMv
    Eps.pmGps.CurrentMa
    Eps.pmRadio.VoltageCode
    Eps.pmRadio.CurrentCode
    Eps.pmRadio.VoltageMv
    Eps.pmRadio.CurrentMa
    Eps.pmJetson.VoltageCode
    Eps.pmJetson.CurrentCode
    Eps.pmJetson.VoltageMv
    Eps.pmJetson.CurrentMa
  }