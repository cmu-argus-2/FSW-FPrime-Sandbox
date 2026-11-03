module Eps {

  constant BASE_ID = 0x10200000

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

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Electrical power subsystem: its drivers and, once it exists, the EPS
  @ task that calls them. Connections to shared buses (Board) and to the
  @ rate groups (Base) are made by the deployment.
  topology Subtopology {

    instance batteryHeaters
    instance heatEnGpio
    instance heat0Gpio
    instance heat1Gpio

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

  }

}
