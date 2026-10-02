// ======================================================================
// \title  FirstDeploymentTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <project/Components/FirstDeployment/Top/FirstDeploymentTopologyAc.hpp>
// Note: Uncomment when using Svc:TlmPacketizer
// #include <project/Components/FirstDeployment/Top/FirstDeploymentPacketsAc.hpp>

// Necessary project-specified types
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <Fw/Logger/Logger.hpp>
#include <Fw/Types/MallocAllocator.hpp>

// Public functions for use in main program are namespaced with deployment module Components
// This is also the namespace where the topology components are instantiated by FPP.
namespace Components {

// Instantiate a malloc allocator for cmdSeq buffer allocation
Fw::MallocAllocator mallocator;

// PERIPH_PWR_EN (GPIO42): gates the 3.3 V rail carrying the I2C peripherals.
// Declared in the board devicetree under /zephyr,user.
static const struct gpio_dt_spec periphPwrEn =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), periph_pwr_en_gpios);

// How long the peripheral rail needs before its devices are addressable.
static constexpr int PERIPH_PWR_SETTLE_MS = 100;

//! Bring up the peripheral 3.3 V rail and wait for it to settle.
//!
//! This must happen before any I2C transaction. Devices on this rail latch
//! their I2C address at power-up, so talking to the bus early does not simply
//! fail -- it can find a device at an unexpected address, which presents as an
//! intermittent addressing fault rather than an obviously dead rail.
//!
//! \return true if the rail was enabled
static bool enablePeripheralPower() {
    if (!gpio_is_ready_dt(&periphPwrEn)) {
        Fw::Logger::log("[ERROR] PERIPH_PWR_EN GPIO not ready\n");
        return false;
    }
    const int rc = gpio_pin_configure_dt(&periphPwrEn, GPIO_OUTPUT_ACTIVE);
    if (rc != 0) {
        Fw::Logger::log("[ERROR] Failed to assert PERIPH_PWR_EN: %d\n", rc);
        return false;
    }
    k_msleep(PERIPH_PWR_SETTLE_MS);
    return true;
}

// Rate group timing: base clock interval and divisors are coupled to rate group names
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}, {4, 0}}};
// Divisors: 1Hz, 0.5Hz, 0.25Hz

// Context tokens for rate group members (unused, set to zero)
Svc::ActiveRateGroup::ContextArray rateGroup_1HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_5HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_25HzContext(0);

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 */
void configureTopology() {
    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);
    timer.configure(1000);

    // Rate groups require context arrays.
    rateGroup_1Hz.configure(rateGroup_1HzContext);
    rateGroup_0_5Hz.configure(rateGroup_0_5HzContext);
    rateGroup_0_25Hz.configure(rateGroup_0_25HzContext);

    // Command sequencer needs to allocate memory to hold contents of command sequences
    cmdSeq.allocateBuffer(0, mallocator, 5 * 1024);

    // PrmDb file name must be supplied by the using topology
    FileHandling::prmDb.configure("PrmDb.dat");

    // Power the peripheral rail before touching the bus it feeds. The order
    // of these two calls is a hardware requirement, not a preference.
    (void)enablePeripheralPower();

    // Hand the I2C driver its Zephyr device handle. This is the one place
    // where devicetree (which knows about pins) meets F Prime (which does
    // not): everything above this line addresses the fuel gauge purely by
    // I2C address and register number.
    //
    // i2c1 is SDA1/SCL1, GPIO46/47, per the board devicetree.
    const Drv::I2cStatus i2cStatus = i2cDriver.open(DEVICE_DT_GET(DT_NODELABEL(i2c1)));
    if (i2cStatus != Drv::I2cStatus::I2C_OK) {
        // Nothing is up yet to carry an event, so say it on the console. A
        // silent failure here would look exactly like a broken driver later.
        Fw::Logger::log("[ERROR] Failed to open i2c1 for the fuel gauge\n");
    }
}

void setupTopology(const TopologyState& state) {
    // Autocoded initialization. Function provided by autocoder.
    initComponents(state);
    // Autocoded id setup. Function provided by autocoder.
    setBaseIds();
    // Autocoded connection wiring. Function provided by autocoder.
    connectComponents();
    // Autocoded command registration. Function provided by autocoder.
    regCommands();
    // Autocoded configuration. Function provided by autocoder.
    configComponents(state);
    // Project-specific component configuration. Function provided above. May be inlined, if desired.
    configureTopology();
    comDriver.configure(DEVICE_DT_GET(DT_CHOSEN(zephyr_console)), 115200);
    // Autocoded parameter read from file. Function provided by autocoder.
    readParameters();
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);
}

void startRateGroups() {
    timer.start();
    while (true) {
        timer.cycle();
    }
}

void stopRateGroups() {
    timer.stop();
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);

    // Resource deallocation
    cmdSeq.deallocateBuffer(mallocator);

    tearDownComponents(state);
    deinitComponents(state);
}
};  // namespace Components
