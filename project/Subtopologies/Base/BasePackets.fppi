# Telemetry packets for the Base subtopology, and for the F´ CdhCore and
# ComCcsds subtopologies it imports. Included by every deployment's packet
# set. Packet ids 1-99.

  packet CDH id 1 group 1 {
    CdhCore.cmdDisp.CommandsDispatched
    CdhCore.cmdDisp.CommandsDropped
    CdhCore.events.EventsDropped

    Base.cmdSeq.CS_LoadCommands
    Base.cmdSeq.CS_CancelCommands
    Base.cmdSeq.CS_CommandsExecuted
    Base.cmdSeq.CS_SequencesCompleted
    ComCcsds.comQueue.comQueueDepth
    ComCcsds.comQueue.buffQueueDepth
    ComCcsds.commsBufferManager.TotalBuffs
    ComCcsds.commsBufferManager.CurrBuffs
    ComCcsds.commsBufferManager.HiBuffs

    Base.rateGroup_1Hz.RgMaxTime
    Base.rateGroup_0_5Hz.RgMaxTime
    Base.rateGroup_0_25Hz.RgMaxTime
  }

  packet CDHErrors id 2 group 1 {
    CdhCore.cmdDisp.CommandErrors
    CdhCore.$health.PingLateWarnings

    Base.cmdSeq.CS_Errors
    ComCcsds.commsBufferManager.NoBuffs
    ComCcsds.commsBufferManager.EmptyBuffs

    Base.rateGroup_1Hz.RgCycleSlips
    Base.rateGroup_0_5Hz.RgCycleSlips
    Base.rateGroup_0_25Hz.RgCycleSlips
  }

  # Kept out of CDH because the string is 242 bytes and only changes when a
  # new sequence starts.
  packet Sequence id 3 group 2 {
    Base.cmdSeq.CS_CurrentSequence
  }

  packet SystemResources id 4 group 2 {
    Base.systemResources.MEMORY_TOTAL
    Base.systemResources.MEMORY_USED
    Base.systemResources.NON_VOLATILE_TOTAL
    Base.systemResources.NON_VOLATILE_FREE
    Base.systemResources.CPU
    Base.systemResources.CPU_00
    Base.systemResources.CPU_01
    Base.systemResources.CPU_02
    Base.systemResources.CPU_03
    Base.systemResources.CPU_04
    Base.systemResources.CPU_05
    Base.systemResources.CPU_06
    Base.systemResources.CPU_07
    Base.systemResources.CPU_08
    Base.systemResources.CPU_09
    Base.systemResources.CPU_10
    Base.systemResources.CPU_11
    Base.systemResources.CPU_12
    Base.systemResources.CPU_13
    Base.systemResources.CPU_14
    Base.systemResources.CPU_15
  }

  packet TlmPacketizer id 5 group 2 {
    CdhCore.tlmSend.GroupConfigs
    CdhCore.tlmSend.SectionEnabled
  }

  # Version strings serialize to 42 bytes each and custom versions to about
  # 87, so they are split to stay under 512 bytes per packet.
  packet Version id 20 group 3 {
    CdhCore.version.FrameworkVersion
    CdhCore.version.ProjectVersion
    CdhCore.version.LibraryVersion01
    CdhCore.version.LibraryVersion02
    CdhCore.version.LibraryVersion03
    CdhCore.version.LibraryVersion04
    CdhCore.version.LibraryVersion05
  }

  packet VersionLibrary id 21 group 3 {
    CdhCore.version.LibraryVersion06
    CdhCore.version.LibraryVersion07
    CdhCore.version.LibraryVersion08
    CdhCore.version.LibraryVersion09
    CdhCore.version.LibraryVersion10
  }

  packet VersionCustom1 id 22 group 3 {
    CdhCore.version.CustomVersion01
    CdhCore.version.CustomVersion02
    CdhCore.version.CustomVersion03
    CdhCore.version.CustomVersion04
    CdhCore.version.CustomVersion05
  }

  packet VersionCustom2 id 23 group 3 {
    CdhCore.version.CustomVersion06
    CdhCore.version.CustomVersion07
    CdhCore.version.CustomVersion08
    CdhCore.version.CustomVersion09
    CdhCore.version.CustomVersion10
  }
