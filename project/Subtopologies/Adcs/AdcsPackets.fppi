# Telemetry packets for the Adcs subtopology. Included by the packet set of
# every deployment that imports Adcs. Packet ids 300-399.

  # All three sensors are sampled together, so one packet carries them:
  # three ImuVector channels and the boot-time power modes, 37 bytes of data.
  # PmuStatus is written once, at boot, and rides along in every packet.
  packet Imu id 300 group 1 {
    Adcs.imu.Gyro
    Adcs.imu.Accel
    Adcs.imu.Mag
    Adcs.imu.PmuStatus
  }
