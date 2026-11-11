module BenchTools {

  constant BASE_ID = 0x10F00000

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  # Bench and bring-up tools from project/Tools. Only bench deployments
  # import this subtopology, so none of them reach the flight image. Like
  # the drivers, each is connected to the shared buses by the deployment.

  @ I2C bus scanner. Send BenchTools.i2cScanner.SCAN from the GDS.
  instance i2cScanner: project.I2cScanner base id BASE_ID + 0x01000

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Tools for testing hardware and other components on the flatsat.
  @ Connections to shared buses (Board) are made by the deployment.
  topology Subtopology {

    instance i2cScanner

  }

}
