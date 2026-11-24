#!/usr/bin/env python3
"""Generate AdcsVectors.hpp: expected outputs for the AdcsLogic unit tests.

Runs the flight software's own ADCS functions (control laws, coil allocator,
mode logic and sensor checks) with numpy standing in for ulab, on a fixed set
of inputs, and writes the results as C++ initializers.

    python3 gen_adcs_vectors.py <FSW-mainboard>/flight > AdcsVectors.hpp

Needs numpy. The output is deterministic, so regenerating it with an
unchanged flight tree produces an identical file.
"""

import ast
import sys
import types
from pathlib import Path

import numpy as np


def extract(path, names):
    """Return the top-level functions, classes and assignments named in `names`."""
    tree = ast.parse(Path(path).read_text())
    keep = []
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)) and node.name in names:
            keep.append(node)
        elif isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in names for t in node.targets
        ):
            keep.append(node)
    found = {getattr(n, "name", None) or n.targets[0].id for n in keep}
    missing = set(names) - found
    if missing:
        sys.exit(f"{path}: not found: {sorted(missing)}")
    return ast.Module(body=keep, type_ignores=[])


def load(flight):
    flight = Path(flight)
    ns = {"np": np}

    # Constants. ControllerModes reads the SD card, so only its values are kept.
    exec(compile(extract(flight / "apps/adcs/consts.py", ["StatusConst", "Modes", "ControllerConst"]), "consts", "exec"), ns)
    ns["ControllerModes"] = types.SimpleNamespace(BDOT=0, BCROSS=1, SUN_POINTING=2)

    # Control laws and the coil allocator, with a stand-in for the coil drivers.
    class Satellite:
        available = [True] * 6
        applied = {}

        def TORQUE_DRIVERS_AVAILABLE(self, face):
            return self.available[("XP", "XM", "YP", "YM", "ZP", "ZM").index(face)]

        def APPLY_MAGNETIC_CONTROL(self, face, throttle):
            self.applied[face] = throttle

    ns["SATELLITE"] = Satellite()
    exec(
        compile(
            extract(
                flight / "apps/adcs/acs.py",
                [
                    "readings_are_valid",
                    "bdot_controller",
                    "bcross_controller",
                    "mcm_coil_allocator",
                    "_DERIVATIVE_WEIGHTS",
                    "_MCM_FACES",
                    "_COIL_STATUS",
                    "_U_EFF",
                ],
            ),
            "acs",
            "exec",
        ),
        ns,
    )

    # Sensor checks and mode logic, with a stand-in for the IMU.
    class Imu:
        gyro_value = (0.0, 0.0, 0.0)
        mag_value = (0.0, 0.0, 0.0)

        def gyro(self):
            return self.gyro_value

        def mag(self):
            return self.mag_value

    ns["SATELLITE"].IMU_AVAILABLE = True
    ns["SATELLITE"].IMU = Imu()
    exec(
        compile(
            extract(
                flight / "apps/adcs/sensors.py",
                ["_MIN_MAG_NORM", "_MAX_MAG_NORM", "_MAX_GYRO_NORM", "read_gyro", "read_magnetometer", "update_mode"],
            ),
            "sensors",
            "exec",
        ),
        ns,
    )
    return ns


def f(x):
    """A float as a C++ F32 literal. Whole numbers get a decimal point, which
    C++ requires before the F suffix."""
    if not np.isfinite(x):
        return "NAN"
    text = f"{float(x):.9g}"
    if not any(c in text for c in ".en"):
        text += ".0"
    return text + "F"


def vec(v):
    return "{" + ", ".join(f(x) for x in v) + "}"


def main(flight):
    ns = load(flight)
    sat, imu = ns["SATELLITE"], ns["SATELLITE"].IMU
    rng = np.random.default_rng(20261123)
    out = []
    w = out.append

    def unit():
        v = rng.normal(size=3)
        return v / np.linalg.norm(v)

    # --- Gyro check: rad/s in, status and checked value out -----------------
    max_gyro = ns["_MAX_GYRO_NORM"]
    w("const GyroCase GYRO_CASES[] = {")
    for norm in (0.0, 0.1, 1.0, max_gyro * 0.999, max_gyro * 1.001, 50.0):
        raw = unit() * norm
        imu.gyro_value = tuple(raw)
        status, value = ns["read_gyro"]()
        w(f"    {{{vec(raw)}, {int(status)}, {vec(value)}}},")
    w("};\n")

    # --- Magnetometer correction: uT in, T out --------------------------------
    w("const MagCase MAG_CASES[] = {")
    for norm_ut, bias_ut, scale in (
        (45.0, (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
        (45.0, (3.0, -2.0, 1.5), (1.02, 0.97, 1.1)),
        (0.5, (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
        (3000.0, (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
        (1200.0, (10.0, 10.0, -10.0), (1.0, 1.0, 1.0)),
    ):
        raw = unit() * norm_ut
        bias_t = np.array(bias_ut) * 1e-6
        ns["_MAG_BIAS"] = bias_t.copy()
        ns["_MAG_SCALE"] = np.array(scale)
        imu.mag_value = tuple(raw)
        status, value = ns["read_magnetometer"]()
        w(f"    {{{vec(raw)}, {vec(bias_t)}, {vec(scale)}, {int(status)}, {vec(value)}}},")
    w("};\n")

    # --- B-dot: six samples 80 ms apart --------------------------------------
    w("const BdotCase BDOT_CASES[] = {")
    for spin in (0.01, 0.2, 1.5, 0.0):
        b0 = unit() * 45e-6
        axis = unit()
        samples = []
        for k in range(6):
            angle = spin * 0.08 * k
            # Rotate b0 about axis (Rodrigues), plus a little noise.
            b = (
                b0 * np.cos(angle)
                + np.cross(axis, b0) * np.sin(angle)
                + axis * np.dot(axis, b0) * (1 - np.cos(angle))
            )
            samples.append(b + rng.normal(scale=2e-8, size=3))
        buf = np.array(samples)
        u = ns["bdot_controller"](buf, 0.08)
        w("    {{" + ", ".join(vec(s) for s in samples) + f"}}, 0.08F, {vec(u)}}},")
    w("};\n")

    # --- B-cross ---------------------------------------------------------------
    w("const BcrossCase BCROSS_CASES[] = {")
    for omega_norm in (0.0, 0.05, 0.5, 2.0):
        mag = unit() * 45e-6
        omega = unit() * omega_norm
        u = ns["bcross_controller"](mag, omega)
        w(f"    {{{vec(mag)}, {vec(omega)}, {vec(u)}}},")
    w("};\n")

    # --- Coil allocation ---------------------------------------------------------
    w("const AllocCase ALLOC_CASES[] = {")
    faces = ("XP", "XM", "YP", "YM", "ZP", "ZM")
    for available, u_norm in (
        ([True] * 6, 0.5),                                   # all coils, below the power limit
        ([True] * 6, 3.0),                                   # all coils, scaled down
        ([True, False, True, True, True, True], 0.6),        # one coil of an axis missing
        ([False, False, True, True, True, True], 0.6),       # X axis failed: projected along b
        ([False, False, False, False, True, True], 0.6),     # two axes failed: no compensation
        ([False] * 6, 0.6),                                  # no coil drivers at all
    ):
        u = unit() * u_norm
        b = unit() * 45e-6
        sat.available = list(available)
        sat.applied = {}
        status = ns["mcm_coil_allocator"](u, b)
        throttle = [sat.applied.get(face, 0.0) for face in faces]
        avail = "{" + ", ".join("true" if a else "false" for a in available) + "}"
        stat = "{" + ", ".join("true" if s else "false" for s in status) + "}"
        w(f"    {{{vec(u)}, {vec(b)}, {avail}, {vec(throttle)}, {stat}}},")
    w("};\n")

    # --- Mode logic --------------------------------------------------------------
    # Rates kept at least 0.005 rad/s from every threshold, so single-precision
    # rounding cannot flip a comparison.
    rates = (0.0, 0.03, 0.078, 0.095, 0.3, 0.53, 0.55, 1.2, 2.6, 2.64, 3.0, 3.07, 5.0)
    w("const ModeCase MODE_CASES[] = {")
    for current in range(5):
        for controller in (0, 1):
            for gyro_status in (0, 21):
                for rate in rates:
                    omega = unit() * rate
                    mode = ns["update_mode"](current, controller, gyro_status, omega, 0, np.zeros(3))
                    w(f"    {{{current}, {controller}, {gyro_status}, {vec(omega)}, {int(mode)}}},")
    w("};")

    print("// Generated by gen_adcs_vectors.py from the flight software's ADCS functions.")
    print("// Do not edit by hand; regenerate instead.")
    print("// clang-format off")
    print()
    print("\n".join(out))
    print()
    print("// clang-format on")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
