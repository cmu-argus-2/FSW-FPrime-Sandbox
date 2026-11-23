# Telemetry packets for the Ground subtopology. Included by the packet set
# of every deployment that imports Ground. Packet ids 400-499.

  packet Commanding id 400 group 1 {
    Ground.cmdDisp.CommandsDispatched
    Ground.cmdDisp.CommandErrors
    Ground.cmdDisp.CommandsDropped
  }
