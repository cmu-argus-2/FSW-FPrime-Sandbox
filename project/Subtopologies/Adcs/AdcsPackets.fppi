# Telemetry packets for the Adcs subtopology. Included by the packet set of
# every deployment that imports Adcs. Packet ids 300-399.

  # One F32 per sensor, 36 bytes for all nine.
  packet LightSensorLux id 300 group 1 {
    Adcs.lightSensorXp.Lux
    Adcs.lightSensorXm.Lux
    Adcs.lightSensorYp.Lux
    Adcs.lightSensorYm.Lux
    Adcs.lightSensorZpXp.Lux
    Adcs.lightSensorZpYm.Lux
    Adcs.lightSensorZpXm.Lux
    Adcs.lightSensorZpYp.Lux
    Adcs.lightSensorZm.Lux
  }

  # Separate from the lux packet because the flags are only read on request,
  # so most of the time this packet is never sent.
  packet LightSensorFlags id 301 group 1 {
    Adcs.lightSensorXp.Flags
    Adcs.lightSensorXm.Flags
    Adcs.lightSensorYp.Flags
    Adcs.lightSensorYm.Flags
    Adcs.lightSensorZpXp.Flags
    Adcs.lightSensorZpYm.Flags
    Adcs.lightSensorZpXm.Flags
    Adcs.lightSensorZpYp.Flags
    Adcs.lightSensorZm.Flags
  }
