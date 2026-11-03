# =======================================================================
# CdhCore telemetry override for the RP2350 deployments
#
# Replaces TlmChan with TlmPacketizer. TlmChan reserves a full
# FW_TLM_BUFFER_MAX_SIZE (~500 B) slot per channel, twice over, so it costs
# roughly 1.2 KB of static RAM per channel whatever the channel's type.
# TlmPacketizer stores channels packed into predefined packets, so a 4-byte
# channel costs 4 bytes of packet buffer plus a small index entry.
#
# Each subtopology lists its channels in a <Name>Packets.fppi next to its
# .fpp file, and each deployment's packet set includes the ones for the
# subtopologies it imports. Every channel in the topology must appear in a
# packet or in its omit list, or FPP rejects the topology.
#
# This file is shared by every deployment, so the phase code below names the
# packet list through TlmPackets, an alias each deployment's
# <Name>TopologyDefs.hpp points at its own packet set.
# =======================================================================

module CdhCore{

    instance tlmSend: Svc.TlmPacketizer base id CdhCoreConfig.BASE_ID + 0x06000 \
        queue size CdhCoreConfig.QueueSizes.tlmSend \
        stack size CdhCoreConfig.StackSizes.tlmSend \
        priority CdhCoreConfig.Priorities.tlmSend \
        cpu CdhCoreConfig.CpuAffinities.tlmSend \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        CdhCore::tlmSend.setPacketList(
            TlmPackets::packetList,
            TlmPackets::omittedChannels,
            1
        );
        """
    }
}
