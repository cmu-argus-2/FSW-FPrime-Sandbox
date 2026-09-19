# =======================================================================
# CdhCore subtopology configuration override for the RP2350 deployment
#
# Only StackSizes differ from upstream. Os::Zephyr::Task::start() passes
# the requested size to k_thread_stack_alloc(), and with
# CONFIG_DYNAMIC_THREAD_ALLOC=n that must be satisfied from the pool,
# whose slots are exactly CONFIG_DYNAMIC_THREAD_STACK_SIZE (8192) bytes.
# The upstream 64KiB request cannot be served and fails the task start.
# =======================================================================

module CdhCoreConfig {
    #Base ID for the CdhCore Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x01000000

    module QueueSizes {
        constant cmdDisp     = 10
        constant events      = 10
        constant tlmSend     = 10
        constant $health     = 25
    }

    module StackSizes {
        constant cmdDisp     = 8192
        constant events      = 8192
        constant tlmSend     = 8192
    }

    module Priorities {
        constant cmdDisp     = 35
        constant $health     = 24
        constant events      = 23
        constant tlmSend     = 22

    }

    module CpuAffinities {
        constant cmdDisp     = Os.TASK_DEFAULT
        constant events      = Os.TASK_DEFAULT
        constant tlmSend     = Os.TASK_DEFAULT
    }
}
