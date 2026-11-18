/**
 * \file
 * \brief TlmPacketizer limits for EpsMin, sized to exactly its packet set
 *
 * Replaces project/config/TlmPacketizerCfg.hpp for this deployment only (see
 * ../deployment.cmake). Each packet reserves a full FW_COM_BUFFER_MAX_SIZE
 * buffer, and each channel an offset for every possible packet, so spare
 * capacity here is RAM spent on nothing.
 *
 * Both limits are checked by assertions in setPacketList(), at boot. When a
 * channel or packet is added to Top/EpsMinPackets.fppi, raise them to match,
 * or the deployment stops at boot. scripts/ram_report.py compares them with
 * the built dictionary.
 */

#ifndef SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_
#define SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Svc {
//! Packets in Top/EpsMinPackets.fppi: EveryRun and FuelGaugeCycle.
static const FwChanIdType MAX_PACKETIZER_PACKETS = 2;

//! Every channel in the topology, packetized or omitted: 30 in EveryRun, 22
//! in FuelGaugeCycle and 2 omitted.
static const FwChanIdType MAX_PACKETIZER_CHANNELS = 54;

//! Channels reported as arriving outside every packet. FPP already rejects a
//! packet set that leaves a channel out, so one slot is enough.
static const FwChanIdType TLMPACKETIZER_MAX_MISSING_TLM_CHECK = 1;
}  // namespace Svc

#endif /* SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_ */
