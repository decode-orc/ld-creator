# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Reference IEC 60857 NTSC video demodulator for ld-creator RF.

A decoder written from the standard alone, to test whether ld-creator's RF
carries the IEC 60857 video signal: if this decodes the RF to the truth
within the round-trip thresholds, the RF is right by the standard, and a
disagreement with ld-decode lies between the standard and ld-decode's
calibration. It shares no code with ld-creator or ld-decode.

Chain, per frame, in the frequency domain:

1. Analytic signal of the RF over the FM band (optionally divided by the
   S11 readout transfer, computed independently, for disc RF).
2. Instantaneous frequency, from z and its exact derivative.
3. Video in IRE from the §9.2.1-§9.2.3 carrier frequencies.
4. De-emphasis, the exact inverse of the §9.2.4 pre-emphasis.
5. Optionally, the playback equaliser that §9.1.7's pre-distortion
   presumes: a group delay of −t_d(f).
6. A zero-phase video low-pass, and resampling to the CVBS stored
   frames (4fsc, 0H-aligned).

    python3 -E -B scripts/reference_demod.py <rf.s16> <truth.cvbs> <out.cvbs>
        [--playback-equaliser] [--mtf <resolved project>]
"""

from __future__ import annotations

import argparse
import math
import re
import shutil
import sys
from pathlib import Path

import numpy as np

import rf_analysis as ra

RF_RATE_HZ = 40e6
FSC_HZ = 315e6 / 88
OUT_RATE_HZ = 4 * FSC_HZ                    # 63/176 of the RF rate
FRAME_PERIOD_S = 1001 / 30000
FRAME_SAMPLES = 525 * 910
ORIGIN_S = (57 / 90) / OUT_RATE_HZ          # 0H to stored sample 0

# IEC 60857 §9.2.1-§9.2.3: blanking 8.1 MHz; sync tip (−40 IRE) to white
# (100 IRE) spans 1.7 MHz.
BLANKING_HZ = 8.1e6
HZ_PER_IRE = 1.7e6 / 140
# §9.2.4 pre-emphasis (1 + jωt1)/(1 + jωt2).
T1_S, T2_S = 320e-9, 120e-9
# §9.1.7 pre-distortion t_d(f) (MHz, ns), relative to 0.5 MHz.
PREDISTORTION = ((0.0, 0.0), (0.5, 0.0), (2.0, -15.0), (3.0, -45.0),
                 (3.58, -80.0), (4.0, -135.0), (4.2, -200.0))

# Declared filters: the FM band passed to the demodulator, and the video
# low-pass. Raised-cosine edges; both flat over the band the metrics use.
RF_BAND_HZ = (1.5e6, 18.5e6)
RF_EDGE_HZ = 1.0e6
VIDEO_PASS_HZ, VIDEO_STOP_HZ = 4.6e6, 6.0e6
# The inverse readout transfer is applied where H is at least this.
MIN_TRANSFER = 0.005

PAD_S = 100e-6


def raised_cosine_band(f, low, high, edge):
    """1 in [low, high], falling to 0 over `edge` outside it."""
    a = np.abs(f)
    g = np.ones_like(a)
    lo = a < low
    g[lo] = 0.5 * (1 + np.cos(np.pi * np.clip((low - a[lo]) / edge, 0, 1)))
    hi = a > high
    g[hi] = 0.5 * (1 + np.cos(np.pi * np.clip((a[hi] - high) / edge, 0, 1)))
    return g


def video_lowpass(f):
    a = np.abs(f)
    x = np.clip((a - VIDEO_PASS_HZ) / (VIDEO_STOP_HZ - VIDEO_PASS_HZ), 0, 1)
    return 0.5 * (1 + np.cos(np.pi * x))


def de_emphasis(f):
    w = 2j * np.pi * f
    return (1 + w * T2_S) / (1 + w * T1_S)


def playback_equaliser(f):
    """Group delay −t_d(f): the player response §9.1.7 pre-distorts for.

    t_d is interpolated linearly between the table points and held beyond
    4.2 MHz; the phase is −2π∫τ df, odd in f.
    """
    fm = np.array([p[0] for p in PREDISTORTION]) * 1e6
    td = np.array([p[1] for p in PREDISTORTION]) * 1e-9
    grid = np.linspace(0, max(np.abs(f).max(), fm[-1]), 20001)
    tau = -np.interp(grid, fm, td)
    phase = -2 * np.pi * np.concatenate(
        [[0.0], np.cumsum(0.5 * (tau[1:] + tau[:-1]) * np.diff(grid))])
    return np.exp(1j * np.sign(f) * np.interp(np.abs(f), grid, phase))


class ReadoutInverse:
    """1/H(f/v) from the S11 linear readout (independent quadrature)."""

    def __init__(self, project_text: str):
        def key(name):
            m = re.search(rf"^\s+{name}: (\S+)$", project_text, re.M)
            return m.group(1).strip('"') if m else None

        self.format = key("format")
        self.radius_m = float(key("start_radius_mm")) * 1e-3
        self.pitch_m = float(key("track_pitch_um")) * 1e-6
        self.na = float(key("numerical_aperture"))
        self.rim = float(key("rim_intensity"))
        velocity = key("linear_velocity_m_s")
        self.velocity = float(velocity) if velocity else None
        self.nu = np.linspace(0, 2, 4001)
        self.h = np.array([ra.readout_transfer(n, self.rim) for n in self.nu])

    def speed(self, t: float) -> float:
        if self.format == "clv":
            return self.velocity
        r = self.radius_m + self.pitch_m * t / FRAME_PERIOD_S
        return 2 * math.pi * r / FRAME_PERIOD_S

    def gain(self, f, t: float):
        nu = np.abs(f) / self.speed(t) * ra.READOUT_WAVELENGTH_M / self.na
        h = np.interp(nu, self.nu, self.h, right=0.0)
        g = np.zeros_like(h)
        ok = h >= MIN_TRANSFER
        g[ok] = 1.0 / h[ok]
        return g


def demodulate(x, rate_hz, mtf_gain=None):
    """Video in IRE (before de-emphasis) from RF samples."""
    n = x.size
    f = np.fft.fftfreq(n, 1 / rate_hz)
    spectrum = np.fft.fft(x - x.mean())
    band = raised_cosine_band(f, *RF_BAND_HZ, RF_EDGE_HZ) * (f > 0) * 2
    if mtf_gain is not None:
        band = band * mtf_gain(f)
    z_spec = spectrum * band
    z = np.fft.ifft(z_spec)
    dz = np.fft.ifft(2j * np.pi * f * z_spec)
    inst = np.imag(np.conj(z) * dz) / (2 * np.pi * np.maximum(np.abs(z) ** 2, 1e-300))
    return (inst - BLANKING_HZ) / HZ_PER_IRE


def video_filter(f, equalise: bool):
    g = de_emphasis(f) * video_lowpass(f)
    if equalise:
        g = g * playback_equaliser(f)
    return g


def decode_frame(rf, frame: int, equalise: bool, readout: ReadoutInverse | None):
    """One stored frame (477 750 samples of IRE)."""
    pad_out = int(round(PAD_S * OUT_RATE_HZ))
    start_s = frame * FRAME_PERIOD_S + ORIGIN_S - pad_out / OUT_RATE_HZ
    n0 = math.floor(start_s * RF_RATE_HZ)
    delta = start_s - n0 / RF_RATE_HZ
    blocks = math.ceil((FRAME_SAMPLES + 2 * pad_out) * 176 / 63 / 176)
    blocks += blocks % 2
    n = 176 * blocks
    m = 63 * blocks
    x = np.zeros(n)
    lo, hi = max(n0, 0), min(n0 + n, rf.size)
    if hi > lo:
        x[lo - n0:hi - n0] = rf[lo:hi]
    t_mid = (frame + 0.5) * FRAME_PERIOD_S
    gain = (lambda f: readout.gain(f, t_mid)) if readout else None
    video = demodulate(x, RF_RATE_HZ, gain)
    f = np.fft.fftfreq(n, 1 / RF_RATE_HZ)
    v = np.fft.fft(video) * video_filter(f, equalise) * np.exp(2j * np.pi * f * delta)
    out = np.zeros(m, dtype=complex)
    half = m // 2
    out[:half] = v[:half]
    out[-half + 1:] = v[-half + 1:]
    samples = np.real(np.fft.ifft(out)) * m / n
    return samples[pad_out:pad_out + FRAME_SAMPLES]


def write_cvbs_s16(frames_ire, path: Path):
    """CVBS_S16_4FSC: int16 = (code − 240)·32, codes clamped to 4-1019."""
    with path.open("wb") as out:
        for ire in frames_ire:
            code = np.clip(np.round(240 + 5.6 * ire), 4, 1019)
            out.write(((code - 240) * 32).astype("<i2").tobytes())


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("rf", type=Path)
    parser.add_argument("truth", type=Path, help="the truth .cvbs (its .meta is copied)")
    parser.add_argument("output", type=Path)
    parser.add_argument("--playback-equaliser", action="store_true",
                        help="apply the player group delay §9.1.7 pre-distorts for")
    parser.add_argument("--mtf", type=Path,
                        help="resolved project: divide by its S11 readout transfer")
    args = parser.parse_args(argv)
    rf = np.memmap(args.rf, dtype="<i2", mode="r")
    readout = ReadoutInverse(args.mtf.read_text()) if args.mtf else None
    frames = int(rf.size / RF_RATE_HZ / FRAME_PERIOD_S + 1e-9)
    write_cvbs_s16((decode_frame(rf, k, args.playback_equaliser, readout)
                    for k in range(frames)), args.output)
    meta = args.truth.with_suffix(".meta")
    shutil.copyfile(meta, args.output.with_suffix(".meta"))
    print(f"{frames} frames → {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
