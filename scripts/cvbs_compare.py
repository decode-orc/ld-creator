# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Round-trip comparator: decoded NTSC CVBS against the videosynth truth.

Implements docs-tech/user/ntsc-round-trip-metrics.md (plan Task 7.2):
alignment by VBI picture number, the level, chroma, frequency-response,
timing and VBI measurements, and their thresholds.

Both files are read through their CVBS metadata (the CVBS file format
specification), and the VBI is decoded here from IEC 60857 §10. Nothing is
taken from ld-decode except its output files, and nothing from ld-creator.

    python3 -E scripts/cvbs_compare.py <truth.cvbs> <decoded.cvbs> [--json out]
"""

from __future__ import annotations

import argparse
import json
import math
import sqlite3
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from scipy import optimize, signal

# ---------------------------------------------------------------------------
# NTSC 4fsc stored frames (CVBS spec, video-standard-presets.md, "NTSC" and
# "Stored Frame and Line Origin")
# ---------------------------------------------------------------------------

FSC_HZ = 315e6 / 88
RATE_HZ = 4 * FSC_HZ
SAMPLE_US = 1e6 / RATE_HZ
LINE_SAMPLES = 910
FRAME_LINES = 525
LINE_US = LINE_SAMPLES * SAMPLE_US
# 0H falls 57/90 of a sample before the first stored sample of each line
# (SMPTE 244M §3.1-§3.2, as restated by the CVBS spec).
ORIGIN_SAMPLES = 57 / 90

# NTSC 10-bit levels (SMPTE 244M-2003 §4.2.1): blanking 0 IRE, white 100 IRE.
BLANKING_10BIT = 240
WHITE_10BIT = 800
IRE_PER_CODE = 100.0 / (WHITE_10BIT - BLANKING_10BIT)

# Lines with a normal horizontal sync pulse, as 1-based frame lines: field 1
# lines 10-262 and field 2 lines 273-525 (SMPTE 170M Figure 6; lines 1-9 and
# 263-272 carry equalising and broad pulses).
NORMAL_SYNC_LINES = list(range(10, 263)) + list(range(273, 526))
# Active picture lines scored by P1, clear of the VITS and the half lines.
ACTIVE_LINES = list(range(22, 263)) + list(range(285, 525))


@dataclass
class CvbsFile:
    """An NTSC composite CVBS file as frames of IRE values."""

    path: Path
    encoding: str
    frame_count: int
    _words: np.ndarray

    def frame(self, index: int) -> np.ndarray:
        """Frame `index` as a (525, 910) array of IRE."""
        w = self._words[index].astype(np.float64)
        if self.encoding == "CVBS_S16_4FSC":
            codes = w / 32.0 + BLANKING_10BIT
        elif self.encoding == "CVBS_U16_4FSC":
            codes = w / 64.0
        else:  # CVBS_U10_4FSC
            codes = w
        return (codes - BLANKING_10BIT) * IRE_PER_CODE


def read_cvbs(path) -> CvbsFile:
    """Opens a composite NTSC `.cvbs` and its `.meta`."""
    path = Path(path)
    meta = sqlite3.connect(f"file:{path.with_suffix('.meta')}?mode=ro", uri=True)
    try:
        preset, encoding, signal_type = meta.execute(
            "SELECT preset, sample_encoding_preset, signal_type FROM cvbs_file"
        ).fetchone()
    finally:
        meta.close()
    if preset != "NTSC" or signal_type != "composite":
        raise ValueError(f"{path}: {preset} {signal_type}; only NTSC composite is compared")
    dtypes = {"CVBS_S16_4FSC": "<i2", "CVBS_U16_4FSC": "<u2", "CVBS_U10_4FSC": "<u2"}
    if encoding not in dtypes:
        raise ValueError(f"{path}: sample encoding {encoding} is not supported")
    raw = np.memmap(path, dtype=dtypes[encoding], mode="r")
    frame_samples = FRAME_LINES * LINE_SAMPLES
    count = raw.size // frame_samples
    words = raw[: count * frame_samples].reshape(count, FRAME_LINES, LINE_SAMPLES)
    return CvbsFile(path, encoding, count, words)


def window(t_begin_us: float, t_end_us: float, whole_cycles: bool = True) -> slice:
    """Samples of a line whose times from 0H lie in [t_begin, t_end].

    With `whole_cycles`, the window is cut to a whole number of subcarrier
    cycles (4 samples), so its mean holds no chroma.
    """
    first = math.ceil(t_begin_us / SAMPLE_US - ORIGIN_SAMPLES)
    last = math.floor(t_end_us / SAMPLE_US - ORIGIN_SAMPLES)
    count = last - first + 1
    if whole_cycles:
        count -= count % 4
    return slice(first, first + count)


def sample_times_us(s: slice) -> np.ndarray:
    return (np.arange(s.start, s.stop) + ORIGIN_SAMPLES) * SAMPLE_US


def row(line: int) -> int:
    """Array row of a 1-based frame line."""
    return line - 1


def level(line: np.ndarray, t_begin_us: float, t_end_us: float,
          whole_cycles: bool = True) -> float:
    return float(line[window(t_begin_us, t_end_us, whole_cycles)].mean())


def fsc_phasor(line: np.ndarray, t_begin_us: float, t_end_us: float) -> complex:
    """Complex amplitude of the subcarrier over a window.

    x[n] ≈ |A|·cos(π·n/2 + arg A), with n the stored sample index, so phasors
    from different windows of a line share one phase reference. Exact for a
    whole number of cycles at 4fsc.
    """
    s = window(t_begin_us, t_end_us)
    x = line[s] - line[s].mean()
    n = np.arange(s.start, s.stop)
    return complex(2.0 / x.size * np.sum(x * np.exp(-0.5j * np.pi * n)))


def tone_amplitude(line: np.ndarray, t_begin_us: float, t_end_us: float,
                   frequency_hz: float) -> float:
    """Least-squares amplitude of c + a·cos(ωt) + b·sin(ωt) over a window."""
    s = window(t_begin_us, t_end_us, whole_cycles=False)
    w = 2 * np.pi * frequency_hz * sample_times_us(s) * 1e-6
    design = np.column_stack([np.ones_like(w), np.cos(w), np.sin(w)])
    (_, a, b), *_ = np.linalg.lstsq(design, line[s], rcond=None)
    return math.hypot(a, b)


def wrap_degrees(angle: float) -> float:
    return (angle + 180.0) % 360.0 - 180.0


# ---------------------------------------------------------------------------
# VBI decoding (IEC 60857 §10)
# ---------------------------------------------------------------------------

DECISION_IRE = 50.0  # midway between the 0 and 100 IRE digital levels


def _first_rise(line: np.ndarray, after_us: float) -> float | None:
    """Time (µs from 0H) of the first upward crossing of 50 IRE after a time."""
    start = window(after_us, LINE_US, whole_cycles=False).start
    x = line[start:]
    above = x >= DECISION_IRE
    rises = np.flatnonzero(~above[:-1] & above[1:])
    if rises.size == 0:
        return None
    k = int(rises[0])
    frac = (DECISION_IRE - x[k]) / (x[k + 1] - x[k])
    return (start + k + frac + ORIGIN_SAMPLES) * SAMPLE_US


def decode_biphase(line: np.ndarray) -> int | None:
    """The 24-bit biphase code of a line (§10.1, Figure 11), or None.

    Bit cells are 2 µs; a rise at the cell centre is a 1 and a fall a 0. The
    key's first bit is 1, so the first rise after blanking is the centre of
    bit 1, and every later centre follows at 2 µs.
    """
    centre = _first_rise(line, 8.0)
    if centre is None:
        return None
    value = 0
    for k in range(24):
        c = centre + 2.0 * k
        if c + 0.8 > LINE_US:
            return None
        before = level(line, c - 0.8, c - 0.2, False)
        after = level(line, c + 0.2, c + 0.8, False)
        if abs(after - before) < DECISION_IRE:
            return None
        value = (value << 1) | int(after > before)
    return value if value >> 23 else None


FM_CLOCK_BITS = (0, 0, 1, 1)
FM_LEADING_BITS = (1, 1, 1, 0, 0, 1, 0)
FM_TRAILING_BITS = (0, 0, 0, 1, 1, 0, 1)


@dataclass(frozen=True)
class FmCode:
    field_one: bool
    number: int


def decode_fm(line: np.ndarray) -> FmCode | None:
    """The 40-bit FM code of a line (§10.2, Figure 13), or None.

    Bit cells are 1 µs, with a transition at every cell boundary; a further
    transition at the cell centre is a 1. The code starts from blanking, so
    the first rise after the back porch is the start of bit 1. The clock,
    recognition and parity bits are checked.
    """
    start = _first_rise(line, 8.0)
    if start is None:
        return None
    bits = []
    for k in range(40):
        s = start + k
        if s + 0.9 > LINE_US:
            return None
        first = level(line, s + 0.15, s + 0.4, False) >= DECISION_IRE
        second = level(line, s + 0.6, s + 0.85, False) >= DECISION_IRE
        bits.append(int(first != second))
    if (tuple(bits[0:4]) != FM_CLOCK_BITS or tuple(bits[5:12]) != FM_LEADING_BITS
            or tuple(bits[33:40]) != FM_TRAILING_BITS):
        return None
    if sum(bits[12:33]) % 2 != 1:  # odd parity over data and parity bit
        return None
    digits = []
    for d in range(5):  # X5 (bits 13-16) first, each LSB first
        nibble = bits[12 + 4 * d: 16 + 4 * d]
        digits.append(sum(b << i for i, b in enumerate(nibble)))
    if any(d > 9 for d in digits):
        return None
    number = sum(d * 10 ** i for i, d in enumerate(digits))
    return FmCode(bool(bits[4]), number)


def white_flag(line: np.ndarray) -> bool:
    """The 100 IRE white flag (§10.2.1, Figure 12: 0.160H to 0.950H)."""
    return level(line, 0.25 * LINE_US, 0.85 * LINE_US) >= DECISION_IRE


BIPHASE_LINES = (16, 17, 18, 279, 280, 281)
FM_LINES = (10, 273)
FLAG_LINES = (11, 274)
LEAD_IN_CODE = 0x88FFFF


@dataclass
class FrameVbi:
    biphase: dict
    fm: dict
    flags: dict
    picture: int | None = None   # CAV picture number (§10.1.3)
    lead_in: bool = False
    first_field: int | None = None


def bcd_picture_number(code: int) -> int | None:
    """Picture number of an FX₁X₂X₃X₄X₅ code (§10.1.3), or None."""
    if code >> 20 != 0xF:
        return None
    digits = [(code >> (4 * i)) & 0xF for i in range(5)]
    digits[4] &= 0x7  # X₁'s first bit is the early picture-stop flag (Note)
    if any(d > 9 for d in digits):
        return None
    return sum(d * 10 ** i for i, d in enumerate(digits))


def read_vbi(frame: np.ndarray) -> FrameVbi:
    vbi = FrameVbi(
        biphase={ln: decode_biphase(frame[row(ln)]) for ln in BIPHASE_LINES},
        fm={ln: decode_fm(frame[row(ln)]) for ln in FM_LINES},
        flags={ln: white_flag(frame[row(ln)]) for ln in FLAG_LINES},
    )
    for first_field, lines in ((1, (17, 18)), (2, (280, 281))):
        numbers = {bcd_picture_number(vbi.biphase[ln]) for ln in lines
                   if vbi.biphase[ln] is not None}
        numbers.discard(None)
        if len(numbers) == 1:
            vbi.picture = numbers.pop()
            vbi.first_field = first_field
            break
    vbi.lead_in = all(vbi.biphase[ln] == LEAD_IN_CODE for ln in (17, 18, 280, 281))
    return vbi


# ---------------------------------------------------------------------------
# Measurement windows (µs from 0H). Positions from the definitions the pinned
# videosynth renders (src/vits_definition_provider.cpp; NTC Report No. 7,
# SMPTE RP 168, IEC 60857 Figures 6b and 7), each kept clear of its
# transitions by at least 0.4 µs plus the transition time.
# ---------------------------------------------------------------------------

SYNC_TIP = (1.0, 3.7)        # sync 0-4.7 µs (SMPTE 170M)
BACK_PORCH = (8.2, 9.0)      # after the burst (5.3-7.8 µs), before 9.15 µs
BURST = (5.7, 7.4)

VIRS_LINES = (19, 282)       # VIRS, 12-60 µs (IEC 60857 Figure 6b)
VIRS_CHROMA = (14.0, 34.0)   # chroma reference 12-36 µs (1 µs ramps)
VIRS_LUMINANCE = (36.7, 47.3)  # luminance reference 36-48 µs
VIRS_BLACK = (48.7, 59.3)    # black reference 48-60 µs

COMPOSITE_LINE = 20          # NTC-7 composite
WHITE_BAR = (14.0, 28.0)     # 12-30 µs
PULSE_2T_CENTRE = 34.0       # 33.75-34.25 µs, HAD 250 ns
PULSE_2T_BASELINE = (31.0, 33.3)
PULSE_12T5 = (35.0, 39.0)    # 35.4-38.6 µs, HAD 1.5625 µs
# Modulated staircase: six treads, risers 46.2 + 2.9·k µs (k = 0-4), chroma
# reference 42-60 µs riding on every tread.
STAIRCASE_TREADS = ((43.0, 45.6), (46.9, 48.8), (49.8, 51.7), (52.7, 54.6),
                    (55.6, 57.5), (58.5, 59.6))

COMBINATION_LINE = 283       # NTC-7 combination: multiburst on 50 IRE
MULTIBURST = ((0.5e6, (18.6, 22.4)), (1.0e6, (24.5, 26.5)), (2.0e6, (28.5, 30.5)),
              (3.0e6, (32.5, 34.5)), (FSC_HZ, (36.5, 38.5)), (4.2e6, (40.5, 42.5)))

ACTIVE = (11.0, 61.5)


@dataclass
class Metric:
    key: str
    description: str
    unit: str
    threshold: float | None      # None: reported, not judged
    values: list = field(default_factory=list)  # (frame label, value)

    def add(self, label, value):
        self.values.append((label, float(value)))

    def summary(self) -> dict:
        v = np.array([x for _, x in self.values]) if self.values else np.array([])
        out = {"key": self.key, "description": self.description, "unit": self.unit,
               "threshold": self.threshold, "count": int(v.size)}
        if v.size:
            out.update(min=float(v.min()), mean=float(v.mean()), max=float(v.max()))
            if self.threshold is not None:
                bad = {lab for lab, x in self.values if abs(x) > self.threshold}
                out["frames_outside"] = len(bad)
                out["verdict"] = "confirmed" if not bad else "refuted"
            else:
                out["verdict"] = "reported"
        else:
            out["verdict"] = "no data"
        return out


def metric_table() -> dict:
    m = [
        Metric("L1", "Blanking level (back porch), all normal-sync lines", "IRE", 1.0),
        Metric("L2", "Sync tip level, all normal-sync lines", "IRE", 1.0),
        Metric("L3b", "VIRS black reference, lines 19/282", "IRE", 1.0),
        Metric("L3l", "VIRS luminance reference, lines 19/282", "IRE", 1.0),
        Metric("L4", "NTC-7 white bar, line 20", "IRE", 1.0),
        Metric("L5", "NTC-7 staircase treads, line 20", "IRE", 1.0),
        Metric("C1", "VIRS chroma amplitude, decoded/truth − 1", "%", 2.0),
        Metric("C2", "VIRS chroma phase relative to burst", "deg", 1.0),
        Metric("C3a", "Staircase chroma amplitude (DG), decoded/truth − 1", "%", 2.0),
        Metric("C3p", "Staircase chroma phase (DP) relative to burst", "deg", 1.0),
    ]
    for f, _ in MULTIBURST:
        name = "3.58" if f == FSC_HZ else f"{f / 1e6:g}"
        m.append(Metric(f"F1 {name}", f"Multiburst {name} MHz, line 283", "dB",
                        None if f > FSC_HZ else 0.5))
    m += [
        # Reported, not judged: the truth's 2T is not band-limited (energy to
        # 7 MHz), so any decoder that band-limits to the video band changes
        # its peak by several percent (metrics document, F2).
        Metric("F2a", "2T pulse amplitude, decoded/truth − 1", "%", None),
        Metric("F2d", "2T pulse half-amplitude duration difference", "ns", None),
        Metric("F3g", "12.5T chroma-to-luma gain, decoded/truth − 1", "%", 2.0),
        Metric("F3d", "12.5T chroma-to-luma delay difference", "ns", 15.0),
        Metric("T1", "0H position difference, all normal-sync lines", "ns", None),
        Metric("T2", "Burst phase at 0H (SC/H) difference", "deg", None),
        Metric("V1", "FM codes, lines 10/273: lines wrong", "lines", 0.0),
        Metric("V2", "White flags, lines 11/274: lines wrong", "lines", 0.0),
        Metric("V3", "Biphase codes, lines 16-18/279-281: lines wrong", "lines", 0.0),
        Metric("P1", "Active picture RMS difference", "IRE", None),
    ]
    return {x.key: x for x in m}


# ---------------------------------------------------------------------------
# Measurements on one pair of frames
# ---------------------------------------------------------------------------


def _levels(frame, lines, t):
    s = window(*t)
    return frame[[row(ln) for ln in lines], s].mean(axis=1)


def _zero_h(frame: np.ndarray, lines) -> np.ndarray:
    """0H of each line, in samples from its sample 0 (negative: before it).

    The 50 % point between the line's own sync tip and blanking levels on the
    falling edge, linearly interpolated.
    """
    flat = frame.reshape(-1)
    tips = _levels(frame, lines, SYNC_TIP)
    blanks = _levels(frame, lines, BACK_PORCH)
    out = np.empty(len(lines))
    for i, ln in enumerate(lines):
        base = row(ln) * LINE_SAMPLES
        seg = flat[base - 4: base + 4]
        half = 0.5 * (tips[i] + blanks[i])
        k = np.flatnonzero((seg[:-1] > half) & (seg[1:] <= half))
        if k.size == 0:
            out[i] = np.nan
            continue
        k = int(k[-1])
        out[i] = k - 4 + (seg[k] - half) / (seg[k] - seg[k + 1])
    return out


def _pulse_2t(line):
    base = level(line, *PULSE_2T_BASELINE)
    s = window(PULSE_2T_CENTRE - 1.2, PULSE_2T_CENTRE + 1.2, whole_cycles=False)
    up = 16
    y = signal.resample_poly(line[s] - base, up, 1)
    t = (s.start + ORIGIN_SAMPLES + np.arange(y.size) / up) * SAMPLE_US
    k = int(np.argmax(y))
    peak = y[k]
    half = 0.5 * peak
    left = k - np.flatnonzero(y[:k][::-1] < half)[0]
    right = k + np.flatnonzero(y[k:] < half)[0]
    tl = np.interp(half, [y[left - 1], y[left]], [t[left - 1], t[left]])
    tr = np.interp(half, [y[right], y[right - 1]], [t[right], t[right - 1]])
    return peak, (tr - tl) * 1e3


def _sin_squared(t_us, centre_us, had_us):
    x = (t_us - centre_us) / (2 * had_us)
    return np.where(np.abs(x) < 0.5, np.cos(np.pi * x) ** 2, 0.0)


PULSE_12T5_CENTRE = 37.0
PULSE_12T5_HAD = 1.5625  # 12.5 T, T = 125 ns


def _pulse_12t5(line):
    """Luma amplitude, chroma amplitude, luma and chroma centres (µs).

    Least-squares fit of the NTC-7 modulated pulse model over the pulse:
    c + a_y·S(t − t_y; h_y) + a_c·S(t − t_c; h_c)·cos(ω_sc·t + φ), with S a
    sin² pulse of half-amplitude duration h. Fitting both components
    together keeps each out of the other's estimate, which a filter
    separation does not when chroma and luma are offset in time.
    """
    s = window(*PULSE_12T5, whole_cycles=False)
    t = sample_times_us(s)
    x = line[s]
    w = 2 * np.pi * FSC_HZ * 1e-6

    def model(p):
        c, ay, ty, hy, ac, tc, hc, phi = p
        return (c + ay * _sin_squared(t, ty, hy)
                + ac * _sin_squared(t, tc, hc) * np.cos(w * t + phi))

    n = np.arange(s.start, s.stop)
    phi0 = float(np.angle(np.sum(x * np.exp(-0.5j * np.pi * n))))
    phi0 -= w * ORIGIN_SAMPLES * SAMPLE_US  # phasor phase at t = 0H
    best = None
    for phi in (phi0, phi0 + np.pi):
        p0 = [0.0, x.max() / 2, PULSE_12T5_CENTRE, PULSE_12T5_HAD,
              x.max() / 2, PULSE_12T5_CENTRE, PULSE_12T5_HAD, phi]
        fit = optimize.least_squares(lambda p: model(p) - x, p0, x_scale="jac")
        if best is None or fit.cost < best.cost:
            best = fit
    _, ay, ty, _, ac, tc, _, _ = best.x
    return ay, abs(ac), ty, tc


def measure_pair(label, truth, decoded, metrics, score_signal: bool):
    """Adds one frame pair's measurements to `metrics`."""
    tv, dv = read_vbi(truth), read_vbi(decoded)
    metrics["V1"].add(label, sum(tv.fm[ln] != dv.fm[ln] for ln in FM_LINES))
    metrics["V2"].add(label, sum(tv.flags[ln] != dv.flags[ln] for ln in FLAG_LINES))
    metrics["V3"].add(label, sum(tv.biphase[ln] != dv.biphase[ln] for ln in BIPHASE_LINES))
    if not score_signal:
        return

    def each(key, values):
        for v in np.atleast_1d(values):
            metrics[key].add(label, v)

    lines = NORMAL_SYNC_LINES
    each("L1", _levels(decoded, lines, BACK_PORCH) - _levels(truth, lines, BACK_PORCH))
    each("L2", _levels(decoded, lines, SYNC_TIP) - _levels(truth, lines, SYNC_TIP))
    each("L3b", _levels(decoded, VIRS_LINES, VIRS_BLACK) - _levels(truth, VIRS_LINES, VIRS_BLACK))
    each("L3l", _levels(decoded, VIRS_LINES, VIRS_LUMINANCE)
         - _levels(truth, VIRS_LINES, VIRS_LUMINANCE))
    tl, dl = truth[row(COMPOSITE_LINE)], decoded[row(COMPOSITE_LINE)]
    each("L4", level(dl, *WHITE_BAR) - level(tl, *WHITE_BAR))
    for tread in STAIRCASE_TREADS:
        each("L5", level(dl, *tread) - level(tl, *tread))

    def chroma(line, t):
        c, b = fsc_phasor(line, *t), fsc_phasor(line, *BURST)
        return abs(c), math.degrees(np.angle(c / b))

    for ln in VIRS_LINES:
        (ta, tp), (da, dp) = chroma(truth[row(ln)], VIRS_CHROMA), chroma(decoded[row(ln)], VIRS_CHROMA)
        each("C1", 100 * (da / ta - 1))
        each("C2", wrap_degrees(dp - tp))
    for tread in STAIRCASE_TREADS:
        (ta, tp), (da, dp) = chroma(tl, tread), chroma(dl, tread)
        each("C3a", 100 * (da / ta - 1))
        each("C3p", wrap_degrees(dp - tp))

    tc, dc = truth[row(COMBINATION_LINE)], decoded[row(COMBINATION_LINE)]
    for f, t in MULTIBURST:
        name = "3.58" if f == FSC_HZ else f"{f / 1e6:g}"
        each(f"F1 {name}", 20 * math.log10(tone_amplitude(dc, *t, f) / tone_amplitude(tc, *t, f)))

    (tpk, thad), (dpk, dhad) = _pulse_2t(tl), _pulse_2t(dl)
    each("F2a", 100 * (dpk / tpk - 1))
    each("F2d", dhad - thad)
    ty, tcp, tty, ttc = _pulse_12t5(tl)
    dy, dcp, dty, dtc = _pulse_12t5(dl)
    each("F3g", 100 * ((dcp / dy) / (tcp / ty) - 1))
    each("F3d", 1e3 * ((dtc - dty) - (ttc - tty)))

    th, dh = _zero_h(truth, lines), _zero_h(decoded, lines)
    each("T1", (dh - th) * SAMPLE_US * 1e3)
    sch = []
    for i, ln in enumerate(lines):
        bt, bd = fsc_phasor(truth[row(ln)], *BURST), fsc_phasor(decoded[row(ln)], *BURST)
        # Phase at sample position s is π·s/2 + arg A; evaluate at 0H.
        pt = math.degrees(np.angle(bt)) + 90.0 * th[i]
        pd = math.degrees(np.angle(bd)) + 90.0 * dh[i]
        sch.append(wrap_degrees(pd - pt))
    each("T2", sch)

    s = window(*ACTIVE, whole_cycles=False)
    rows = [row(ln) for ln in ACTIVE_LINES]
    each("P1", math.sqrt(np.mean((decoded[rows, s] - truth[rows, s]) ** 2)))


# ---------------------------------------------------------------------------
# Alignment (metrics document, "Alignment") and the comparison
# ---------------------------------------------------------------------------


@dataclass
class Comparison:
    metrics: dict
    pairs: list
    discontinuities: list
    truth_frames: int
    decoded_frames: int
    offset_ire: float = 0.0

    def summaries(self):
        return [m.summary() for m in self.metrics.values()]

    def verdict(self) -> str:
        judged = [s["verdict"] for s in self.summaries() if s["threshold"] is not None]
        return "refuted" if "refuted" in judged or "no data" in judged else "confirmed"

    def to_json(self) -> dict:
        return {"verdict": self.verdict(), "truth_frames": self.truth_frames,
                "decoded_frames": self.decoded_frames, "offset_ire": self.offset_ire,
                "pairs": self.pairs, "discontinuities": self.discontinuities,
                "metrics": self.summaries(),
                "values": {k: m.values for k, m in self.metrics.items()}}


def _pair_frames(truth_vbi, decoded_vbi):
    """[(truth index, decoded index, label, programme?)] and discontinuities."""
    notes = []
    t_numbers = {v.picture: i for i, v in enumerate(truth_vbi) if v.picture is not None}
    t_first = min(t_numbers.values()) if t_numbers else len(truth_vbi)
    d_first = next((i for i, v in enumerate(decoded_vbi) if v.picture is not None), None)
    pairs = []
    if d_first is None:
        notes.append("no decoded frame carries a picture number")
        return pairs, notes
    for i in range(d_first):  # lead-in, by position before the first programme frame
        t = t_first - (d_first - i)
        if t >= 0:
            pairs.append((t, i, f"lead-in {t}", False))
    first_lead_in = max(t_first - d_first, 0)
    if first_lead_in:
        notes.append(f"lead-in frames 0-{first_lead_in - 1} not decoded "
                     "(before the first decoded frame)")
    seen = {}
    for i in range(d_first, len(decoded_vbi)):
        v = decoded_vbi[i]
        if v.picture is None:
            notes.append(f"decoded frame {i}: no valid picture number")
            continue
        if v.first_field != 1:
            notes.append(f"decoded frame {i}: picture {v.picture} starts on field {v.first_field}")
        if v.picture in seen:
            notes.append(f"decoded frame {i}: picture {v.picture} repeats frame {seen[v.picture]}")
            continue
        seen[v.picture] = i
        if v.picture not in t_numbers:
            notes.append(f"decoded frame {i}: picture {v.picture} is not in the truth")
            continue
        pairs.append((t_numbers[v.picture], i, f"picture {v.picture}", True))
    missing = sorted(set(t_numbers) - set(seen))
    if missing:
        last_seen = max(seen) if seen else 0
        tail = [n for n in missing if n > last_seen]
        inner = [n for n in missing if n <= last_seen]
        if inner:
            notes.append(f"pictures missing inside the decoded range: {inner}")
        if tail:
            notes.append(f"pictures {tail[0]}-{tail[-1]} not decoded (after the last decoded frame)")
    return pairs, notes


def compare(truth: CvbsFile, decoded: CvbsFile, offset_ire: float = 0.0) -> Comparison:
    """Aligns and measures. `offset_ire` is added to every decoded sample (the
    comparator's own check, plan Task 7.2)."""
    t_frames = [truth.frame(i) for i in range(truth.frame_count)]
    d_vbi = []
    for i in range(decoded.frame_count):
        d_vbi.append(read_vbi(decoded.frame(i) + offset_ire))
    pairs, notes = _pair_frames([read_vbi(f) for f in t_frames], d_vbi)
    metrics = metric_table()
    for t, d, label, programme in pairs:
        measure_pair(label, t_frames[t], decoded.frame(d) + offset_ire, metrics, programme)
    return Comparison(metrics, [(t, d, lab) for t, d, lab, _ in pairs], notes,
                      truth.frame_count, decoded.frame_count, offset_ire)


def _fmt(x, unit):
    if unit in ("lines",):
        return f"{x:.0f}"
    return f"{x:+.3f}" if abs(x) < 100 else f"{x:+.1f}"


def markdown_table(comparison: Comparison) -> str:
    lines = ["| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |",
             "|---|-------------|------|-----------|-----|------|-----|----------------|---------|"]
    for s in comparison.summaries():
        thr = "reported" if s["threshold"] is None else f"±{s['threshold']:g}"
        if s["count"]:
            vals = [_fmt(s[k], s["unit"]) for k in ("min", "mean", "max")]
        else:
            vals = ["—"] * 3
        outside = str(s.get("frames_outside", "—"))
        lines.append(f"| {s['key']} | {s['description']} | {s['unit']} | {thr} | "
                     + " | ".join(vals) + f" | {outside} | {s['verdict']} |")
    return "\n".join(lines)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("truth")
    parser.add_argument("decoded")
    parser.add_argument("--offset-ire", type=float, default=0.0,
                        help="add this to every decoded sample (comparator check)")
    parser.add_argument("--json", help="write the full results here")
    args = parser.parse_args(argv)
    result = compare(read_cvbs(args.truth), read_cvbs(args.decoded), args.offset_ire)
    print(markdown_table(result))
    for note in result.discontinuities:
        print(f"- {note}")
    print(f"Verdict: {result.verdict()}")
    if args.json:
        Path(args.json).write_text(json.dumps(result.to_json(), indent=1))
    return 0 if result.verdict() == "confirmed" else 1


if __name__ == "__main__":
    sys.exit(main())
