module EpsBench {

  @ The EPS subsystem with only the plumbing it needs: no other subsystem
  @ is present, so it can be flashed to the flatsat to bring up an EPS
  @ driver, and its timing and memory use measured without ADCS or
  @ payload work in the way.
  deployment topology EpsBench {

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

    include "EpsBenchPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections Bench {
      # Until the EPS task exists, a driver under test can be driven
      # straight from a rate group here, starting at the first free member
      # port (Base uses 1Hz 0-5, 0.5Hz 0, 0.25Hz 0-1), e.g.
      #   Base.rateGroup_1Hz.RateGroupMemberOut[6] -> Eps.fuelGauge.run
      # and connected to its bus, e.g.
      #   Eps.fuelGauge.busWriteRead -> Board.i2c1.writeRead
      # The EPS task PR replaces the rate group lines with the task.
    }

  }

}
