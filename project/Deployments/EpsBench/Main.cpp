// ======================================================================
// \title Main.cpp
// \brief Entry point for the Zephyr F' application
// ======================================================================

#include <Os/Os.hpp>
#include <project/Deployments/EpsBench/Top/EpsBenchTopology.hpp>

int main() {
    Os::init();

    // Board-specific I/O is declared in the Zephyr device tree, so the
    // deployment has no host command-line arguments.
    EpsBench::TopologyState inputs{};
    EpsBench::setupTopology(inputs);
    EpsBench::startRateGroups();
    return 0;
}
