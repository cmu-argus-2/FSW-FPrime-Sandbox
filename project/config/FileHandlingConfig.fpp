# =======================================================================
# FileHandling subtopology configuration override for the RP2350 deployment
#
# This deployment instantiates only FileHandling.prmDb (the parameter
# database backing the topology's param connections); fileUplink,
# fileDownlink and fileManager are not part of the topology. Only
# StackSizes differ from upstream: Os::Zephyr::Task::start() serves the
# requested size from the dynamic thread pool, whose slots are exactly
# CONFIG_DYNAMIC_THREAD_STACK_SIZE (8192) bytes, so a 64KiB request fails.
# =======================================================================

module FileHandlingConfig {
    #Base ID for the FileHandling Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x05000000

    module QueueSizes {
        constant fileUplink    = 10
        constant fileDownlink  = 10
        constant fileManager   = 10
        constant prmDb         = 10
    }

    module StackSizes {
        constant fileUplink    = 8192
        constant fileDownlink  = 8192
        constant fileManager   = 8192
        constant prmDb         = 8192
    }

    module Priorities {
        constant fileUplink    = 24
        constant fileDownlink  = 23
        constant fileManager   = 22
        constant prmDb         = 21
    }

    module CpuAffinities {
        constant fileUplink    = Os.TASK_DEFAULT
        constant fileDownlink  = Os.TASK_DEFAULT
        constant fileManager   = Os.TASK_DEFAULT
        constant prmDb         = Os.TASK_DEFAULT
    }

    # File downlink configuration constants
    module DownlinkConfig {
        constant cooldown       = 1000         # File downlink cooldown in ms
        constant cycleTime      = 1000         # File downlink cycle time in ms
        constant fileQueueDepth = 10           # File downlink queue depth
    }
}
