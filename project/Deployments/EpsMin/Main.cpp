// ======================================================================
// \title Main.cpp
// \brief Entry point for the Zephyr F' application
// ======================================================================

#include <Os/Os.hpp>
#include <project/Deployments/EpsMin/Top/EpsMinTopology.hpp>

int main() {
    Os::init();

    EpsMin::TopologyState inputs{};
    EpsMin::setupTopology(inputs);
    EpsMin::startRateGroups();
    return 0;
}
