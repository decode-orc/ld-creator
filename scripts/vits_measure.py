# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Absolute NTSC VITS measurements, without a truth file.

The round-trip comparator scores a decode against its truth. A real disc
has no truth, so this measures the test signals themselves, against their
nominal values, the same way on any source:

- ``cvbs``: a decoded ``.cvbs`` (ld-decode's, the reference decoder's, or
  the videosynth truth);
- ``rf``: raw RF (``.s16`` at 40 MSPS, ld-creator's or a DomesdayDuplicator
  capture cut with ld-cut), demodulated by the IEC 60857 chain of
  ``reference_demod.py`` and line-locked to its own syncs, so no frame
  alignment is needed.

Lines are found by their content, not their number, because discs carry
their VITS on different lines: the NTC-7 composite (white bar, 2T, 12.5T,
modulated staircase), the NTC-7 combination (multiburst) and VIRS. The
measurement windows are the comparator's.

    python3 -E -B scripts/vits_measure.py cvbs <file.cvbs> [--frames N]
    python3 -E -B scripts/vits_measure.py rf <file.s16> [--frames N]
        [--rf-band LOW HIGH] [--playback-equaliser] [--sidebands]

``--sidebands`` adds a least-squares fit of the raw RF's FM sidebands during
the burst and each multiburst packet: the first and second sidebands relative
to the carrier, from which the RF chain's tilt (lower − upper) and the
modulation depth (their sum) follow.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np
from scipy import interpolate

import cvbs_compare as cc
import reference_demod as rd

# Classification thresholds (IRE), well clear of the nominal levels they
# separate: a 100 IRE bar, a 50 IRE VIRS luminance reference, 7.5 IRE black.
BAR_MIN_IRE = 85.0
VIRS_LUMA_IRE = (40.0, 60.0)
VIRS_BLACK_IRE = (-5.0, 15.0)  # 7.5 IRE with setup, 0 without
CHROMA_PRESENT_IRE = 8.0     # subcarrier amplitude (peak) taken as present
FLAG = (12.5, 15.5)          # combination line's white flag, before the 0.5 MHz packet

# RF mode: the FM band passed to the demodulator. The default lower edge is
# above the analogue audio carriers (2.3 and 2.8 MHz, IEC 60857 §9.3), which
# a real disc carries and ld-creator's RF does not yet; the lowest chroma
# sideband used (fc − 3.58 MHz at blanking) is 4.5 MHz.
RF_BAND_HZ = (4.0e6, 18.5e6)
RF_EDGE_HZ = 1.0e6
BLOCK = 1 << 20              # RF samples demodulated per FFT block
PAD = 1 << 14                # discarded at each block edge
RF_PER_OUT = rd.RF_RATE_HZ / rd.OUT_RATE_HZ
LINE_RF = cc.LINE_SAMPLES * RF_PER_OUT
SYNC_SLICE_IRE = -20.0
SYNC_WIDTH_US = (4.2, 5.2)   # normal sync 4.7 µs; equalising 2.3, broad 27
TIMEBASE_HALF = 3            # lines each side in the local timebase fit
TIMEBASE_TOLERANCE = 0.01    # line period within 1 % of nominal


# ---------------------------------------------------------------------------
# Line classification and measurement
# ---------------------------------------------------------------------------


def chroma_ire(line, t):
    return abs(cc.fsc_phasor(line, *t))


def classify(line) -> str | None:
    if (cc.level(line, *cc.WHITE_BAR) > BAR_MIN_IRE
            and abs(cc.level(line, *cc.PULSE_2T_BASELINE)) < VIRS_BLACK_IRE[1]
            and chroma_ire(line, cc.STAIRCASE_TREADS[0]) > CHROMA_PRESENT_IRE):
        return "composite"
    if (cc.level(line, *FLAG) > BAR_MIN_IRE
            and chroma_ire(line, cc.MULTIBURST[4][1]) > CHROMA_PRESENT_IRE):
        return "combination"
    if (VIRS_LUMA_IRE[0] < cc.level(line, *cc.VIRS_LUMINANCE) < VIRS_LUMA_IRE[1]
            and VIRS_BLACK_IRE[0] < cc.level(line, *cc.VIRS_BLACK) < VIRS_BLACK_IRE[1]
            and chroma_ire(line, cc.VIRS_CHROMA) > CHROMA_PRESENT_IRE):
        return "virs"
    return None


class Results:
    def __init__(self):
        self.values: dict[str, list] = {}

    def add(self, key, value):
        self.values.setdefault(key, []).append(float(value))

    def table(self) -> str:
        rows = ["| Quantity | Mean | SD | N |", "|---|---|---|---|"]
        for key, v in self.values.items():
            a = np.array(v)
            if "(deg)" in key:
                # Circular statistics: phases may straddle ±180°.
                m = np.mean(np.exp(1j * np.radians(a)))
                mean = math.degrees(np.angle(m))
                sd = math.degrees(math.sqrt(max(-2 * math.log(max(abs(m), 1e-12)), 0.0)))
            else:
                mean, sd = a.mean(), a.std()
            rows.append(f"| {key} | {mean:.3f} | {sd:.3f} | {a.size} |")
        return "\n".join(rows)


# µs: the burst (5.3-7.8 µs) with its edges, starting after the sync's
# rising edge (4.7 µs plus its 140 ns rise), whose step would read as burst.
BURST_SEARCH = (4.95, 9.5)
BURST_CENTRE_US = 6.55
# µs: the flat top of any burst from 7 cycles up, with 300 ns edges.
BURST_TOP_US = (6.05, 7.05)
BURST_KERNEL = np.array([1.0, 2.0, 2.0, 2.0, 1.0]) / 8.0


def burst_duration_us(line):
    """Burst start and duration (µs) at the envelope's 50 % points.

    BT.470-6 Table 2 items 2.14g/h define both at those points. The
    envelope is 2·|z ∗ k| with z the line demodulated at fsc and k the
    kernel [1, 2, 2, 2, 1]/8, which passes the burst's term with unit gain
    and cancels its image (alternating in sign every sample at 4fsc) up to
    a linear ramp, so the edges are not biased by the burst's phase. The
    50 % level is half the envelope's mean over the burst's flat top.
    """
    s = cc.window(*BURST_SEARCH, whole_cycles=False)
    x = line[s] - cc.level(line, *cc.BACK_PORCH)
    n = np.arange(s.start, s.stop)
    z = x * np.exp(-0.5j * np.pi * n)
    env = 2 * np.abs(np.convolve(z, BURST_KERNEL, mode="valid"))
    t = (n[:env.size] + (BURST_KERNEL.size - 1) / 2 + cc.ORIGIN_SAMPLES) * cc.SAMPLE_US
    top = (t >= BURST_TOP_US[0]) & (t <= BURST_TOP_US[1])
    half = 0.5 * env[top].mean()
    # Walk out from the burst's centre to the first crossing on each side,
    # so noise elsewhere in the window is not taken for an edge.
    c = int(np.argmin(np.abs(t - BURST_CENTRE_US)))
    below = env < half
    if below[c]:
        return None
    left, right = np.flatnonzero(below[:c]), np.flatnonzero(below[c:])
    if left.size == 0 or right.size == 0:
        return None
    i, j = left[-1] + 1, c + right[0] - 1
    start = np.interp(half, env[i - 1:i + 1], t[i - 1:i + 1])
    end = np.interp(half, env[j:j + 2][::-1], t[j:j + 2][::-1])
    return start, end - start


def phase_to_burst(line, t):
    """Chroma phase relative to the line's own burst, folded to ±90°.

    Folded because videosynth's VITS chroma is not locked to the burst: its
    phase relative to the burst alternates by 180° from line to line.
    """
    p = math.degrees(np.angle(cc.fsc_phasor(line, *t) / cc.fsc_phasor(line, *cc.BURST)))
    return (p + 90.0) % 180.0 - 90.0


def measure_line(line, res: Results):
    """Every line: sync, blanking and burst; VITS lines: their signals."""
    tip, blank = cc.level(line, *cc.SYNC_TIP), cc.level(line, *cc.BACK_PORCH)
    if not (-50 < tip < -30 and -5 < blank < 5):
        return
    res.add("Sync tip (IRE)", tip)
    res.add("Blanking (IRE)", blank)
    res.add("Burst (IRE p-p)", 2 * chroma_ire(line, cc.BURST))
    timing = burst_duration_us(line)
    if timing is not None:
        res.add("Burst 50 % start (µs)", timing[0])
        res.add("Burst 50 % duration (cycles)", timing[1] * cc.FSC_HZ * 1e-6)
    kind = classify(line)
    if kind == "virs":
        res.add("VIRS luminance (IRE)", cc.level(line, *cc.VIRS_LUMINANCE))
        res.add("VIRS black (IRE)", cc.level(line, *cc.VIRS_BLACK))
        res.add("VIRS chroma (IRE p-p)", 2 * chroma_ire(line, cc.VIRS_CHROMA))
        res.add("VIRS chroma / burst", chroma_ire(line, cc.VIRS_CHROMA)
                / chroma_ire(line, cc.BURST))
        res.add("VIRS chroma phase − burst, mod 180° (deg)", phase_to_burst(line, cc.VIRS_CHROMA))
    elif kind == "composite":
        res.add("White bar (IRE)", cc.level(line, *cc.WHITE_BAR))
        amps = [chroma_ire(line, t) for t in cc.STAIRCASE_TREADS]
        phases = [phase_to_burst(line, t) for t in cc.STAIRCASE_TREADS]
        res.add("Staircase chroma, mean (IRE p-p)", 2 * np.mean(amps))
        res.add("Staircase chroma / burst", np.mean(amps) / chroma_ire(line, cc.BURST))
        res.add("DG, max − min (% of mean)", 100 * (max(amps) - min(amps)) / np.mean(amps))
        res.add("DP, max − min (deg)", max(phases) - min(phases))
        res.add("DP, top − bottom tread (deg)", cc.wrap_degrees(phases[-1] - phases[0]))
        res.add("Staircase phase − burst, mod 180° (deg)", np.mean(phases))
        peak, had = cc._pulse_2t(line)
        res.add("2T peak (IRE)", peak)
        res.add("2T HAD (ns)", had)
        ay, ac, ty, tc = cc._pulse_12t5(line)
        res.add("12.5T chroma / luma", ac / ay)
        res.add("12.5T chroma − luma delay (ns)", 1e3 * (tc - ty))
    elif kind == "combination":
        # Referred to the 1 MHz packet: packet timings differ between
        # generators, and the 0.5 MHz packet is the one that moves most.
        ref = cc.tone_amplitude(line, *cc.MULTIBURST[1][1], cc.MULTIBURST[1][0])
        for f, t in cc.MULTIBURST[1:]:
            name = "3.58" if f == cc.FSC_HZ else f"{f / 1e6:g}"
            a = cc.tone_amplitude(line, *t, f)
            res.add(f"Multiburst {name} MHz (IRE p-p)", 2 * a)
            if f != cc.MULTIBURST[1][0]:
                res.add(f"Multiburst {name} MHz re 1 MHz (dB)", 20 * math.log10(a / ref))


# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------


def cvbs_lines(path: Path, frames: int | None):
    f = cc.read_cvbs(path)
    for k in range(min(f.frame_count, frames or f.frame_count)):
        frame = f.frame(k)
        for ln in cc.NORMAL_SYNC_LINES:
            yield frame[cc.row(ln)]


def demodulate_block(x, band, equalise):
    """Video (IRE, de-emphasised, 40 MSPS) and the carrier envelope."""
    n = x.size
    f = np.fft.fftfreq(n, 1 / rd.RF_RATE_HZ)
    spectrum = np.fft.fft(x - x.mean())
    z_spec = spectrum * rd.raised_cosine_band(f, *band, RF_EDGE_HZ) * (f > 0) * 2
    z = np.fft.ifft(z_spec)
    dz = np.fft.ifft(2j * np.pi * f * z_spec)
    inst = np.imag(np.conj(z) * dz) / (2 * np.pi * np.maximum(np.abs(z) ** 2, 1e-300))
    raw = (inst - rd.BLANKING_HZ) / rd.HZ_PER_IRE
    video = np.real(np.fft.ifft(np.fft.fft(raw) * rd.video_filter(f, equalise)))
    return video, np.abs(z)


def sync_edges(video):
    """0H of each normal-sync line (fractional RF samples)."""
    low = video < SYNC_SLICE_IRE
    falls = np.flatnonzero(~low[:-1] & low[1:])
    rises = np.flatnonzero(low[:-1] & ~low[1:])
    out = []
    us = rd.RF_RATE_HZ * 1e-6
    for k in falls:
        r = rises[np.searchsorted(rises, k)] if np.searchsorted(rises, k) < rises.size else None
        if r is None or not SYNC_WIDTH_US[0] * us < r - k < SYNC_WIDTH_US[1] * us:
            continue
        if k < 8 or k + 9.0 * us >= video.size:
            continue
        # Both levels from this line (the comparator's windows), not from
        # the end of the previous line, whose content can reach its sync.
        tip = video[int(k + cc.SYNC_TIP[0] * us):int(k + cc.SYNC_TIP[1] * us)].mean()
        blank = video[int(k + cc.BACK_PORCH[0] * us):int(k + cc.BACK_PORCH[1] * us)].mean()
        half = 0.5 * (tip + blank)
        j = k - 8 + np.flatnonzero((video[k - 8:k + 8] > half) & (video[k - 7:k + 9] <= half))
        if j.size == 0:
            continue
        j = int(j[-1])
        out.append(j + (video[j] - half) / (video[j] - video[j + 1]))
    return np.array(out)


def local_timebase(edges, i):
    """0H and line period at edge i, from a straight-line fit of the 0H of
    the consecutive lines around it.

    Averages out a single line's 0H error while following the disc's wow,
    which changes the line period by far less over a few lines.
    """
    a, b = max(i - TIMEBASE_HALF, 0), min(i + TIMEBASE_HALF + 1, edges.size)
    idx = np.rint((edges[a:b] - edges[i]) / LINE_RF)
    resid = edges[a:b] - edges[i] - idx * LINE_RF
    ok = np.abs(resid) < TIMEBASE_TOLERANCE * LINE_RF
    if ok.sum() < 4 or np.unique(idx[ok]).size != ok.sum():
        return None
    slope, intercept = np.polyfit(idx[ok], edges[a:b][ok], 1)
    if abs(slope / LINE_RF - 1) > TIMEBASE_TOLERANCE:
        return None
    return intercept, slope


SIDEBAND_ORDERS = 3          # carrier ± up to 3·fm in the sideband fit


def sidebands(x, h, scale, video_line, f_mod, t_window):
    """First lower and upper FM sideband levels relative to the carrier (dB).

    Least-squares fit, over the window, of the raw RF to tones at
    fc + k·f_mod (|k| ≤ 3) with fc from the window's mean video level.
    """
    fc = rd.BLANKING_HZ + cc.level(video_line, *t_window, whole_cycles=False) * rd.HZ_PER_IRE
    rate = rd.RF_RATE_HZ * scale      # block samples per second of this line
    a = int(math.ceil(h + t_window[0] * 1e-6 * rate))
    b = int(math.floor(h + t_window[1] * 1e-6 * rate))
    t = (np.arange(a, b) - h) / rate
    cols = [np.ones_like(t)]
    for k in range(-SIDEBAND_ORDERS, SIDEBAND_ORDERS + 1):
        w = 2 * np.pi * (fc + k * f_mod) * t
        cols += [np.cos(w), np.sin(w)]
    coef, *_ = np.linalg.lstsq(np.column_stack(cols), x[a:b], rcond=None)
    amp = np.hypot(coef[1::2], coef[2::2])
    db = 20 * np.log10(amp / amp[SIDEBAND_ORDERS])
    return fc, db[SIDEBAND_ORDERS - 1], db[SIDEBAND_ORDERS + 1], db[SIDEBAND_ORDERS - 2], \
        db[SIDEBAND_ORDERS + 2]


def measure_sidebands(x, h, scale, line, res: Results):
    kind = classify(line)
    windows = [("Burst", cc.FSC_HZ, cc.BURST)]
    if kind == "combination":
        windows += [(f"Multiburst {'3.58' if f == cc.FSC_HZ else f'{f / 1e6:g}'} MHz", f, t)
                    for f, t in cc.MULTIBURST[1:]]
    for name, f, t in windows:
        fc, lower, upper, lower2, upper2 = sidebands(x, h, scale, line, f, t)
        res.add(f"{name}: carrier (MHz)", fc / 1e6)
        res.add(f"{name}: lower sideband re carrier (dB)", lower)
        res.add(f"{name}: upper sideband re carrier (dB)", upper)
        res.add(f"{name}: second lower sideband re carrier (dB)", lower2)
        res.add(f"{name}: second upper sideband re carrier (dB)", upper2)


def rf_lines(path: Path, frames: int | None, band, equalise, res: Results,
             sideband_fit: bool = False):
    rf = np.memmap(path, dtype="<i2", mode="r")
    limit = rf.size if frames is None else min(rf.size, int(frames * rd.FRAME_PERIOD_S
                                                             * rd.RF_RATE_HZ))
    start = 0
    n_out = np.arange(cc.LINE_SAMPLES) + cc.ORIGIN_SAMPLES
    while start + 2 * PAD < limit:
        stop = min(start + BLOCK, limit)
        x = np.asarray(rf[start:stop], dtype=np.float64)
        video, env = demodulate_block(x, band, equalise)
        spline = interpolate.CubicSpline(np.arange(video.size), video)
        edges = sync_edges(video)
        lo, hi = (PAD if start else 0), video.size - PAD
        for i, h in enumerate(edges):
            if not lo <= h < hi or h + LINE_RF + 50 > video.size:
                continue
            timebase = local_timebase(edges, i)
            if timebase is None:
                continue
            h, period = timebase
            line = spline(h + n_out * RF_PER_OUT * period / LINE_RF)
            measure_line(line, res)
            if sideband_fit and -50 < cc.level(line, *cc.SYNC_TIP) < -30:
                measure_sidebands(x, h, period / LINE_RF, line, res)
            # Carrier envelope at sync tip and blanking: the RF transfer's
            # slope between 7.6 and 8.1 MHz.
            us = rd.RF_RATE_HZ * 1e-6
            tip = env[int(h + 1 * us):int(h + 3.7 * us)].mean()
            blank = env[int(h + 8.2 * us):int(h + 9.0 * us)].mean()
            res.add("Envelope, blanking / sync tip", blank / tip)
        if stop == limit:
            break
        start = stop - 2 * PAD


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", choices=("cvbs", "rf"))
    parser.add_argument("path", type=Path)
    parser.add_argument("--frames", type=int)
    parser.add_argument("--rf-band", type=float, nargs=2, metavar=("LOW", "HIGH"),
                        default=[b / 1e6 for b in RF_BAND_HZ], help="MHz")
    parser.add_argument("--playback-equaliser", action="store_true")
    parser.add_argument("--sidebands", action="store_true",
                        help="rf: also fit the FM sidebands of the burst and multiburst")
    args = parser.parse_args(argv)
    res = Results()
    if args.source == "cvbs":
        for line in cvbs_lines(args.path, args.frames):
            measure_line(line, res)
    else:
        band = tuple(b * 1e6 for b in args.rf_band)
        rf_lines(args.path, args.frames, band, args.playback_equaliser, res, args.sidebands)
    print(res.table())
    return 0


if __name__ == "__main__":
    sys.exit(main())
