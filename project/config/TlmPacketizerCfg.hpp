/**
 * \file
 * \brief TlmPacketizer configuration override for the RP2350 deployments
 *
 * The upstream limits (50 packets, 200 channels) cost about 75 KB of
 * static RAM. Each packet reserves one FW_COM_BUFFER_MAX_SIZE buffer, and
 * each channel entry holds an 8-byte offset for every possible packet, so
 * RAM grows with MAX_PACKETIZER_PACKETS * MAX_PACKETIZER_CHANNELS.
 *
 * Both limits are checked by assertions in setPacketList(), which runs at
 * boot. Exceeding either one stops the deployment from starting rather
 * than failing the build, so raise them when the largest deployment's
 * packet set (Argus, project/Deployments/Argus/Top/ArgusPackets.fppi)
 * grows past them.
 */

#ifndef SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_
#define SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Svc {
//! Maximum number of packets that the packetizer can handle.
//! Must be >= the number of packets in the largest deployment (Argus: currently 10).
static const FwChanIdType MAX_PACKETIZER_PACKETS = 16;

//! Maximum number of channels that the packetizer can handle. Must be >= number of non-omitted channels
//! in the largest deployment (Argus: currently 71, counting the packetizer's own two).
static const FwChanIdType MAX_PACKETIZER_CHANNELS = 128;

//! Maximum number of missing channels to track and report
static const FwChanIdType TLMPACKETIZER_MAX_MISSING_TLM_CHECK = 25;

}  // namespace Svc

#endif /* SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_ */
