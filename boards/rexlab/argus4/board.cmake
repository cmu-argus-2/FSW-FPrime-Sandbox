# SPDX-License-Identifier: Apache-2.0

# The RP2350 enumerates as a mass-storage device in BOOTSEL mode and is flashed
# by copying a UF2 onto it. The board-id is the one the RP2350 bootrom reports,
# not a board-specific string.
board_runner_args(uf2 "--board-id=RP2350")

board_runner_args(openocd --cmd-pre-init "source [find interface/cmsis-dap.cfg]")
board_runner_args(openocd --cmd-pre-init "source [find target/rp2350.cfg]")
board_runner_args(openocd --cmd-pre-init "set_adapter_speed_if_not_set 5000")
board_runner_args(probe-rs "--chip=RP235x")

include(${ZEPHYR_BASE}/boards/common/uf2.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/probe-rs.board.cmake)
