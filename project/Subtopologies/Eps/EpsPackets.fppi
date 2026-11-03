# Telemetry packets for the Eps subtopology. Included by the packet set of
# every deployment that imports Eps. Packet ids 200-299.

  packet BatteryHeaters id 200 group 1 {
    Eps.batteryHeaters.Heater0Enabled
    Eps.batteryHeaters.Heater1Enabled
  }
