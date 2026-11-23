// ======================================================================
// \title  EpsMinTopology.hpp
// \brief header file containing the topology instantiation definitions
//
// ======================================================================
#ifndef EPSMIN_EPSMINTOPOLOGY_HPP
#define EPSMIN_EPSMINTOPOLOGY_HPP

#include <project/Deployments/EpsMin/Top/EpsMinTopologyDefs.hpp>

namespace EpsMin {

//! Initialize, wire, configure and start the topology
void setupTopology(const TopologyState& state);

//! Stop tasks and release resources
void teardownTopology(const TopologyState& state);

//! Run the 1 Hz timer on the calling thread. Does not return.
void startRateGroups();

//! Stop the timer
void stopRateGroups();

}  // namespace EpsMin
#endif
