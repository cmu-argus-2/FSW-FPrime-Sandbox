module Eps {

  constant BASE_ID = 0x10200000

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  @ The EPS task. Its drivers below are passive, so their I2C and GPIO
  @ transactions run on this thread. Priority sits below the rate groups
  @ (41-43) and the command dispatcher (35), above the comms queues (29-30).
  instance epsManager: project.EpsManager base id BASE_ID + 0x05000 \
    queue size Base.Default.QUEUE_SIZE \
    stack size Base.Default.STACK_SIZE \
    priority 31

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance batteryHeaters: project.BatteryHeaters base id BASE_ID + 0x01000

  # Heater GPIOs. Only the heaters use these pins, so unlike the I2C buses
  # they belong to EPS rather than Board. The board's GPIO hogs hold them low
  # from reset; each is driven low again as soon as it is opened.

  @ HEAT_EN, GPIO6
  instance heatEnGpio: Zephyr.ZephyrGpioDriver base id BASE_ID + 0x02000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    const struct gpio_dt_spec pin = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), heat_en_gpios);
    """
    phase Fpp.ToCpp.Phases.configComponents """
    FW_ASSERT(Eps::heatEnGpio.open(ConfigObjects::Eps_heatEnGpio::pin, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT) == Os::File::Status::OP_OK);
    gpio_pin_set_dt(&ConfigObjects::Eps_heatEnGpio::pin, 0);
    """
  }

  @ HEAT0_ON, GPIO4
  instance heat0Gpio: Zephyr.ZephyrGpioDriver base id BASE_ID + 0x03000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    const struct gpio_dt_spec pin = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), heat0_on_gpios);
    """
    phase Fpp.ToCpp.Phases.configComponents """
    FW_ASSERT(Eps::heat0Gpio.open(ConfigObjects::Eps_heat0Gpio::pin, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT) == Os::File::Status::OP_OK);
    gpio_pin_set_dt(&ConfigObjects::Eps_heat0Gpio::pin, 0);
    """
  }

  @ HEAT1_ON, GPIO5
  instance heat1Gpio: Zephyr.ZephyrGpioDriver base id BASE_ID + 0x04000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    const struct gpio_dt_spec pin = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), heat1_on_gpios);
    """
    phase Fpp.ToCpp.Phases.configComponents """
    FW_ASSERT(Eps::heat1Gpio.open(ConfigObjects::Eps_heat1Gpio::pin, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT) == Os::File::Status::OP_OK);
    gpio_pin_set_dt(&ConfigObjects::Eps_heat1Gpio::pin, 0);
    """
  }

  # Power monitors, all on I2C1 with a 10 mOhm sense resistor and the 14:1
  # divider (26.35 V full scale), per the EPS spec. configure() sends the
  # spec's 0x05 command. A device that fails to configure logs
  # ConfigurationError or I2cError and then reads as absent: its read port
  # returns I2C_OPEN_ERR and EPS leaves its fields at 0.
  #
  # configure() writes to the bus, so it relies on Board.i2c1 being opened
  # first. Board's phase code runs before Eps's (Board's base IDs and its
  # import come first in every deployment).

  @ Mainboard power monitor, I2C1 0x40
  instance pmMainboard: project.ADM1176 base id BASE_ID + 0x06000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    (void) Eps::pmMainboard.configure(0x40, 10, false);
    """
  }

  @ GPS (peripheral) power monitor, I2C1 0x41
  instance pmGps: project.ADM1176 base id BASE_ID + 0x07000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    (void) Eps::pmGps.configure(0x41, 10, false);
    """
  }

  @ Radio power monitor, I2C1 0x42
  instance pmRadio: project.ADM1176 base id BASE_ID + 0x08000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    (void) Eps::pmRadio.configure(0x42, 10, false);
    """
  }

  @ Jetson power monitor, I2C1 0x46
  instance pmJetson: project.ADM1176 base id BASE_ID + 0x09000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    (void) Eps::pmJetson.configure(0x46, 10, false);
    """
  }

  @ Battery fuel gauge, I2C1 0x36 (registers) and 0x0B (shadow RAM). Those
  @ are the driver's defaults, so it needs no configure() call.
  instance fuelGauge: project.MAX17205 base id BASE_ID + 0x0A000

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Electrical power subsystem: the EPS task and the drivers it calls.
  @ Connections to shared buses (Board) and to the rate groups (Base) are
  @ made by the deployment.
  topology Subtopology {

    instance epsManager
    instance batteryHeaters
    instance heatEnGpio
    instance heat0Gpio
    instance heat1Gpio
    instance pmMainboard
    instance pmGps
    instance pmRadio
    instance pmJetson
    instance fuelGauge

    connections Heaters {
      # HEAT_EN, GPIO6
      batteryHeaters.enableWrite  -> heatEnGpio.gpioWrite
      batteryHeaters.enableRead   -> heatEnGpio.gpioRead

      # HEAT0_ON, GPIO4
      batteryHeaters.heater0Write -> heat0Gpio.gpioWrite
      batteryHeaters.heater0Read  -> heat0Gpio.gpioRead

      # HEAT1_ON, GPIO5
      batteryHeaters.heater1Write -> heat1Gpio.gpioWrite
      batteryHeaters.heater1Read  -> heat1Gpio.gpioRead
    }

    connections Task {
      # Power monitors, in the spec's order: mainboard, peripheral (GPS),
      # radio, Jetson
      epsManager.powerMonitors[0] -> pmMainboard.read
      epsManager.powerMonitors[1] -> pmGps.read
      epsManager.powerMonitors[2] -> pmRadio.read
      epsManager.powerMonitors[3] -> pmJetson.read

      epsManager.fuelGauge -> fuelGauge.read

      epsManager.heaterSet -> batteryHeaters.setHeater
      epsManager.heaterGet -> batteryHeaters.getHeater
    }

  }

}