# FSW-fprime-sandbox

An [F´ (F Prime)](https://fprime.jpl.nasa.gov) flight software deployment running on
[Zephyr RTOS](https://zephyrproject.org) on a Raspberry Pi Pico 2 (RP2350, Cortex-M33).

The deployment implements the F´ "Hello World" tutorial component (`HiComponent`, with a
`SAY_HI` command) on top of a trimmed CCSDS command/telemetry stack, and talks to the F´
Ground Data System over the board's USB serial port.

## Requirements

- Raspberry Pi Pico 2 (RP2350) — board target `rpi_pico2/rp2350a/m33`
- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html)
  with the `arm-zephyr-eabi` toolchain (developed against SDK 1.0.1)
- Python 3, CMake, Ninja
- macOS or Linux

## Setup

```bash
git clone <this-repo> && cd FSW-fprime-sandbox
git submodule update --init --recursive
```

### Apply the submodule patches

This deployment needs two small fixes to its submodules. They cannot be committed to this
repository directly, because git tracks submodules by commit SHA rather than by content.

```bash
./patches/apply.sh
```

| Patch | Fixes |
| --- | --- |
| `0001-fprime-install-destination.patch` | F´'s install step passes `CMAKE_INSTALL_PREFIX=/`, which newer CMake turns into an empty destination, failing the build. |
| `0002-fprime-zephyr-uart-irq-update.patch` | Zephyr 4.4 changed `uart_irq_update()` from returning `int` to `void`; the F´ Zephyr UART driver still checked its return value. |

The script is idempotent — running it twice is harmless.

### Fetch the Zephyr workspace

`lib/zephyr-workspace/` is roughly 9 GB and is therefore not committed. `west` reconstructs
it from `west.yml`, which pins Zephyr and every module to exact commits, so this produces
the same tree the deployment was built and tested against:

```bash
python3 -m venv fprime-venv
source fprime-venv/bin/activate
pip install -r requirements.txt

# `.west/` is not committed either, so create the workspace against the
# in-tree manifest before updating. Only needed once.
west init -l .
west update
```

## Build

Generate the build cache once, then build. The board comes from
`default_cmake_options` in `settings.ini`, so no `-DBOARD=` is needed:

```bash
source fprime-venv/bin/activate
fprime-util generate zephyr   # once, or after changing settings.ini
fprime-util build zephyr
```

The `zephyr` argument is required. Plain `fprime-util build` targets a host-native build
cache that this project does not generate, and fails with
`could not find CMAKE_PROJECT_NAME in Cache`.

Build output: `build-fprime-automatic-zephyr/zephyr/zephyr.uf2`

## Flash

1. Unplug the board, hold **BOOTSEL**, plug it back in, release.
2. Drag `build-fprime-automatic-zephyr/zephyr/zephyr.uf2` onto the `RP2350` drive that mounts.

The board reboots and runs the deployment automatically.

## Run the Ground Data System

Find the board's serial port. On macOS use the `/dev/cu.*` device, not `/dev/tty.*`:

```bash
ls /dev/cu.usbmodem*
```

Then:

```bash
source fprime-venv/bin/activate
fprime-gds \
  --communication-selection uart \
  --uart-device /dev/cu.usbmodem101 \
  --uart-baud 115200 \
  --dictionary build-fprime-automatic-zephyr/project/Components/FirstDeployment/Top/FirstDeploymentTopologyDictionary.json \
  --gui-port 5010 \
  -n
```

Open <http://127.0.0.1:5010>. The status indicator ("the orb") turns green once telemetry
is flowing.

To exercise the tutorial component, send `Components.hiCmpntInstance.SAY_HI` from the
**Commanding** tab with any greeting string. A `SayHiEvent` appears under **Events**, and
`GreetingCount` increments under **Channels**.

Notes:

- `--gui-port 5010` avoids port 5000, which macOS AirPlay Receiver occupies and answers
  with an HTTP 403.
- `-n` tells the GDS not to launch a binary itself; the flight software is already running
  on the board.
- Pass the dictionary from the build tree, as shown. A stale copy under `build-artifacts/`
  may carry outdated component IDs, in which case nothing decodes.

## RP2350 configuration notes

F´'s stock configuration targets Linux-class systems. Several defaults do not fit the
RP2350's 520 KiB of SRAM, and are overridden in `project/config/` rather than by editing
the vendored framework:

| Override | Reason |
| --- | --- |
| `TlmChanImplCfg.hpp` | The stock 500-bucket telemetry hash table needs ~600 KiB by itself. Sized to the deployment's channel count instead. |
| `ComCcsdsConfig.fpp` | Stock ComQueue depths (200 events / 500 telemetry / 100 file) request ~365 KiB in a single allocation; the buffer manager's file bin reserves a further ~90 KiB. |
| `CdhCoreConfig.fpp`, `FileHandlingConfig.fpp`, `ComCcsdsConfig.fpp` | Stock thread stacks are 64 KiB each. `Os::Zephyr::Task` serves stacks from Zephyr's dynamic thread pool, whose slots are exactly `CONFIG_DYNAMIC_THREAD_STACK_SIZE` (8192) bytes, so larger requests fail. |
| `PlatformCfg.fpp` | Sizes F´'s OS handle storage to Zephyr's actual object sizes on this target. |

Do **not** set `CONFIG_HEAP_MEM_POOL_SIZE` in `prj.conf` to give F´ more heap. That sizes
Zephyr's kernel heap (`k_malloc`), which F´ never calls, and it is reserved in `.bss`.
Because `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=-1` sizes the `malloc()` arena as whatever
RAM remains after static allocation, raising the kernel heap directly *shrinks* the heap
F´ actually uses.

The topology also omits the `DataProducts` and `FileHandling` subtopologies (apart from
`prmDb`, which backs the topology's parameter connections), since neither is used here.

### Zephyr revision

`west.yml` pins Zephyr to commit `5a5e6c5b3cb9` (`v4.4.0-15270-g5a5e6c5b3cb`) rather than
to a release tag. Zephyr 4.4 changed `uart_irq_update()` from returning `int` to returning
`void`, and `patches/0002` adapts the fprime-zephyr UART driver to that signature. That
patch does not apply to Zephyr 4.3, so changing this pin means revisiting the patch.

Module revisions are imported from Zephyr's own manifest rather than pinned separately,
which keeps them consistent with whichever Zephyr revision is selected.
