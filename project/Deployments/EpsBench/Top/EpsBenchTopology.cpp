// ======================================================================
// \title  EpsBenchTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <project/Deployments/EpsBench/Top/EpsBenchTopologyAc.hpp>

// Public functions for use in main program are namespaced with deployment module EpsBench
// This is also the namespace where the topology components are instantiated by FPP.
namespace EpsBench {

/**
 * \brief configure/setup components in project-specific way
 *
 * Instances configure themselves through phase code in their subtopology's .fpp file, which the autocoder runs in
 * configComponents(). Only instances that belong to another project's subtopology are configured here.
 */
void configureTopology() {
    // PrmDb file name must be supplied by the using topology
    FileHandling::prmDb.configure("PrmDb.dat");
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
    // Autocoded parameter read from file. Function provided by autocoder.
    readParameters();
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);
}

void startRateGroups() {
    Base::timer.start();
    while (true) {
        Base::timer.cycle();
    }
}

void stopRateGroups() {
    Base::timer.stop();
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);

    tearDownComponents(state);
    deinitComponents(state);
}
};  // namespace EpsBench
