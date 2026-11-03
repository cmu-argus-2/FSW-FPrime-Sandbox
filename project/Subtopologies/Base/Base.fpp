module Base {

  constant BASE_ID = 0x10000000

  # ----------------------------------------------------------------------
  # Defaults
  # ----------------------------------------------------------------------

  module Default {
    constant QUEUE_SIZE = 10
    @ Must match CONFIG_DYNAMIC_THREAD_STACK_SIZE in prj.conf.
    constant STACK_SIZE = 8192
  }

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup_1Hz
    rateGroup_0_5Hz
    rateGroup_0_25Hz
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  # 1Hz rate group (divisor 1 of 1Hz base clock)
  instance rateGroup_1Hz: Svc.ActiveRateGroup base id BASE_ID + 0x01000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 43 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    // Context tokens for rate group members (unused, set to zero)
    Svc::ActiveRateGroup::ContextArray context(0);
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Base::rateGroup_1Hz.configure(ConfigObjects::Base_rateGroup_1Hz::context);
    """
  }

  # 0.5Hz rate group (divisor 2 of 1Hz base clock)
  instance rateGroup_0_5Hz: Svc.ActiveRateGroup base id BASE_ID + 0x02000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 42 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Svc::ActiveRateGroup::ContextArray context(0);
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Base::rateGroup_0_5Hz.configure(ConfigObjects::Base_rateGroup_0_5Hz::context);
    """
  }

  # 0.25Hz rate group (divisor 4 of 1Hz base clock)
  instance rateGroup_0_25Hz: Svc.ActiveRateGroup base id BASE_ID + 0x03000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 41 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Svc::ActiveRateGroup::ContextArray context(0);
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Base::rateGroup_0_25Hz.configure(ConfigObjects::Base_rateGroup_0_25Hz::context);
    """
  }

  instance cmdSeq: Svc.CmdSequencer base id BASE_ID + 0x04000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 40 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Fw::MallocAllocator allocator;
    """
    phase Fpp.ToCpp.Phases.configComponents """
    // Command sequencer needs to allocate memory to hold contents of command sequences
    Base::cmdSeq.allocateBuffer(0, ConfigObjects::Base_cmdSeq::allocator, 5 * 1024);
    """
    phase Fpp.ToCpp.Phases.tearDownComponents """
    Base::cmdSeq.deallocateBuffer(ConfigObjects::Base_cmdSeq::allocator);
    """
  }

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance chronoTime: Zephyr.ZephyrTime base id BASE_ID + 0x10000

  instance rateGroupDriver: Svc.RateGroupDriver base id BASE_ID + 0x11000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    // Rate group timing: base clock interval (timer) and divisors are coupled
    // to rate group names. Divisors: 1Hz, 0.5Hz, 0.25Hz
    Svc::RateGroupDriver::DividerSet divisors{{{1, 0}, {2, 0}, {4, 0}}};
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Base::rateGroupDriver.configure(ConfigObjects::Base_rateGroupDriver::divisors);
    """
  }

  instance systemResources: Svc.SystemResources base id BASE_ID + 0x12000

  instance timer: Zephyr.ZephyrRateDriver base id BASE_ID + 0x13000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    // 1000 ms base clock, divided down by rateGroupDriver
    Base::timer.configure(1000);
    """
  }

  instance comDriver: Zephyr.ZephyrUartDriver base id BASE_ID + 0x14000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Base::comDriver.configure(DEVICE_DT_GET(DT_CHOSEN(zephyr_console)), 115200);
    """
  }

  # ----------------------------------------------------------------------
  # Subtopology
  # ----------------------------------------------------------------------

  @ Command and telemetry plumbing, rate groups and the UART ground link.
  @ Every deployment imports this. Rate group member ports used here are
  @ listed in the RateGroups connections; deployments take the next free
  @ ones for their subsystems.
  topology Subtopology {

    import CdhCore.Subtopology
    import ComCcsds.Subtopology

    # Parameter database only. The rest of the FileHandling subtopology
    # (fileUplink, fileDownlink, fileManager) is unused. Its file name is
    # supplied by each deployment's configureTopology().
    instance FileHandling.prmDb
    instance chronoTime
    instance rateGroup_1Hz
    instance rateGroup_0_5Hz
    instance rateGroup_0_25Hz
    instance rateGroupDriver
    instance systemResources
    instance timer
    instance comDriver
    instance cmdSeq

    connections ComCcsds_CdhCore {
      # Core events and telemetry to communication queue
      CdhCore.events.PktSend -> ComCcsds.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      # PktSend[0] is the REALTIME section. PktSend[1] (RECORDED) stays
      # unconnected until there is storage to record to, and the packetizer
      # skips sections whose port is unconnected.
      CdhCore.tlmSend.PktSend[0] -> ComCcsds.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router to Command Dispatcher
      ComCcsds.fprimeRouter.commandOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> ComCcsds.fprimeRouter.cmdResponseIn
    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate      -> ComCcsds.commsBufferManager.bufferGetCallee
      comDriver.deallocate    -> ComCcsds.commsBufferManager.bufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                     -> ComCcsds.comStub.drvReceiveIn
      ComCcsds.comStub.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsds.comStub.drvSendOut      -> comDriver.$send
      comDriver.ready         -> ComCcsds.comStub.drvConnected
    }

    connections RateGroups {
      # timer to drive rate group
      timer.CycleOut -> rateGroupDriver.CycleIn

      # 1Hz rate group: members 0-5 used here
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_1Hz] -> rateGroup_1Hz.CycleIn
      rateGroup_1Hz.RateGroupMemberOut[0] -> CdhCore.tlmSend.Run
      rateGroup_1Hz.RateGroupMemberOut[1] -> systemResources.run
      rateGroup_1Hz.RateGroupMemberOut[2] -> ComCcsds.comQueue.run
      rateGroup_1Hz.RateGroupMemberOut[3] -> ComCcsds.aggregator.timeout
      rateGroup_1Hz.RateGroupMemberOut[4] -> CdhCore.cmdDisp.run
      rateGroup_1Hz.RateGroupMemberOut[5] -> comDriver.schedIn

      # 0.5Hz rate group: member 0 used here
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_0_5Hz] -> rateGroup_0_5Hz.CycleIn
      rateGroup_0_5Hz.RateGroupMemberOut[0] -> cmdSeq.schedIn

      # 0.25Hz rate group: members 0-1 used here
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_0_25Hz] -> rateGroup_0_25Hz.CycleIn
      rateGroup_0_25Hz.RateGroupMemberOut[0] -> CdhCore.$health.Run
      rateGroup_0_25Hz.RateGroupMemberOut[1] -> ComCcsds.commsBufferManager.schedIn
    }

    connections CdhCore_cmdSeq {
      # Command Sequencer
      cmdSeq.comCmdOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn
    }

  }

}
