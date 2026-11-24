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

  # All three sensors are sampled together, so one packet carries them:
  # three ImuVector channels and the boot-time power modes, 37 bytes of data.
  # PmuStatus is written once, at boot, and rides along in every packet.
  packet Imu id 302 group 1 {
    Adcs.imu.Gyro
    Adcs.imu.Accel
    Adcs.imu.Mag
    Adcs.imu.PmuStatus
  }

  # ADCS task: one packet per run, 69 bytes of data.
  packet AdcsTask id 303 group 1 {
    Adcs.adcsManager.SatState
    Adcs.adcsManager.Controller
    Adcs.adcsManager.Mode
    Adcs.adcsManager.Gyro
    Adcs.adcsManager.GyroStatus
    Adcs.adcsManager.Mag
    Adcs.adcsManager.MagStatus
    Adcs.adcsManager.Dipole
    Adcs.adcsManager.Throttles
    Adcs.adcsManager.CycleTimeUs
  }
