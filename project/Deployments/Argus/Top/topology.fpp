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

  }

}
