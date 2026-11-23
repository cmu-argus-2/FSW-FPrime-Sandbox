// ======================================================================
// \title  SubtopologyTopologyDefs.hpp
// \brief  definitions the Base subtopology's phase code needs, included by
//         every deployment's TopologyDefs.hpp
// ======================================================================
#ifndef BASE_SUBTOPOLOGYTOPOLOGYDEFS_HPP
#define BASE_SUBTOPOLOGYTOPOLOGYDEFS_HPP

// F Prime headers before any Zephyr header: Zephyr defines an EMPTY macro
// that breaks the Os::Queue status enum if it is seen first. For the same
// reason, deployments include this file and Ground's before the other
// subtopologies' defs, and Ground's must not pull in Os::Queue itself.
#include <Fw/Types/MallocAllocator.hpp>
#include <cstring>

#include "project/Subtopologies/Base/FppConstantsAc.hpp"
#include "project/Subtopologies/Base/Ports_ComPacketQueueEnumAc.hpp"
#include "project/Subtopologies/Base/Ports_RateGroupsEnumAc.hpp"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#endif
