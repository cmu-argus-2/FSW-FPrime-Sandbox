module Argus {

  @ The flight image: every subsystem, as it will fly. Only task PRs change
  @ this file; drivers are brought up on a subsystem bench first.
  deployment topology Argus {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import Base.Subtopology
    import Board.Subtopology
    import Eps.Subtopology

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

    include "ArgusPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections EpsWiring {
      # EPS task on the 1 Hz rate group (Base uses members 0-5)
      Base.rateGroup_1Hz.RateGroupMemberOut[6] -> Eps.epsManager.run

      # Power monitors on I2C1
      Eps.pmMainboard.i2cWrite -> Board.i2c1.write
      Eps.pmMainboard.i2cRead  -> Board.i2c1.read
      Eps.pmGps.i2cWrite       -> Board.i2c1.write
      Eps.pmGps.i2cRead        -> Board.i2c1.read
      Eps.pmRadio.i2cWrite     -> Board.i2c1.write
      Eps.pmRadio.i2cRead      -> Board.i2c1.read
      Eps.pmJetson.i2cWrite    -> Board.i2c1.write
      Eps.pmJetson.i2cRead     -> Board.i2c1.read

      # Fuel gauge on I2C1
      Eps.fuelGauge.busWriteRead -> Board.i2c1.writeRead
      Eps.fuelGauge.busWrite     -> Board.i2c1.write
    }

  }

}