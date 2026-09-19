// ======================================================================
// \title Main.cpp
// \brief Entry point for the Zephyr F' application
// ======================================================================

#include <project/Components/FirstDeployment/Top/FirstDeploymentTopology.hpp>
#include <Os/Os.hpp>

int main() {
    Os::init();

    // Board-specific I/O is declared in the Zephyr device tree, so the
    // deployment has no host command-line arguments.
    Components::TopologyState inputs{};
    Components::setupTopology(inputs);
    Components::startRateGroups();
    return 0;
}
