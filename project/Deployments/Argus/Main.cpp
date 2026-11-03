// ======================================================================
// \title Main.cpp
// \brief Entry point for the Zephyr F' application
// ======================================================================

#include <Os/Os.hpp>
#include <project/Deployments/Argus/Top/ArgusTopology.hpp>

int main() {
    Os::init();

    // Board-specific I/O is declared in the Zephyr device tree, so the
    // deployment has no host command-line arguments.
    Argus::TopologyState inputs{};
    Argus::setupTopology(inputs);
    Argus::startRateGroups();
    return 0;
}
