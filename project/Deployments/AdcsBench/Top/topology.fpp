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
      #
      # IMU, i2c0.
      Adcs.imu.busWriteRead -> Board.i2c0.writeRead
      Adcs.imu.busWrite -> Board.i2c0.write

      # Light sensors, i2c1: the +X/+Y faces and the +Z sun sensor.
      Adcs.lightSensorXp.busWriteRead -> Board.i2c1.writeRead
      Adcs.lightSensorXp.busWrite -> Board.i2c1.write
      Adcs.lightSensorYp.busWriteRead -> Board.i2c1.writeRead
      Adcs.lightSensorYp.busWrite -> Board.i2c1.write
      Adcs.lightSensorZpXp.busWriteRead -> Board.i2c1.writeRead
      Adcs.lightSensorZpXp.busWrite -> Board.i2c1.write
      Adcs.lightSensorZpYm.busWriteRead -> Board.i2c1.writeRead
      Adcs.lightSensorZpYm.busWrite -> Board.i2c1.write
      Adcs.lightSensorZpXm.busWriteRead -> Board.i2c1.writeRead
      Adcs.lightSensorZpXm.busWrite -> Board.i2c1.write
      Adcs.lightSensorZpYp.busWriteRead -> Board.i2c1.writeRead
      Adcs.lightSensorZpYp.busWrite -> Board.i2c1.write

      # Light sensors, i2c0: the -X/-Y/-Z faces.
      Adcs.lightSensorXm.busWriteRead -> Board.i2c0.writeRead
      Adcs.lightSensorXm.busWrite -> Board.i2c0.write
      Adcs.lightSensorYm.busWriteRead -> Board.i2c0.writeRead
      Adcs.lightSensorYm.busWrite -> Board.i2c0.write
      Adcs.lightSensorZm.busWriteRead -> Board.i2c0.writeRead
      Adcs.lightSensorZm.busWrite -> Board.i2c0.write
    }

    connections Bench {
      # Rate group members, from the first free one (Base uses 1Hz 0-3 and
      # Ground 1Hz 4-5; every 0.5Hz member is free).
      #
      # ADCS task, 1 Hz. It boots the IMU and reads it itself.
      Base.rateGroup_1Hz.RateGroupMemberOut[6] -> Adcs.adcsManager.run

      # The task does not use the light sensors yet, so until it does they are
      # still driven straight from a rate group, for bring-up. They run at
      # 0.5 Hz because the nine need nine member ports: the first tick boots each
      # sensor, every tick after reads it. A read normally takes three short
      # transactions, but blocks for up to 1.1 s if a part never reports a
      # finished conversion.
      Base.rateGroup_0_5Hz.RateGroupMemberOut[0] -> Adcs.lightSensorXp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[1] -> Adcs.lightSensorXm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[2] -> Adcs.lightSensorYp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[3] -> Adcs.lightSensorYm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[4] -> Adcs.lightSensorZpXp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[5] -> Adcs.lightSensorZpYm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[6] -> Adcs.lightSensorZpXm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[7] -> Adcs.lightSensorZpYp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[8] -> Adcs.lightSensorZm.run
    }

  }

}
