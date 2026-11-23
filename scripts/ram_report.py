#!/usr/bin/env python3
"""RAM used by a deployment, split into the task, F Prime overhead and the platform.

Usage:
    python3 scripts/ram_report.py BUILD_DIR [--heap-peak B] [--stacks-peak B] [--eps-stack-unused B]

BUILD_DIR is a Zephyr build cache, e.g. build-fprime-zephyr-EpsMin. Static RAM
(data + bss, as CI counts it) is read from its zephyr.elf, so no board is needed.
The runtime figures come from the EpsMemory telemetry packet after a few
minutes of running (HeapPeakBytes, AllStacksPeakBytes, EpsStackUnusedBytes);
pass them to add the heap and to report stack actually used instead of
reserved.

Each static symbol is attributed by the instance namespace it belongs to:

    EPS task       Eps::*         the EPS task and its drivers
    Core           Base::*        rate groups, time, events, telemetry, downlink
    Ground         Ground::*      uplink and command dispatcher
    Buses          Board::*       I2C bus drivers
    Platform       everything else: Zephyr (kernel, USB stack, main, idle and
                   interrupt stacks), libc, and F Prime's own runtime statics

Each F Prime thread's stack slot (CONFIG_DYNAMIC_THREAD_STACK_SIZE bytes from
Zephyr's dynamic thread pool) is attributed to the instance that owns it.
Pool slots no instance uses are listed as spare.
"""

import argparse
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRAM_BYTES = 520 * 1024

BUCKETS = [
    ("EPS task", r"^(Eps::|\w+::ConfigObjects::Eps_)"),
    ("Core (Base)", r"^(Base::|\w+::ConfigObjects::Base_)"),
    ("Ground", r"^(Ground::|\w+::ConfigObjects::Ground_)"),
    ("Buses (Board)", r"^(Board::|\w+::ConfigObjects::Board_)"),
]
ORDER = ["EPS task", "Core (Base)", "Ground", "Buses (Board)", "Platform"]

# Statically allocated thread stacks (K_THREAD_STACK_DEFINE and friends):
# F Prime's dynamic thread pool, and Zephyr's main, idle, work queue and USB
# threads. The interrupt stack is not a thread's, and stays static.
THREAD_STACK = re.compile(r"(_stack|_stacks|_stack_\d+)$")
NOT_THREAD_STACK = {"z_interrupt_stacks"}


def cache_value(build: Path, key: str) -> str:
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(key + ":"):
            return line.split("=", 1)[1]
    sys.exit(f"{key} not found in {build}/CMakeCache.txt")


def kconfig(build: Path, key: str) -> int:
    m = re.search(rf"^{key}=(\d+)$", (build / "zephyr" / ".config").read_text(), re.M)
    if not m:
        sys.exit(f"{key} not found in {build}/zephyr/.config")
    return int(m.group(1))


def bucket_of(name: str) -> str:
    for label, rx in BUCKETS:
        if re.search(rx, name):
            return label
    return "Platform"


def static_ram(build: Path, nm: str, size_tool: str, elf: Path):
    """Section total, symbol bytes per bucket (thread pool kept apart), and
    the bytes of every statically allocated thread stack."""
    # data + bss from Berkeley-format size, exactly as the CI RAM budget check
    # (.github/workflows/build.yml) computes it. Zephyr's no-init sections,
    # which hold the thread stacks, count as bss.
    last = subprocess.run([size_tool, str(elf)], capture_output=True, text=True, check=True).stdout.splitlines()[-1]
    _, data, bss = (int(x) for x in last.split()[:3])
    total = data + bss

    buckets = Counter()
    pool = 0
    thread_stacks = 0
    out = subprocess.run([nm, "-S", "-C", "-t", "d", str(elf)], capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        parts = line.split(None, 3)
        if len(parts) < 4 or parts[2] not in "bBdD":
            continue
        size, name = int(parts[1]), parts[3]
        if THREAD_STACK.search(name) and name not in NOT_THREAD_STACK:
            thread_stacks += size
        if name == "dynamic_stack":
            pool += size
        else:
            buckets[bucket_of(name)] += size
    return total, buckets, pool, thread_stacks


def active_instances(build: Path, deployment: str):
    """Instances whose thread startTasks() starts, e.g. ['Base::rateGroup_1Hz', ...]."""
    ac = build / "project" / "Deployments" / deployment / "Top" / f"{deployment}TopologyAc.cpp"
    text = ac.read_text()
    body = text[text.index("startTasks("):]
    body = body[: body.index("\n  }\n")]
    # Active instances are started with arguments; passive ones with start().
    return re.findall(r"^\s*([A-Za-z_]\w*::\w+)\.start\(\s*$", body, re.M)


def check_packetizer(build: Path, deployment: str):
    cfg = REPO / "project" / "Deployments" / deployment / "config" / "TlmPacketizerCfg.hpp"
    if not cfg.exists():
        cfg = REPO / "project" / "config" / "TlmPacketizerCfg.hpp"
    text = cfg.read_text()
    limit = {k: int(re.search(rf"{k}\s*=\s*(\d+)", text).group(1)) for k in ("MAX_PACKETIZER_PACKETS", "MAX_PACKETIZER_CHANNELS")}
    dictionary = build / "project" / "Deployments" / deployment / "Top" / f"{deployment}TopologyDictionary.json"
    d = json.loads(dictionary.read_text())
    channels = len(d["telemetryChannels"])
    packets = sum(len(s["members"]) for s in d.get("telemetryPacketSets", []))
    lines = [f"Packetizer limits ({cfg.relative_to(REPO)}): {packets}/{limit['MAX_PACKETIZER_PACKETS']} packets, "
             f"{channels}/{limit['MAX_PACKETIZER_CHANNELS']} channels"]
    if packets > limit["MAX_PACKETIZER_PACKETS"] or channels > limit["MAX_PACKETIZER_CHANNELS"]:
        lines.append("  ERROR: over a limit, so the deployment will assert at boot. Raise the limits.")
    return lines


def row(label: str, value: int, note: str = "") -> str:
    return f"  {label:<34}{value:>9,} B  {value * 100 / SRAM_BYTES:5.1f} %  {note}".rstrip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", type=Path, help="Zephyr build cache, e.g. build-fprime-zephyr-EpsMin")
    ap.add_argument("--heap-peak", type=int, help="HeapPeakBytes from the board")
    ap.add_argument("--stacks-peak", type=int, help="AllStacksPeakBytes from the board")
    ap.add_argument("--eps-stack-unused", type=int, help="EpsStackUnusedBytes from the board")
    args = ap.parse_args()

    build = args.build.resolve()
    elf = build / "zephyr" / "zephyr.elf"
    deployment = cache_value(build, "ARGUS_DEPLOYMENT")
    nm = cache_value(build, "CMAKE_NM")
    size_tool = str(Path(nm).with_name(Path(nm).name.replace("-nm", "-size")))

    total, buckets, pool, thread_stacks = static_ram(build, nm, size_tool, elf)
    slot = kconfig(build, "CONFIG_DYNAMIC_THREAD_STACK_SIZE")
    actives = active_instances(build, deployment)

    stacks = Counter()
    for inst in actives:
        stacks[bucket_of(inst + "::")] += slot
    spare = pool - slot * len(actives)

    print(f"{deployment}: {elf.relative_to(REPO) if elf.is_relative_to(REPO) else elf}")
    print()
    print("Static RAM, as linked (data + bss, as CI counts it):")
    for label in ORDER:
        if buckets[label] or stacks[label]:
            note = f"incl. {stacks[label] // slot} thread stack(s) of {slot} B" if stacks[label] else ""
            print(row(label, buckets[label] + stacks[label], note))
    if spare:
        print(row("Spare thread pool slots", spare, f"{spare // slot} slot(s) no instance uses"))
    attributed = sum(buckets.values()) + pool
    print(row("Padding and unnamed data", total - attributed))
    print(row("Total", total))
    print()

    owners = ", ".join(a.replace("::", ".") for a in actives)
    print(f"Threads ({len(actives)}): {owners}")
    for line in check_packetizer(build, deployment):
        print(line)
    print()

    fprime = buckets["Core (Base)"] + stacks["Core (Base)"] + buckets["Ground"] + stacks["Ground"]
    eps = buckets["EPS task"] + stacks["EPS task"]
    print("Summary (static, stacks at their reserved size):")
    print(row("EPS task", eps))
    print(row("F Prime overhead (Base + Ground)", fprime))
    print(row("Platform, buses, spare, padding", total - eps - fprime))
    print()

    if args.heap_peak is not None or args.stacks_peak is not None:
        print("At runtime:")
        used = total
        if args.stacks_peak is not None:
            used = total - thread_stacks + args.stacks_peak
            print(row("Static, minus reserved thread stacks", total - thread_stacks,
                      f"{thread_stacks:,} B of stacks, every thread"))
            print(row("+ stack actually used, all threads", args.stacks_peak, "AllStacksPeakBytes"))
        if args.heap_peak is not None:
            used += args.heap_peak
            print(row("+ heap peak", args.heap_peak, "HeapPeakBytes: mostly Base's queues"))
        print(row("RAM used", used))
        if args.eps_stack_unused is not None:
            eps_used = buckets["EPS task"] + (slot - args.eps_stack_unused)
            print(row("of which EPS task", eps_used, "its statics + its thread's peak stack"))
        print()

    print(f"BENCH,fprime,memory,static_ram,{deployment},total,{total},B,1")
    print(f"BENCH,fprime,memory,static_ram,{deployment},eps,{eps},B,1")
    print(f"BENCH,fprime,memory,static_ram,{deployment},fprime_overhead,{fprime},B,1")


if __name__ == "__main__":
    main()
