module Ground {

  constant BASE_ID = 0x10400000

  module BuffMgr {
    @ Uplink bytes held while a frame is reassembled
    constant frameAccumulatorSize = 2048
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  @ Command dispatcher: routes each uplinked command to the component that
  @ registered its opcode
  instance cmdDisp: Svc.CommandDispatcher base id BASE_ID + 0x01000 \
    queue size Base.Default.QUEUE_SIZE \
    stack size Base.Default.STACK_SIZE \
    priority 35

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  @ Reassembles F Prime frames from the UART byte stream
  instance frameAccumulator: Svc.FrameAccumulator base id BASE_ID + 0x02000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Fw::MallocAllocator allocator;
    Svc::FrameDetectors::FprimeFrameDetector frameDetector;
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Ground::frameAccumulator.configure(
        ConfigObjects::Ground_frameAccumulator::frameDetector,
        0,
        ConfigObjects::Ground_frameAccumulator::allocator,
        Ground::BuffMgr::frameAccumulatorSize
    );
    """
    phase Fpp.ToCpp.Phases.tearDownComponents """
    Ground::frameAccumulator.cleanup();
    """
  }

  @ Strips the F Prime frame header and CRC
  instance deframer: Svc.FprimeDeframer base id BASE_ID + 0x03000

  @ Sends command packets to the dispatcher. File packets are dropped:
  @ fileOut is not connected.
  instance router: Svc.FprimeRouter base id BASE_ID + 0x04000

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Commanding from the GDS: the uplink side of Base's ground link and the
  @ command dispatcher. Import it alongside Base.Subtopology, and add
  @   command connections instance Ground.cmdDisp
  @ to the deployment.
  @
  @ Health pings are deliberately left out. Svc.Health is software only: it
  @ pings every active component on a rate group and raises a WARNING_HI
  @ event, then a FATAL one, when a component's thread stops answering
  @ (e.g. blocked forever in a driver call). Its watchdog output is not
  @ wired to a hardware watchdog here. To add it back:
  @   - an instance of Svc.Health here, scheduled from a rate group
  @     (it used to run at 0.25 Hz), with setPingEntries() phase code;
  @   - a PingEntries.hpp entry (WARN/FATAL thresholds) for every active
  @     component, as project/Subtopologies/Eps/PingEntries.hpp has for EPS;
  @   - "health connections instance Ground.$health" in each deployment;
  @   - a fatal event handler (Zephyr.FatalHandler, which reboots) on
  @     Base.events.FatalAnnounce, since Health is what emits FATAL events.
  topology Subtopology {

    instance cmdDisp
    instance frameAccumulator
    instance deframer
    instance router

    # Base instances this subtopology connects to
    instance Base.rateGroup_1Hz
    instance Base.comDriver
    instance Base.comStub
    instance Base.commsBufferManager

    connections Uplink {
      # UART driver reads into buffers from Base's buffer manager
      Base.comDriver.allocate   -> Base.commsBufferManager.bufferGetCallee
      Base.comDriver.deallocate -> Base.commsBufferManager.bufferSendIn

      Base.comDriver.$recv -> Base.comStub.drvReceiveIn
      Base.comStub.drvReceiveReturnOut -> Base.comDriver.recvReturnIn

      Base.comStub.dataOut -> frameAccumulator.dataIn
      frameAccumulator.dataReturnOut -> Base.comStub.dataReturnIn
      frameAccumulator.bufferAllocate   -> Base.commsBufferManager.bufferGetCallee
      frameAccumulator.bufferDeallocate -> Base.commsBufferManager.bufferSendIn

      frameAccumulator.dataOut -> deframer.dataIn
      deframer.dataReturnOut -> frameAccumulator.dataReturnIn

      deframer.dataOut -> router.dataIn
      router.dataReturnOut -> deframer.dataReturnIn

      router.commandOut -> cmdDisp.seqCmdBuff
      cmdDisp.seqCmdStatus -> router.cmdResponseIn
    }

    connections RateGroups {
      # 1Hz members 4-5 (Base uses 0-3)
      Base.rateGroup_1Hz.RateGroupMemberOut[4] -> Base.comDriver.schedIn
      Base.rateGroup_1Hz.RateGroupMemberOut[5] -> cmdDisp.run
    }

  }

}
