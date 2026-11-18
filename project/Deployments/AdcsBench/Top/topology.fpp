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
    import Board.Subtopology
    import Adcs.Subtopology

  # ----------------------------------------------------------------------
  # Pattern graph specifiers
  # ----------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp
    event connections instance CdhCore.events
    telemetry connections instance CdhCore.tlmSend
    text event connections instance CdhCore.textLogger
    health connections instance CdhCore.$health
    param connections instance FileHandling.prmDb
    time connections instance Base.chronoTime

  # ----------------------------------------------------------------------
  # Telemetry packets for CdhCore.tlmSend, a TlmPacketizer
  # (see project/config/CdhCoreTlmConfig.fpp)
  # ----------------------------------------------------------------------

    include "AdcsBenchPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections Buses {
      # Light sensors -> I2C bus drivers. Output ports must be connected:
      # invoking an unconnected one asserts at runtime.
      #
      # i2c1: the +X/+Y faces and the +Z sun sensor.
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

      # i2c0: the -X/-Y/-Z faces.
      Adcs.lightSensorXm.busWriteRead -> Board.i2c0.writeRead
      Adcs.lightSensorXm.busWrite -> Board.i2c0.write
      Adcs.lightSensorYm.busWriteRead -> Board.i2c0.writeRead
      Adcs.lightSensorYm.busWrite -> Board.i2c0.write
      Adcs.lightSensorZm.busWriteRead -> Board.i2c0.writeRead
      Adcs.lightSensorZm.busWrite -> Board.i2c0.write
    }

    connections Bench {
      # Rate group drives sampling, standing in for the ADCS task until that
      # exists: the first tick boots each sensor, every tick after reads it.
      # The 0.5 Hz group is used because it has exactly the nine free member
      # ports needed (Base uses member 0); the 1 Hz group has four. A read
      # normally takes three short transactions, but blocks for up to 1.1 s
      # if a part never reports a finished conversion. The ADCS task PR
      # replaces these lines with the task.
      Base.rateGroup_0_5Hz.RateGroupMemberOut[1] -> Adcs.lightSensorXp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[2] -> Adcs.lightSensorXm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[3] -> Adcs.lightSensorYp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[4] -> Adcs.lightSensorYm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[5] -> Adcs.lightSensorZpXp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[6] -> Adcs.lightSensorZpYm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[7] -> Adcs.lightSensorZpXm.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[8] -> Adcs.lightSensorZpYp.run
      Base.rateGroup_0_5Hz.RateGroupMemberOut[9] -> Adcs.lightSensorZm.run
    }

  }

}
