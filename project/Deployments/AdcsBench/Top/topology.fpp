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
    import BenchTools.Subtopology

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

    connections BenchTools {
      # I2C bus scanner: one read port per bus (see project/Tools/I2cScanner).
      BenchTools.i2cScanner.bus0Read -> Board.i2c0.read
      BenchTools.i2cScanner.bus1Read -> Board.i2c1.read
    }

    connections Bench {
      # Until the ADCS task exists, a driver under test can be driven
      # straight from a rate group here, starting at the first free member
      # port (Base uses 1Hz 0-5, 0.5Hz 0, 0.25Hz 0-1), e.g.
      #   Base.rateGroup_1Hz.RateGroupMemberOut[6] -> Adcs.imu.run
      # and connected to its bus, e.g.
      #   Adcs.imu.busWriteRead -> Board.i2c0.writeRead
      # The ADCS task PR replaces the rate group lines with the task.
    }

  }

}
