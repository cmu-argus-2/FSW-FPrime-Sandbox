module Adcs {

  constant BASE_ID = 0x10300000

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  # OPT4003 light sensors, one per face plus the four angled sensors of the
  # +Z sun sensor. Passive: their I2C transactions run on whichever thread
  # calls them. The bus each is wired to is set by the deployment, and its
  # address here; the pair identifies the sensor, since the same address is
  # reused on both buses.

  @ +X face light sensor, i2c1 0x44
  instance lightSensorXp: project.OPT4003 base id BASE_ID + 0x01000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorXp.configure(0x44);
    """
  }

  @ -X face light sensor, i2c0 0x44
  instance lightSensorXm: project.OPT4003 base id BASE_ID + 0x02000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorXm.configure(0x44);
    """
  }

  @ +Y face light sensor, i2c1 0x45
  instance lightSensorYp: project.OPT4003 base id BASE_ID + 0x03000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorYp.configure(0x45);
    """
  }

  @ -Y face light sensor, i2c0 0x45
  instance lightSensorYm: project.OPT4003 base id BASE_ID + 0x04000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorYm.configure(0x45);
    """
  }

  @ +Z sun sensor, +X-facing element, i2c1 0x54
  instance lightSensorZpXp: project.OPT4003 base id BASE_ID + 0x05000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorZpXp.configure(0x54);
    """
  }

  @ +Z sun sensor, -Y-facing element, i2c1 0x55
  instance lightSensorZpYm: project.OPT4003 base id BASE_ID + 0x06000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorZpYm.configure(0x55);
    """
  }

  @ +Z sun sensor, -X-facing element, i2c1 0x56
  instance lightSensorZpXm: project.OPT4003 base id BASE_ID + 0x07000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorZpXm.configure(0x56);
    """
  }

  @ +Z sun sensor, +Y-facing element, i2c1 0x57
  instance lightSensorZpYp: project.OPT4003 base id BASE_ID + 0x08000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorZpYp.configure(0x57);
    """
  }

  @ -Z face light sensor, i2c0 0x46
  instance lightSensorZm: project.OPT4003 base id BASE_ID + 0x09000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Adcs::lightSensorZm.configure(0x46);
    """
  }

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Attitude determination and control: its sensors and, once it exists,
  @ the ADCS task that calls them. Connections to shared buses (Board) and
  @ to the rate groups (Base) are made by the deployment.
  topology Subtopology {

    instance lightSensorXp
    instance lightSensorXm
    instance lightSensorYp
    instance lightSensorYm
    instance lightSensorZpXp
    instance lightSensorZpYm
    instance lightSensorZpXm
    instance lightSensorZpYp
    instance lightSensorZm

  }

}
