# FSW-fprime-sandbox

An [F´ (F Prime)](https://fprime.jpl.nasa.gov) flight software deployment running on
[Zephyr RTOS](https://zephyrproject.org) on the Argus mainboard (RP2350B, Cortex-M33).

It is one of three ports of the Argus Python flight software (the others are in Rust and
C), built so the three can be compared on the same hardware. The deployments run on a
trimmed CCSDS command/telemetry stack and talk to the F´ Ground Data System over the
board's USB serial port.

## Repository layout

```
project/
  config/          F´ configuration overrides for the RP2350
  Drivers/         one passive component per chip
  Tasks/           one active component per flight task (EPS, ADCS, ...)
  Subtopologies/
    Base/          command/telemetry plumbing, rate groups, UART link
    Board/         buses shared between subsystems (I2C)
    Eps/           EPS drivers, and the EPS task once it exists, wired together
  Deployments/
    Argus/         the flight image: every subsystem
    EpsBench/      Base + Board + Eps only
```

Each layer has one job:

- **A driver** talks to one chip through `Drv` ports and knows nothing about which bus or
  pin it is on, or who calls it. It is passive, so its transactions run on the thread of
  whatever calls it.
- **A task** is active (it has its own thread, like a Python `TemplateTask`) and calls its
  subsystem's drivers through ports.
- **A subtopology** instantiates a subsystem's drivers and task, wires them to each other,
  and configures them (I2C addresses, GPIO pins) in `phase` blocks in its `.fpp` file.
  Connections to shared things — a `Board` bus, a `Base` rate group — are left to the
  deployment.
- **A deployment** imports subtopologies and makes those cross-subsystem connections.

Instance IDs are `0x1SSCCxxx`, where `SS` is the subtopology's slot (listed in
`project/Subtopologies/CMakeLists.txt`). An instance therefore has the same ID, and decodes
with the same GDS dictionary entries, in every deployment that imports it.

### What a PR touches

| PR | Touches | Does not touch |
| --- | --- | --- |
| Driver | `Drivers/<Chip>/` (component, unit tests, `docs/sdd.md`) and its line in `Drivers/CMakeLists.txt`; its instance in `Subtopologies/<Subsystem>/`; bus wiring and a temporary rate-group hookup in `Deployments/<Subsystem>Bench/` | `Deployments/Argus/` |
| Task | `Tasks/<Task>/`; its wiring to the drivers in `Subtopologies/<Subsystem>/`; replace the bench's rate-group hookup with the task; import the subsystem into `Deployments/Argus/` | other subsystems |
| Infrastructure | `Subtopologies/Base/`, `Subtopologies/Board/`, `config/`, CI | |

A driver goes into its subsystem's subtopology from the start, since that is where it
will stay, and is brought up on the flatsat through that subsystem's bench. The flight
image only changes when a whole task lands.

Step-by-step instructions for adding a driver, a task or a subsystem are in
[`project/README.md`](project/README.md).

## Requirements

- Argus mainboard, revision 4 — board target `argus/rp2350b/m33_0`, defined in
  [FSW-Zephyr-Board-Lib](https://github.com/cmu-argus-2/FSW-Zephyr-Board-Lib)
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

`west update` also fetches FSW-Zephyr-Board-Lib into
`lib/zephyr-workspace/FSW-Zephyr-Board-Lib/`. That repo provides the `argus` board, the
Zephyr revision and the module list; see [Board and Zephyr revision](#board-and-zephyr-revision).

There must be no `.west/` directory inside `lib/zephyr-workspace/`. The build runs west from
inside the Zephyr tree, and west uses the nearest `.west/` above it, so a nested one makes
the build ignore this repository's `west.yml`: it loads Zephyr's full module list and cannot
find the `argus` board.

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

This builds the flight image, `Argus`. Zephyr allows only one deployment per build cache,
so a bench gets a build cache of its own, picked with `ARGUS_DEPLOYMENT`. Every command run
against it needs the same `--build-cache`:

```bash
fprime-util generate zephyr --build-cache build-fprime-zephyr-EpsBench -DARGUS_DEPLOYMENT=EpsBench
fprime-util build zephyr --build-cache build-fprime-zephyr-EpsBench
```

Build output: `build-fprime-zephyr-EpsBench/zephyr/zephyr.uf2`. CI builds every deployment.

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
  --dictionary build-fprime-automatic-zephyr/project/Deployments/Argus/Top/ArgusTopologyDictionary.json \
  --gui-port 5010 \
  -n
```

Open <http://127.0.0.1:5010>. The status indicator ("the orb") turns green once telemetry
is flowing.

For a bench, use the dictionary from its own build cache, e.g.
`build-fprime-zephyr-EpsBench/project/Deployments/EpsBench/Top/EpsBenchTopologyDictionary.json`.

To check the link, send `CdhCore.cmdDisp.CMD_NO_OP` from the **Commanding** tab. A
`NoOpReceived` event appears under **Events**, and `CommandsDispatched` increments under
**Channels**.

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
| `CdhCoreTlmConfig.fpp` | Swaps CdhCore's `TlmChan` for `TlmPacketizer`. `TlmChan` costs ~1.2 KiB of static RAM per channel whatever its type; the packetizer stores channels packed into fixed packets. Every channel must be listed in a packet: see [Telemetry packets](#telemetry-packets). |
| `TlmPacketizerCfg.hpp` | The stock limits (50 packets, 200 channels) cost ~75 KiB. Sized to the largest deployment instead; both limits are checked by assertions at boot. |
| `TlmChanImplCfg.hpp` | Only used if a deployment goes back to `TlmChan`. The stock 500-bucket hash table needs ~600 KiB by itself. |
| `ComCcsdsConfig.fpp` | Stock ComQueue depths (200 events / 500 telemetry / 100 file) request ~365 KiB in a single allocation; the buffer manager's file bin reserves a further ~90 KiB. |
| `CdhCoreConfig.fpp`, `FileHandlingConfig.fpp`, `ComCcsdsConfig.fpp` | Stock thread stacks are 64 KiB each. `Os::Zephyr::Task` serves stacks from Zephyr's dynamic thread pool, whose slots are exactly `CONFIG_DYNAMIC_THREAD_STACK_SIZE` (8192) bytes, so larger requests fail. |
| `PlatformCfg.fpp` | Sizes F´'s OS handle storage to Zephyr's actual object sizes on this target. |

Do **not** set `CONFIG_HEAP_MEM_POOL_SIZE` in `prj.conf` to give F´ more heap. That sizes
Zephyr's kernel heap (`k_malloc`), which F´ never calls, and it is reserved in `.bss`.
Because `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=-1` sizes the `malloc()` arena as whatever
RAM remains after static allocation, raising the kernel heap directly *shrinks* the heap
F´ actually uses.

### Telemetry packets

Telemetry is downlinked in fixed packets rather than channel by channel. Each subtopology
lists its channels in `<Name>Packets.fppi` next to its `.fpp` file, using its own range of
packet ids (`Base` 1-99, `Board` 100-199, `Eps` 200-299, `Adcs` 300-399). Each deployment's
`Top/<Name>Packets.fppi` includes the files of the subtopologies it imports.

A component that adds a telemetry channel must add it to a packet in its subtopology's
packets file. Otherwise the build fails with
`telemetry channel ... is neither used nor marked as omitted`. A packet must fit in 512
bytes including a 15-byte header, or `TlmPacketizer` asserts at boot.

The deployments also omit the `DataProducts` and `FileHandling` subtopologies (apart from
`prmDb`, which backs the topology's parameter connections), since neither is used here.

### Board and Zephyr revision

The board definition is not kept in this repository. It lives in
[FSW-Zephyr-Board-Lib](https://github.com/cmu-argus-2/FSW-Zephyr-Board-Lib), which is shared
with the C flight software so both build against the same pins, buses and Zephyr version.
`west.yml` imports that repo at an exact commit; to pick up board changes, move that commit
and run `west update`. Board fixes go to FSW-Zephyr-Board-Lib, not here.

`settings.ini` selects the board (`BOARD=argus/rp2350b/m33_0`) and the shared
`argus-common` snippet (`SNIPPET=argus-common`), which turns on settings both teams agreed
on, such as I2C.

The Zephyr revision comes from the board lib's own `west.yml`, so this repository does not
list `zephyr` itself. Zephyr 4.4 changed `uart_irq_update()` from returning `int` to
returning `void`, and `patches/0002` adapts the fprime-zephyr UART driver to that signature.
If the board lib moves to a Zephyr revision that changes this API again, the patch needs
revisiting.
