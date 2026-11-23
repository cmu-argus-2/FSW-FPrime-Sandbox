module AdcsBench {

  @ The ADCS subsystem with only the plumbing it needs: no other subsystem
  @ is present, so it can be flashed to the flatsat to bring up an ADCS
  @ driver, and its timing and memory use measured without EPS or
  @ payload work in the way.
  deployment topology AdcsBench {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import Base.Subtopology
    import Ground.Subtopology
    # 0.5 Hz rate group: driver bring-up samples the light sensors here
    import Base.SlowRateGroup
    import Board.Subtopology
    import Adcs.Subtopology

  # ----------------------------------------------------------------------
  # Pattern graph specifiers
  # ----------------------------------------------------------------------

    command connections instance Ground.cmdDisp
    event connections instance Base.events
    telemetry connections instance Base.tlmSend
    time connections instance Base.chronoTime

  # ----------------------------------------------------------------------
  # Telemetry packets for Base.tlmSend, a TlmPacketizer
  # ----------------------------------------------------------------------

    include "AdcsBenchPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections Buses {
      # Drivers -> I2C bus drivers. Output ports must be connected: invoking
      # an unconnected one asserts at runtime.
      Adcs.imu.busWriteRead -> Board.i2c0.writeRead
      Adcs.imu.busWrite -> Board.i2c0.write
    }

    connections Bench {
      # Until the ADCS task exists, each driver under test is driven straight
      # from a rate group here, from the first free member port (Base uses
      # 1Hz 0-3 and Ground 1Hz 4-5; every 0.5Hz member is free). The ADCS
      # task PR replaces these lines with the task.
      #
      # IMU: the first tick boots it, which takes about 120 ms; every tick
      # after reads all three sensors.
      Base.rateGroup_1Hz.RateGroupMemberOut[6] -> Adcs.imu.run
    }

  }

}
