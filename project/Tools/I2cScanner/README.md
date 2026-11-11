# I2cScanner

A bench tool that lists every device answering on the I2C buses. Use it when
bringing up a driver or a board, before suspecting the driver: it answers
"is the chip on the bus at the address I expect?" in one command.

It is built into the bench deployments (`EpsBench`, `AdcsBench`) through the
`BenchTools` subtopology. It is never part of the `Argus` flight image.

## Using it

1. Build, flash and start the GDS for a bench, as in the top-level README. For example:

   ```bash
   fprime-util build zephyr --build-cache build-fprime-zephyr-AdcsBench
   cp build-fprime-zephyr-AdcsBench/zephyr/zephyr.uf2 /Volumes/RP2350/   # after BOOTSEL
   fprime-gds --communication-selection uart --uart-device /dev/cu.usbmodem101 --uart-baud 115200 \
     --dictionary build-fprime-zephyr-AdcsBench/project/Deployments/AdcsBench/Top/AdcsBenchTopologyDictionary.json \
     --gui-port 5010 -n
   ```

2. In the **Commanding** tab, send `BenchTools.i2cScanner.SCAN`. It takes no arguments.

3. The **Events** tab shows one `BusScanned` event per bus:

   ```
   i2c0: 6 device(s): 0x29 0x40 0x41 0x44 0x45 0x68
   i2c1: 11 device(s): 0x0B 0x29 0x36 0x40 0x41 0x44 0x45 0x48 0x49 0x4A 0x68
   ```

Send it as often as you like, for example after reseating a connector. Because it
runs on command, its results are never lost the way events emitted at boot can be,
before the GDS has connected.

## Reading the result

Compare against the addresses in `FSW-mainboard/flight/hal/argus_v4.py`, which
lists every device on each bus. Only boards that are plugged in and powered answer.

| What you see | What it usually means |
| --- | --- |
| The expected address is listed | The chip is powered and answering. If its driver still fails, look at the driver or the chip's configuration. |
| The address is missing, but other parts on the same board answer | That one chip is unpowered, damaged, strapped to a different address, or behind something that is not passing traffic (an address translator, for example). |
| Nothing from a whole board | The board is not connected or not powered. |
| `none` on a bus | Nothing answered: check the bus's pull-ups and the peripheral 3.3 V rail. |
| All 112 addresses, list ending in `...` | SDA is held low, so every address looks like it acknowledged. Something is holding the bus; unplug boards one at a time to find it. |

## How it works

Each non-reserved 7-bit address, 0x08 to 0x77, gets a 1-byte read. A device that is
present acknowledges its address and the read succeeds; an empty address is not
acknowledged and the read fails. Reading a byte does not change the state of any part
on these buses.

- A scan takes tens of milliseconds per bus. It runs on the command dispatcher's thread,
  which is fine for a bench tool but is one reason it stays out of the flight image.
- Other components keep using the buses during a scan. The bus driver serializes the
  transactions, so a scan never corrupts a driver's reads.
- A bus whose read port is not connected is skipped.
- Event strings are capped at 200 characters, about 40 addresses. Beyond that the list
  ends in `...`, and the count stays exact.

## Adding it to a new bench

When you create a bench deployment (see `project/README.md`, "Adding a subsystem or a
bench"), add three things:

1. In `Top/topology.fpp`, import the subtopology next to the others:

   ```
   import BenchTools.Subtopology
   ```

2. In the same file, connect the scanner to the buses:

   ```
   connections BenchTools {
     BenchTools.i2cScanner.bus0Read -> Board.i2c0.read
     BenchTools.i2cScanner.bus1Read -> Board.i2c1.read
   }
   ```

3. In `Top/<Bench>TopologyDefs.hpp`, include its definitions with the other subtopologies:

   ```cpp
   #include "project/Subtopologies/BenchTools/SubtopologyTopologyDefs.hpp"
   ```

Do not import `BenchTools` into `Deployments/Argus`.

## Tests

```bash
cd project/Tools/I2cScanner
fprime-util check
```

The tests run the scanner against two fake buses. They check that each bus reports
exactly the addresses that answered, that every address is probed once with a 1-byte
read, the `none` case, and the stuck-bus case.
