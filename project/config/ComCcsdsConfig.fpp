# =======================================================================
# ComCcsds subtopology configuration override for the RP2350 deployment
#
# Upstream defaults target a Linux-class deployment. Three things do not
# fit in the RP2350's 520KiB SRAM:
#
#   1. QueueDepths. ComQueue allocates (events + tlm) * ComBuffer plus
#      (file * Fw::Buffer) in one chunk. FW_COM_BUFFER_MAX_SIZE is 512,
#      so the stock 200/500/100 depths ask for roughly 365KiB by
#      themselves.
#   2. BuffMgr bin sizing. The stock file bin (3000 * 30) reserves ~90KiB
#      for file downlink, which this deployment no longer performs.
#   3. StackSizes. Os::Zephyr::Task::start() calls k_thread_stack_alloc()
#      with the requested size. With CONFIG_DYNAMIC_THREAD_ALLOC=n the
#      stack must come from the pool, whose slots are exactly
#      CONFIG_DYNAMIC_THREAD_STACK_SIZE (8192) bytes. A 64KiB request
#      returns nullptr and the task fails with ERROR_RESOURCES.
# =======================================================================

module ComCcsdsConfig {
    #Base ID for the ComCcsds Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x02000000

    module QueueSizes {
        constant comQueue    = 50
        constant aggregator  = 10
    }

    module StackSizes {
        constant comQueue   = 8192
        constant aggregator = 8192
    }

    module Priorities {
        constant aggregator = 30
        constant comQueue   = 29
    }

    module CpuAffinities {
        constant aggregator = Os.TASK_DEFAULT
        constant comQueue   = Os.TASK_DEFAULT
    }

    # Queue configuration constants
    module QueueDepths {
        constant events      = 20
        constant tlm         = 30
        # File downlink is not part of this deployment, but ComQueue asserts
        # that every configured entry has a depth greater than zero.
        constant file        = 2
    }

    module QueuePriorities {
        constant events      = 0
        constant tlm         = 2
        constant file        = 1
    }

    # Buffer management constants
    module BuffMgr {
        constant frameAccumulatorSize  = 2048
        constant commsBuffSize         = 2048
        constant commsFileBuffSize     = 256
        constant commsBuffCount        = 5
        constant commsFileBuffCount    = 2
        constant commsBuffMgrId        = 200
    }
}
