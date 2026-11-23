// ======================================================================
// \title  AdcsBenchTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef ADCSBENCH_ADCSBENCHTOPOLOGYDEFS_HPP
#define ADCSBENCH_ADCSBENCHTOPOLOGYDEFS_HPP

// SubtopologyTopologyDefs includes. Base and Ground come first, in a block
// of their own: Base's explains why F Prime headers must precede the Zephyr
// ones, which the others bring in.
#include "project/Subtopologies/Base/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Ground/SubtopologyTopologyDefs.hpp"

#include "project/Subtopologies/Adcs/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Board/SubtopologyTopologyDefs.hpp"

// Telemetry packet list passed to Base.tlmSend in its configComponents phase
#include "project/Deployments/AdcsBench/Top/AdcsBench_AdcsBenchPacketsTlmPacketsAc.hpp"

// Definitions are placed within the same namespace as the FPP module that contains the topology.
namespace AdcsBench {

//! This deployment's packet set, under the name Base.tlmSend's phase code uses
namespace TlmPackets = AdcsBench_AdcsBenchPacketsTlmPackets;

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires a type named `AdcsBench::TopologyState`. None of the instances need any state.
 */
struct TopologyState {};

}  // namespace AdcsBench

#endif
