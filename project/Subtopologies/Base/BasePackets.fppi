# Telemetry packets for the Base subtopology. Included by every
# deployment's packet set. Packet ids 1-99.

  packet Downlink id 1 group 1 {
    Base.rateGroup_1Hz.RgMaxTime
    Base.events.EventsDropped
    Base.comQueue.comQueueDepth
    Base.comQueue.buffQueueDepth
    Base.commsBufferManager.TotalBuffs
    Base.commsBufferManager.CurrBuffs
    Base.commsBufferManager.HiBuffs
  }

  packet DownlinkErrors id 2 group 1 {
    Base.rateGroup_1Hz.RgCycleSlips
    Base.commsBufferManager.NoBuffs
    Base.commsBufferManager.EmptyBuffs
  }

  packet TlmPacketizer id 5 group 2 {
    Base.tlmSend.GroupConfigs
    Base.tlmSend.SectionEnabled
  }
