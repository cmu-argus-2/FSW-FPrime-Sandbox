// ======================================================================
// \title  EpsBenchTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef EPSBENCH_EPSBENCHTOPOLOGYDEFS_HPP
#define EPSBENCH_EPSBENCHTOPOLOGYDEFS_HPP

// SubtopologyTopologyDefs includes. Base and Ground come first, in a block
// of their own: Base's explains why F Prime headers must precede the Zephyr
// ones, which the others bring in.
#include "project/Subtopologies/Base/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Ground/SubtopologyTopologyDefs.hpp"

#include "project/Subtopologies/Board/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Eps/SubtopologyTopologyDefs.hpp"

// Telemetry packet list passed to Base.tlmSend in its configComponents phase
#include "project/Deployments/EpsBench/Top/EpsBench_EpsBenchPacketsTlmPacketsAc.hpp"

// Definitions are placed within the same namespace as the FPP module that contains the topology.
namespace EpsBench {

//! This deployment's packet set, under the name Base.tlmSend's phase code uses
namespace TlmPackets = EpsBench_EpsBenchPacketsTlmPackets;

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires a type named `EpsBench::TopologyState`. None of the instances need any state.
 */
struct TopologyState {};

}  // namespace EpsBench

#endif
