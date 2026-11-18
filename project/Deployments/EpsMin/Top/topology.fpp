module EpsMin {

  @ The EPS task and its drivers with the least F Prime needed to run them
  @ and see their output: Base (one 1 Hz rate group, time, and a one-way
  @ downlink of events and telemetry) and nothing else. There is no uplink,
  @ so it cannot be commanded; use EpsBench for that.
  @
  @ This is the F Prime image for comparing EPS RAM and flash with the other
  @ ports, whose EPS-only builds have no ground interface beyond a one-way
  @ log. Its telemetry packetizer is sized to exactly this packet set
  @ (config/TlmPacketizerCfg.hpp, applied by deployment.cmake).
  deployment topology EpsMin {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import Base.Subtopology
    import Eps.Subtopology

    # The one bus EPS uses. Board.i2c0 (IMU and the -X/-Y/-Z faces) is
    # left out.
    instance Board.i2c1

  # ----------------------------------------------------------------------
  # Pattern graph specifiers. No command, parameter, health or text event
  # connections: those ports stay unconnected, and generated code skips an
  # unconnected event or telemetry port.
  # ----------------------------------------------------------------------

    event connections instance Base.events
    telemetry connections instance Base.tlmSend
    time connections instance Base.chronoTime

  # ----------------------------------------------------------------------
  # Telemetry packets for Base.tlmSend
  # ----------------------------------------------------------------------

    include "EpsMinPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections EpsWiring {
      # EPS task on the 1 Hz rate group (Base uses members 0-3), at the
      # same member as in EpsBench and Argus
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
