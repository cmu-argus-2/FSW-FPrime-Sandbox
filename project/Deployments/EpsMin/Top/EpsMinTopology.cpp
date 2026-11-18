// ======================================================================
// \title  EpsMinTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
#include <project/Deployments/EpsMin/Top/EpsMinTopologyAc.hpp>

namespace EpsMin {

void setupTopology(const TopologyState& state) {
    initComponents(state);
    setBaseIds();
    connectComponents();
    // No regCommands(): there is no command dispatcher, and a component's
    // generated regCommands() asserts that its registration port is
    // connected.
    configComponents(state);
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
    stopTasks(state);
    freeThreads(state);
    tearDownComponents(state);
    deinitComponents(state);
}

}  // namespace EpsMin
