module Components {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup_1Hz
    rateGroup_0_5Hz
    rateGroup_0_25Hz
  }

  deployment topology FirstDeployment {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import CdhCore.Subtopology
    import ComCcsds.Subtopology

  # ----------------------------------------------------------------------
  # Instances used in the topology
  # ----------------------------------------------------------------------
    # Parameter database only. The rest of the FileHandling subtopology
    # (fileUplink, fileDownlink, fileManager) is unused by this deployment.
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
    instance hiCmpntInstance
    # instance max17205Instance
    instance i2cDriver
    instance i2cDriver0
    instance lightSensorXp
    instance lightSensorXm
    instance lightSensorYp
    instance lightSensorYm
    instance lightSensorZpXp
    instance lightSensorZpYm
    instance lightSensorZpXm
    instance lightSensorZpYp
    instance lightSensorZm

  # ----------------------------------------------------------------------
  # Pattern graph specifiers
  # ----------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp
    event connections instance CdhCore.events
    telemetry connections instance CdhCore.tlmSend
    text event connections instance CdhCore.textLogger
    health connections instance CdhCore.$health
    param connections instance FileHandling.prmDb
    time connections instance chronoTime

  # ----------------------------------------------------------------------
  # Telemetry packets (only used when TlmPacketizer is used)
  # ----------------------------------------------------------------------

    # include "FirstDeploymentPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections ComCcsds_CdhCore {
      # Core events and telemetry to communication queue
      CdhCore.events.PktSend -> ComCcsds.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      CdhCore.tlmSend.PktSend -> ComCcsds.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

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

      # 1Hz rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_1Hz] -> rateGroup_1Hz.CycleIn
      rateGroup_1Hz.RateGroupMemberOut[0] -> CdhCore.tlmSend.Run
      rateGroup_1Hz.RateGroupMemberOut[1] -> systemResources.run
      rateGroup_1Hz.RateGroupMemberOut[2] -> ComCcsds.comQueue.run
      rateGroup_1Hz.RateGroupMemberOut[3] -> ComCcsds.aggregator.timeout
      rateGroup_1Hz.RateGroupMemberOut[4] -> CdhCore.cmdDisp.run
      rateGroup_1Hz.RateGroupMemberOut[5] -> comDriver.schedIn

      # 0.5Hz rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_0_5Hz] -> rateGroup_0_5Hz.CycleIn
      rateGroup_0_5Hz.RateGroupMemberOut[0] -> cmdSeq.schedIn

      # 0.25Hz rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup_0_25Hz] -> rateGroup_0_25Hz.CycleIn
      rateGroup_0_25Hz.RateGroupMemberOut[0] -> CdhCore.$health.Run
      rateGroup_0_25Hz.RateGroupMemberOut[1] -> ComCcsds.commsBufferManager.schedIn
    }

    connections CdhCore_cmdSeq {
      # Command Sequencer
      cmdSeq.comCmdOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn
    }

    connections FirstDeployment {
      # Light sensors -> I2C bus drivers. Output ports must be connected:
      # invoking an unconnected one asserts at runtime.
      #
      # i2c1: the +X/+Y faces and the +Z sun sensor.
      lightSensorXp.busWriteRead -> i2cDriver.writeRead
      lightSensorXp.busWrite -> i2cDriver.write
      lightSensorYp.busWriteRead -> i2cDriver.writeRead
      lightSensorYp.busWrite -> i2cDriver.write
      lightSensorZpXp.busWriteRead -> i2cDriver.writeRead
      lightSensorZpXp.busWrite -> i2cDriver.write
      lightSensorZpYm.busWriteRead -> i2cDriver.writeRead
      lightSensorZpYm.busWrite -> i2cDriver.write
      lightSensorZpXm.busWriteRead -> i2cDriver.writeRead
      lightSensorZpXm.busWrite -> i2cDriver.write
      lightSensorZpYp.busWriteRead -> i2cDriver.writeRead
      lightSensorZpYp.busWrite -> i2cDriver.write

      # i2c0: the -X/-Y/-Z faces.
      lightSensorXm.busWriteRead -> i2cDriver0.writeRead
      lightSensorXm.busWrite -> i2cDriver0.write
      lightSensorYm.busWriteRead -> i2cDriver0.writeRead
      lightSensorYm.busWrite -> i2cDriver0.write
      lightSensorZm.busWriteRead -> i2cDriver0.writeRead
      lightSensorZm.busWrite -> i2cDriver0.write

      # Rate group drives sampling, standing in for the ADCS task until that
      # exists: the first tick boots each sensor, every tick after reads it.
      # The 0.5 Hz group is used because it has exactly the nine free member
      # ports needed; the 1 Hz group has four. A read normally takes three
      # short transactions, but blocks for up to 1.1 s if a part never
      # reports a finished conversion. When ADCS lands it should call each
      # sensor's boot and read ports instead, and these lines should go.
      rateGroup_0_5Hz.RateGroupMemberOut[1] -> lightSensorXp.run
      rateGroup_0_5Hz.RateGroupMemberOut[2] -> lightSensorXm.run
      rateGroup_0_5Hz.RateGroupMemberOut[3] -> lightSensorYp.run
      rateGroup_0_5Hz.RateGroupMemberOut[4] -> lightSensorYm.run
      rateGroup_0_5Hz.RateGroupMemberOut[5] -> lightSensorZpXp.run
      rateGroup_0_5Hz.RateGroupMemberOut[6] -> lightSensorZpYm.run
      rateGroup_0_5Hz.RateGroupMemberOut[7] -> lightSensorZpXm.run
      rateGroup_0_5Hz.RateGroupMemberOut[8] -> lightSensorZpYp.run
      rateGroup_0_5Hz.RateGroupMemberOut[9] -> lightSensorZm.run
    }

  }

}
