// ======================================================================
// \title  AdcsBenchTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef ADCSBENCH_ADCSBENCHTOPOLOGYDEFS_HPP
#define ADCSBENCH_ADCSBENCHTOPOLOGYDEFS_HPP

// Subtopology PingEntries includes
#include "Svc/Subtopologies/CdhCore/PingEntries.hpp"
#include "Svc/Subtopologies/ComCcsds/PingEntries.hpp"
#include "Svc/Subtopologies/FileHandling/PingEntries.hpp"
#include "project/Subtopologies/Base/PingEntries.hpp"

// SubtopologyTopologyDefs includes
#include "Svc/Subtopologies/CdhCore/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/ComCcsds/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/FileHandling/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Adcs/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Base/SubtopologyTopologyDefs.hpp"
#include "project/Subtopologies/Board/SubtopologyTopologyDefs.hpp"

// ComCcsds Enum Includes
#include "Svc/Subtopologies/ComCcsds/Ports_ComBufferQueueEnumAc.hpp"
#include "Svc/Subtopologies/ComCcsds/Ports_ComPacketQueueEnumAc.hpp"

// Telemetry packet list passed to CdhCore.tlmSend in its configComponents phase
#include "project/Deployments/AdcsBench/Top/AdcsBench_AdcsBenchPacketsTlmPacketsAc.hpp"

// Definitions are placed within the same namespace as the FPP module that contains the topology.
namespace AdcsBench {

//! This deployment's packet set, under the name project/config/CdhCoreTlmConfig.fpp uses
namespace TlmPackets = AdcsBench_AdcsBenchPacketsTlmPackets;

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires an object that carries state with the name `AdcsBench::TopologyState`. Only the type
 * definition is required by the autocoder and the contents of this object are otherwise opaque to the autocoder. The
 * contents are entirely up to the definition of the project. This deployment uses subtopologies.
 */
struct TopologyState {
    CdhCore::SubtopologyState cdhCore;            //!< Subtopology state for CdhCore
    ComCcsds::SubtopologyState comCcsds;          //!< Subtopology state for ComCcsds
    FileHandling::SubtopologyState fileHandling;  //!< Subtopology state for FileHandling
};

namespace PingEntries = ::PingEntries;
}  // namespace AdcsBench

#endif
