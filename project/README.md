# How to add code to `project/`

This guide covers where new code goes and what each kind of PR changes. For building,
flashing and running the GDS, see the [top-level README](../README.md).

## The folders

| Folder | What lives there | Example |
| --- | --- | --- |
| `Drivers/` | One **passive** component per chip. It talks to the chip through `Drv` ports (I2C, GPIO) and has no idea which bus or pin it is on, or who calls it. | `Drivers/MAX17205/` |
| `Tasks/` | One **active** component per flight task. It owns a thread (like a Python `TemplateTask`) and calls its subsystem's drivers through ports. | `Tasks/EpsManager/` |
| `Subtopologies/<Subsystem>/` | The **instances** of a subsystem's drivers and task, the connections between them, and their configuration (I2C address, GPIO pin). Also the subsystem's telemetry packets. | `Subtopologies/Eps/` |
| `Subtopologies/Base/` | Command and telemetry plumbing, rate groups and the UART ground link. Every deployment imports it. | |
| `Subtopologies/Board/` | Hardware that more than one subsystem uses: the I2C buses. | `Board.i2c1` |
| `Deployments/Argus/` | The flight image. It imports every finished subsystem. | |
| `Deployments/<Subsystem>Bench/` | One subsystem alone, for bringing drivers up on the flatsat and measuring that subsystem in isolation. | `Deployments/EpsBench/` |
| `config/` | F´ configuration overrides that make it fit the RP2350. | |

A **component** (driver or task) is a class: it says what ports it has and what it does.
An **instance** is one object of that class with a name and an ID, such as
`Eps.fuelGauge`. Components go in `Drivers/` or `Tasks/`. Instances only go in a
subtopology, never in a deployment.

### Who connects what

- **Inside a subsystem** (task to its drivers, heater driver to its GPIOs): the subtopology.
- **To something shared** (a driver to a `Board` I2C bus, a task to a `Base` rate group):
  the deployment, because only the deployment has both sides.

## Adding a driver

Example: the MAX17205 fuel gauge, an EPS part on `i2c1`.

**1. The component.** Create `Drivers/MAX17205/` with the `.fpp`, `.hpp`, `.cpp`,
`CMakeLists.txt`, unit tests in `test/ut/`, and `docs/sdd.md`. Register it in
`Drivers/CMakeLists.txt`:

```cmake
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MAX17205/")
```

Put it in `module project`, like the existing drivers, and give it a `run: Svc.Sched`
input port if something needs to poll it before its task exists.

**2. The instance.** Add it to its subsystem's `.fpp`, `Subtopologies/Eps/Eps.fpp`. Take
the next free offset from `BASE_ID`, and put its configuration in a `phase` block. That
code runs at boot, in place of the hand-written `configureTopology()` the deployment used
to have:

```
  @ Battery fuel gauge, i2c1
  instance fuelGauge: project.MAX17205 base id BASE_ID + 0x05000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    Eps::fuelGauge.configure(<device address>, <shadow address>);
    """
  }
```

Then list it in the `topology Subtopology { ... }` block in the same file:

```
    instance fuelGauge
```

If the phase code needs a header (a Zephyr header, for example), include it in
`Subtopologies/Eps/SubtopologyTopologyDefs.hpp`.

**3. Its telemetry.** Add every telemetry channel to a packet in
`Subtopologies/Eps/EpsPackets.fppi`, using an id in Eps's range (200-299). A packet holds
at most 512 bytes, including a 15-byte header.

```
  packet FuelGauge id 201 group 1 {
    Eps.fuelGauge.StateOfCharge
    Eps.fuelGauge.RemainingCapacity
  }
```

If you forget one, the build fails with
`telemetry channel ... is neither used nor marked as omitted`.

**4. The bench wiring.** In `Deployments/EpsBench/Top/topology.fpp`, connect the driver to
its bus. Until the EPS task exists, drive it from a free rate-group port. `Base` uses
1 Hz ports 0-5, 0.5 Hz port 0 and 0.25 Hz ports 0-1:

```
    connections Bench {
      Eps.fuelGauge.busWriteRead -> Board.i2c1.writeRead
      Eps.fuelGauge.busWrite     -> Board.i2c1.write
      Base.rateGroup_1Hz.RateGroupMemberOut[6] -> Eps.fuelGauge.run
    }
```

Every output port a driver calls must be connected, or it asserts at runtime.

**5. Build, flash, test** the bench (commands in the top-level README):

```bash
fprime-util generate zephyr --build-cache build-fprime-zephyr-EpsBench -DARGUS_DEPLOYMENT=EpsBench
fprime-util build zephyr --build-cache build-fprime-zephyr-EpsBench
```

**Do not touch `Deployments/Argus/`.** The driver reaches the flight image when its task
does.

## Adding a task

Example: the EPS task.

1. Create `Tasks/EpsManager/` as an `active component`, and register it in
   `Tasks/CMakeLists.txt`. Its queue and stack sizes come from `Base.Default.QUEUE_SIZE`
   and `Base.Default.STACK_SIZE`. Every thread stack must equal
   `CONFIG_DYNAMIC_THREAD_STACK_SIZE` in `prj.conf`, and the number of threads must fit
   `CONFIG_DYNAMIC_THREAD_POOL_SIZE`.
2. Instantiate it in `Subtopologies/Eps/Eps.fpp`, and connect it to its drivers in a
   `connections` block there, e.g. `manager.fuelGaugeRead -> fuelGauge.read`. Add its
   health-ping thresholds to a `Subtopologies/Eps/PingEntries.hpp`, as
   `Subtopologies/Base/PingEntries.hpp` does, and include that from each deployment's
   `TopologyDefs.hpp`.
3. In `Deployments/EpsBench/Top/topology.fpp`, replace the rate-group lines that drove the
   drivers with one line driving the task: `Base.rateGroup_1Hz.RateGroupMemberOut[6] ->
   Eps.manager.run`.
4. Add the subsystem to the flight image. In `Deployments/Argus/Top/topology.fpp`, add the
   same bus and rate-group connections the bench has (`import Eps.Subtopology` is already
   there). In `ArgusPackets.fppi`, include the subsystem's packets file if it isn't already.

## Adding a subsystem or a bench

For example ADCS, which needs `Subtopologies/Adcs/` and `Deployments/AdcsBench/`.

1. **Subtopology.** Copy the layout of `Subtopologies/Eps/`: `Adcs.fpp` with
   `module Adcs`, `constant BASE_ID` from slot 03 (`0x10300000`), a
   `topology Subtopology` block, `SubtopologyTopologyDefs.hpp`, `AdcsPackets.fppi` (ids
   300-399), and `CMakeLists.txt`. Register it in `Subtopologies/CMakeLists.txt`, and
   claim its slot in the table there.
2. **Bench.** Copy `Deployments/EpsBench/`, and rename every `EpsBench` in file names and
   contents (`module`, topology name, header guards, `#include` paths, the packets set
   and its `TlmPackets` alias in `TopologyDefs.hpp`). Import `Base`, `Board` and the new
   subsystem, and include their packets files.
3. **CI.** Add the bench to the `deployment` matrix in `.github/workflows/build.yml`.

## IDs

| What | Rule | Where it's written down |
| --- | --- | --- |
| Instance base ID | `0x1SSCCxxx`: `SS` is the subtopology slot, `CC` the instance within it | `Subtopologies/CMakeLists.txt` |
| Telemetry packet id | The subtopology's range: Base 1-99, Board 100-199, Eps 200-299, Adcs 300-399 | Same table |

Because a subtopology's IDs don't depend on the deployment, a channel decodes the same
on the bench and on the flight image.
