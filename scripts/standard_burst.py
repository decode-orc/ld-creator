# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Copy an NTSC truth with its colour bursts replaced by the standard burst.

A bisection tool for the round trip (ld-decode-findings.md, "The colour
burst"). The pinned videosynth renders a burst of about 7.3 cycles at its
50 % points (videosynth#32); IEC 60857 §9.1.2 calls for the EIA burst, which
ITU-R BT.470-6 Table 2 items 2.14g/h give for M/NTSC as a start 5.3 µs after
0H and a duration of 9 cycles, both at the envelope's 50 % points. This
writes a copy of the truth in which every line's burst has that start and
duration, the original's amplitude and phase, and sin² edges, and changes
nothing else. Running the copy through the round trip isolates the burst's
effect.

    python3 -E -B scripts/standard_burst.py <truth.cvbs> <out.cvbs>

The output is CVBS_S16_4FSC; the input's `.meta` is copied beside it.
"""

from __future__ import annotations

import argparse
import math
import shutil
import sqlite3
import sys
from pathlib import Path

import numpy as np

import cvbs_compare as cc

# ITU-R BT.470-6 Table 2 item 2.14g/h, M/NTSC: 50 % points.
BURST_START_US = 5.3
BURST_CYCLES = 9
# SMPTE 170M-2004 Table 2: burst envelope rise 300 ns (+200/−100), 10-90 %.
# A sin² edge sin²(π·u/2), u from 0 to 1 over its span, passes 10 % and 90 %
# at u = (2/π)·asin(√0.1) and (2/π)·asin(√0.9), so its 10-90 % rise is
# (2/π)·(asin(√0.9) − asin(√0.1)) = 0.5903 of the span.
RISE_10_90_US = 0.300
EDGE_US = RISE_10_90_US / ((2 / math.pi) * (math.asin(math.sqrt(0.9))
                                            - math.asin(math.sqrt(0.1))))
# Region rewritten around the burst: the back porch from the end of the
# sync's rising edge to before 9.15 µs (the earliest VITS element). The
# new burst's edges (5.05-5.55 µs, 7.56-8.06 µs) lie inside it.
REGION_US = (4.9, 8.3)
# A line carries a burst when its burst-window subcarrier exceeds this (IRE).
BURST_PRESENT_IRE = 10.0

# CVBS_S16_4FSC (CVBS spec): int16 = (code − 240)·32, code = 240 + 5.6·IRE.
S16_PER_IRE = 5.6 * 32


def envelope(t_us: np.ndarray) -> np.ndarray:
    end_us = BURST_START_US + BURST_CYCLES / cc.FSC_HZ * 1e6

    def ramp(x):
        u = np.clip(x / EDGE_US + 0.5, 0.0, 1.0)
        return np.sin(0.5 * np.pi * u) ** 2

    return ramp(t_us - BURST_START_US) * ramp(end_us - t_us)


def standard_burst(frame: np.ndarray) -> tuple[np.ndarray, int]:
    """The frame (IRE) with every burst replaced; and the number replaced."""
    n = np.arange(cc.LINE_SAMPLES)
    t = (n + cc.ORIGIN_SAMPLES) * cc.SAMPLE_US
    region = (t > REGION_US[0]) & (t < REGION_US[1])
    env = envelope(t)
    out = frame.copy()
    replaced = 0
    for r in range(cc.FRAME_LINES):
        b = cc.fsc_phasor(frame[r], *cc.BURST)
        if abs(b) <= BURST_PRESENT_IRE:
            continue
        blank = cc.level(frame[r], *cc.BACK_PORCH)
        burst = abs(b) * env * np.cos(0.5 * np.pi * n + np.angle(b))
        out[r, region] = blank + burst[region]
        replaced += 1
    return out, replaced


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("truth", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    src = cc.read_cvbs(args.truth)
    replaced = 0
    with args.output.open("wb") as out:
        for k in range(src.frame_count):
            frame, count = standard_burst(src.frame(k))
            replaced += count
            words = np.clip(np.round(frame * S16_PER_IRE), -32768, 32767).astype("<i2")
            out.write(words.tobytes())
    meta = args.output.with_suffix(".meta")
    shutil.copyfile(args.truth.with_suffix(".meta"), meta)
    with sqlite3.connect(meta) as db:
        db.execute("UPDATE cvbs_file SET sample_encoding_preset = 'CVBS_S16_4FSC'")
    print(f"{replaced} bursts replaced in {src.frame_count} frames → {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
