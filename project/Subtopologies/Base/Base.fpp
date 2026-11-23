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
  # Downlink sizing
  # ----------------------------------------------------------------------

  module ComQueueDepths {
    constant events = 20
    constant tlm = 30
    @ Nothing is queued here (there is no file downlink), but ComQueue
    @ asserts that every configured entry has a depth greater than zero.
    constant file = 1
  }

  module BuffMgr {
    @ Big enough for one F Prime frame (a 512-byte com buffer plus header
    @ and CRC) and for one uplink read from the UART driver.
    constant commsBuffSize = 2048
    constant commsBuffCount = 5
    constant commsBuffMgrId = 200
  }

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup_1Hz
    rateGroup_0_5Hz
  }

  @ Downlink queue inputs, in priority order
  enum Ports_ComPacketQueue {
    EVENTS
    TELEMETRY
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

  # 0.5Hz rate group (divisor 2 of 1Hz base clock). Only in SlowRateGroup.
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

  @ Event logger: queues every component's events for downlink
  instance events: Svc.EventManager base id BASE_ID + 0x05000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 23

  @ Telemetry packetizer. Each deployment's <Name>TopologyDefs.hpp points
  @ TlmPackets at its own packet set (<Name>Packets.fppi).
  instance tlmSend: Svc.TlmPacketizer base id BASE_ID + 0x06000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 22 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Base::tlmSend.setPacketList(
        TlmPackets::packetList,
        TlmPackets::omittedChannels,
        1
    );
    """
  }

  @ Downlink queue: events ahead of telemetry, one frame at a time
  instance comQueue: Svc.ComQueue base id BASE_ID + 0x07000 \
    queue size 50 \
    stack size Default.STACK_SIZE \
    priority 29 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Fw::MallocAllocator allocator;
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Svc::ComQueue::QueueConfigurationTable table;
    table.entries[Base::Ports_ComPacketQueue::EVENTS].depth = Base::ComQueueDepths::events;
    table.entries[Base::Ports_ComPacketQueue::EVENTS].priority = 0;
    table.entries[Base::Ports_ComPacketQueue::TELEMETRY].depth = Base::ComQueueDepths::tlm;
    table.entries[Base::Ports_ComPacketQueue::TELEMETRY].priority = 1;
    // The one buffer (file) queue, which nothing feeds
    table.entries[Base::Ports_ComPacketQueue::NUM_CONSTANTS].depth = Base::ComQueueDepths::file;
    table.entries[Base::Ports_ComPacketQueue::NUM_CONSTANTS].priority = 2;
    Base::comQueue.configure(table, 0, ConfigObjects::Base_comQueue::allocator);
    """
    phase Fpp.ToCpp.Phases.tearDownComponents """
    Base::comQueue.cleanup();
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
    // to rate group names. Divisors: 1Hz, 0.5Hz. An output with nothing
    // connected (0.5Hz, outside SlowRateGroup) is skipped.
    Svc::RateGroupDriver::DividerSet divisors{{{1, 0}, {2, 0}}};
    """
    phase Fpp.ToCpp.Phases.configComponents """
    Base::rateGroupDriver.configure(ConfigObjects::Base_rateGroupDriver::divisors);
    """
  }

  instance timer: Zephyr.ZephyrRateDriver base id BASE_ID + 0x13000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    // 1000 ms base clock, divided down by rateGroupDriver
    Base::timer.configure(1000);
    """
  }

  @ USB CDC ACM console, carrying the ground link
  instance comDriver: Zephyr.ZephyrUartDriver base id BASE_ID + 0x14000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Base::comDriver.configure(DEVICE_DT_GET(DT_CHOSEN(zephyr_console)), 115200);
    """
  }

  @ Frame buffers: the framer's downlink frames and, with Ground, the UART
  @ driver's uplink reads
  instance commsBufferManager: Svc.BufferManager base id BASE_ID + 0x15000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Fw::MallocAllocator allocator;
    Svc::BufferManager::BufferBins bins;
    """
    phase Fpp.ToCpp.Phases.configComponents """
    memset(&ConfigObjects::Base_commsBufferManager::bins, 0, sizeof(ConfigObjects::Base_commsBufferManager::bins));
    ConfigObjects::Base_commsBufferManager::bins.bins[0].bufferSize = Base::BuffMgr::commsBuffSize;
    ConfigObjects::Base_commsBufferManager::bins.bins[0].numBuffers = Base::BuffMgr::commsBuffCount;
    Base::commsBufferManager.setup(
        Base::BuffMgr::commsBuffMgrId,
        0,
        ConfigObjects::Base_commsBufferManager::allocator,
        ConfigObjects::Base_commsBufferManager::bins
    );
    """
    phase Fpp.ToCpp.Phases.tearDownComponents """
    Base::commsBufferManager.cleanup();
    """
  }

  @ F Prime protocol framer (the GDS's --framing-selection fprime)
  instance framer: Svc.FprimeFramer base id BASE_ID + 0x16000

  @ Adapts the framed stream to the UART driver
  instance comStub: Svc.ComStub base id BASE_ID + 0x17000

  # ----------------------------------------------------------------------
  # Subtopologies
  # ----------------------------------------------------------------------

  @ The least every deployment needs: a 1 Hz rate group, time, and a one-way
  @ downlink of events and telemetry. There is no uplink here; import
  @ Ground.Subtopology as well to command the deployment from the GDS.
  @
  @ Not included, and to be added back only when something needs it:
  @ health pings (see Ground.fpp), command sequencer, parameter database
  @ (no component has parameters), version reporting, SystemResources (its
  @ CPU and memory figures are stubs on Zephyr), the text event logger
  @ (it prints on the same USB port as the link), the fatal event handler
  @ (no component emits FATAL events), and file uplink and downlink.
  @
  @ 1 Hz rate group members 0-5 belong to Base and Ground; subsystems start
  @ at member 6.
  topology Subtopology {

    instance chronoTime
    instance timer
    instance rateGroupDriver
    instance rateGroup_1Hz
    instance events
    instance tlmSend
    instance comQueue
    instance commsBufferManager
    instance framer
    instance comStub
    instance comDriver

    connections Downlink {
      events.PktSend -> comQueue.comPacketQueueIn[Ports_ComPacketQueue.EVENTS]
      # PktSend[0] is the REALTIME section. PktSend[1] (RECORDED) stays
      # unconnected until there is storage to record to, and the packetizer
      # skips sections whose port is unconnected.
      tlmSend.PktSend[0] -> comQueue.comPacketQueueIn[Ports_ComPacketQueue.TELEMETRY]

      comQueue.dataOut -> framer.dataIn
      framer.dataReturnOut -> comQueue.dataReturnIn
      framer.comStatusOut -> comQueue.comStatusIn
      framer.bufferAllocate -> commsBufferManager.bufferGetCallee
      framer.bufferDeallocate -> commsBufferManager.bufferSendIn

      framer.dataOut -> comStub.dataIn
      comStub.dataReturnOut -> framer.dataReturnIn
      comStub.comStatusOut -> framer.comStatusIn

      comStub.drvSendOut -> comDriver.$send
      comDriver.ready -> comStub.drvConnected
    }

    connections RateGroups {
      timer.CycleOut -> rateGroupDriver.CycleIn
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_1Hz] -> rateGroup_1Hz.CycleIn

      # 1Hz members 0-3 used here, 4-5 by Ground
      rateGroup_1Hz.RateGroupMemberOut[0] -> tlmSend.Run
      rateGroup_1Hz.RateGroupMemberOut[1] -> comQueue.run
      rateGroup_1Hz.RateGroupMemberOut[2] -> events.run
      rateGroup_1Hz.RateGroupMemberOut[3] -> commsBufferManager.schedIn
    }

  }

  @ The 0.5 Hz rate group, for deployments that sample something at 0.5 Hz.
  @ Import it alongside Subtopology. None of its members are used here.
  topology SlowRateGroup {

    instance rateGroupDriver
    instance rateGroup_0_5Hz

    connections RateGroups {
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_0_5Hz] -> rateGroup_0_5Hz.CycleIn
    }

  }

}
