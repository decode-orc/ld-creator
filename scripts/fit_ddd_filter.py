# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Fits the DomesdayDuplicator anti-alias filter to the measured board.

Register S13.filter_fit; S13 note, "Filter model". Dataset: the
DomesdayDuplicator repository's `hardware/doc/DdD Gain and filter
calculations.xlsx`, measured on one board at gain switch 1000 with
200 mV p-p in. Sheet 1 is the level (dB relative to the ADC's maximum),
sheet 2 the phase lag (degrees, every 250 kHz to 16 MHz).

Model: the netlist's ladder (register S13.filter_topology) with seven
effective component values, times the fixed first-order input sections
and the OPA690's closed-loop pole (250 MHz / 8.5, register
S13.amplifier_bandwidth). Two nuisance terms are fitted and not modelled:
a gain offset (the gain resistors' tolerance, absorbed by the level
setting) and a pure delay (the measurement path). Method: weighted least
squares, magnitude to 17.8 MHz at 0.1 dB per unit, phase to 15 MHz at 2°
per unit; 12 random starts within ±15 % of nominal, bounds 0.4-2.5 ×
nominal. The best start is the global minimum found from 28 further
starts.

    python3 -E -B scripts/fit_ddd_filter.py
"""

from __future__ import annotations

import math
import sys

import numpy as np
from scipy import optimize

SHEET1 = (
    (10000, -3.9569), (12116, -3.2169), (14678, -2.6693), (17783, -2.2638),
    (21545, -1.9809), (26102, -1.7482), (31623, -1.6039), (38312, -1.5024),
    (46416, -1.4222), (56235, -1.3891), (68130, -1.3488), (82541, -1.3290),
    (100000, -1.3231), (121153, -1.3097), (146780, -1.3094), (177828, -1.3037),
    (215444, -1.3035), (261016, -1.3037), (316228, -1.3090), (383119, -1.3238),
    (464159, -1.3289), (562342, -1.3437), (681293, -1.3681), (825405, -1.4233),
    (1000000, -1.4676), (1211528, -1.5654), (1467800, -1.6678),
    (1778280, -1.8342), (2154435, -2.0469), (2610158, -2.3293),
    (3162278, -2.6237), (3831187, -2.9747), (4641589, -3.2669),
    (5623414, -3.3407), (6812921, -3.0944), (8254042, -2.6230),
    (10000000, -3.0480), (12115277, -4.7400), (14677993, -4.0375),
    (17782795, -29.7295), (21544347, -43.5214),
)
SHEET2_LAG_DEG = (
    3.696, 10.99, 15.57, 23.31, 25.99, 33.62, 35.95, 38.96, 46.43, 51.31,
    55.13, 59.42, 65.06, 67.43, 73.68, 73.81, 81.44, 85.4, 89.18, 89.44,
    95.54, 99.01, 105.5, 109.2, 113.7, 114.2, 121.1, 125.4, 130.9, 135,
    140, 146.6, 153.6, 162.4, 165.5, 172.9, 176.7, 188.8, 192, 199.7,
    205.6, 214.8, 218.7, 226.2, 233.7, 237.7, 241, 249.9, 254.2, 261.8,
    263.9, 269.3, 281.3, 289, 302, 330, 357.3, 16.3, 36.6, 48.96,
    60.41, 69.93, 72.3, 81.57,
)
SHEET2_STEP_HZ = 0.25e6

NOMINAL = {"c403": 330e-12, "c404": 33e-12, "c405": 470e-12, "c406": 100e-12,
           "c407": 270e-12, "l401": 680e-9, "l402": 560e-9}
NAMES = tuple(NOMINAL)
R410_OHM = 47.0
AMPLIFIER_POLE_HZ = 250e6 / 8.5
SHEET_LEVEL_DB = 20 * math.log10(0.2 * 8.5 / 2.0)  # 200 mV p-p × 8.5 into 2 V p-p


def ladder(f, c):
    s = 2j * np.pi * f
    par = lambda a, b: a * b / (a + b)
    z7 = 1 / (s * c["c407"])
    zbc = par(s * c["l402"], 1 / (s * c["c406"]))
    zb = par(1 / (s * c["c405"]), zbc + z7)
    zab = par(s * c["l401"], 1 / (s * c["c404"]))
    za = par(1 / (s * c["c403"]), zab + zb)
    va = za / (R410_OHM + za)
    return va * zb / (zab + zb) * z7 / (zbc + z7)


def first_order(f):
    s = 2j * np.pi * f
    coupling = s * 50e-6 / (1 + s * 50e-6)
    amplifier = (1 + 1500 / (200 + 1 / (s * 100e-9))) / 8.5
    return coupling * amplifier


def chain(f, components):
    return first_order(f) * ladder(f, components) / (1 + 1j * f / AMPLIFIER_POLE_HZ)


def components_of(scales):
    return {n: NOMINAL[n] * k for n, k in zip(NAMES, scales)}


F1 = np.array([p[0] for p in SHEET1], dtype=float)
M1 = np.array([p[1] for p in SHEET1])
USE1 = F1 <= 17.9e6  # 21.5 MHz is the instrument's floor
FP = np.arange(1, len(SHEET2_LAG_DEG) + 1) * SHEET2_STEP_HZ
LAG = np.unwrap(np.radians(SHEET2_LAG_DEG))
USE2 = FP <= 15e6


def residuals(x):
    c = components_of(x[:7])
    gain_db, delay_ns = x[7], x[8]
    model = SHEET_LEVEL_DB + gain_db + 20 * np.log10(np.abs(chain(F1[USE1], c)))
    lag = -np.unwrap(np.angle(chain(FP, c))) + 2 * np.pi * FP * delay_ns * 1e-9
    return np.concatenate([(model - M1[USE1]) / 0.1,
                           np.degrees(lag - LAG)[USE2] / 2.0])


def fit():
    best = None
    for seed in range(12):
        rng = np.random.default_rng(seed)
        x0 = np.concatenate([rng.uniform(0.85, 1.15, 7), [0.0, 11.0]])
        solution = optimize.least_squares(
            residuals, x0, bounds=([0.4] * 7 + [-0.5, 0], [2.5] * 7 + [0.5, 30]))
        if best is None or solution.cost < best.cost:
            best = solution
    return best.x


def main() -> int:
    x = fit()
    rounded = {n: float(f"{v:.4g}") for n, v in components_of(x[:7]).items()}
    x_r = np.concatenate([[rounded[n] / NOMINAL[n] for n in NAMES], x[7:]])
    c = components_of(x_r[:7])
    model = SHEET_LEVEL_DB + x_r[7] + 20 * np.log10(np.abs(chain(F1, c)))
    mag = (M1 - model)[USE1]
    lag = -np.unwrap(np.angle(chain(FP, c))) + 2 * np.pi * FP * x_r[8] * 1e-9
    phase = np.degrees(lag - LAG)
    print("Effective components (4 significant figures):")
    for n in NAMES:
        unit, scale = ("pF", 1e12) if n.startswith("c") else ("nH", 1e9)
        print(f"  {n.upper()}: {rounded[n] * scale:.4g} {unit} ({rounded[n] / NOMINAL[n]:.3f} × nominal)")
    print(f"Gain offset {x_r[7]:+.3f} dB; measurement delay {x_r[8]:.2f} ns")
    print(f"Sheet 1 residual to 17.8 MHz: rms {np.sqrt(np.mean(mag ** 2)):.3f} dB, "
          f"max {np.max(np.abs(mag)):.3f} dB")
    for limit in (13e6, 16e6):
        sel = FP <= limit
        print(f"Sheet 2 residual to {limit / 1e6:.0f} MHz: rms "
              f"{np.sqrt(np.mean(phase[sel] ** 2)):.2f}°, max {np.max(np.abs(phase[sel])):.2f}°")
    return 0


if __name__ == "__main__":
    sys.exit(main())
