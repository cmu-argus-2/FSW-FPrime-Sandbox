// ======================================================================
// \title Main.cpp
// \brief Entry point for the Zephyr F' application
// ======================================================================

#include <Os/Os.hpp>
#include <project/Deployments/AdcsBench/Top/AdcsBenchTopology.hpp>

int main() {
    Os::init();

    // Board-specific I/O is declared in the Zephyr device tree, so the
    // deployment has no host command-line arguments.
    AdcsBench::TopologyState inputs{};
    AdcsBench::setupTopology(inputs);
    AdcsBench::startRateGroups();
    return 0;
}
