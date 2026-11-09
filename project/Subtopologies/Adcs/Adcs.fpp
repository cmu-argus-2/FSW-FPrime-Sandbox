module Adcs {

  constant BASE_ID = 0x10300000

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  # The ADCS sensors and actuators. Passive: their bus transactions run on
  # whichever thread calls them. The bus each is wired to is set by the
  # deployment, and its address here, in a configComponents phase.

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Attitude determination and control: its sensors and actuators and, once
  @ it exists, the ADCS task that calls them. Connections to shared buses
  @ (Board) and to the rate groups (Base) are made by the deployment.
  topology Subtopology {

  }

}
