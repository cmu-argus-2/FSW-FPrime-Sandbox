// ======================================================================
// \title  EpsMinTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef EPSMIN_EPSMINTOPOLOGYDEFS_HPP
#define EPSMIN_EPSMINTOPOLOGYDEFS_HPP

// SubtopologyTopologyDefs includes. Base comes first, in a block of its
// own: its defs explain why F Prime headers must precede the Zephyr ones,
// which the others bring in.
#include "project/Subtopologies/Base/SubtopologyTopologyDefs.hpp"

#include "project/Subtopologies/Board/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Eps/SubtopologyTopologyDefs.hpp"

// Telemetry packet list passed to Base.tlmSend in its configComponents phase
#include "project/Deployments/EpsMin/Top/EpsMin_EpsMinPacketsTlmPacketsAc.hpp"

// Definitions are placed within the same namespace as the FPP module that contains the topology.
namespace EpsMin {

//! This deployment's packet set, under the name Base.tlmSend's phase code uses
namespace TlmPackets = EpsMin_EpsMinPacketsTlmPackets;

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires a type named `EpsMin::TopologyState`. None of the instances need any state.
 */
struct TopologyState {};

}  // namespace EpsMin

#endif
