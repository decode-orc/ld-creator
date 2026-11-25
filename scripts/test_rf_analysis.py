# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Self-tests of rf_analysis.py on synthetic signals (plan Task 6.1).

Acceptance: a synthetic tone is recovered to < 1 kHz and synthetic edges to
< 0.1 ns. Run:

    nix develop "git+file://$PWD" --command python3 -E -m unittest discover -s scripts -p 'test_*.py'
"""

from __future__ import annotations

import math
import tempfile
import unittest
from pathlib import Path

import numpy as np

import rf_analysis as ra

RATE_40 = 40e6
RATE_80 = 80e6
FM_RATE = 24 * 315e6 / 88  # 24 fsc, the S2 modelling rate

TONE_TOLERANCE_HZ = 1e3  # plan Task 6.1
EDGE_TOLERANCE_S = 0.1e-9  # plan Task 6.1


def quantise_10bit(u: np.ndarray) -> np.ndarray:
    """The Domesday Duplicator converter: round(512 + 512·u), clamped."""
    codes = np.clip(np.round(512 + 512 * u), 0, 1023)
    return (codes - 512) / 512


class ToneTest(unittest.TestCase):
    def test_recovers_quantised_carriers_at_40_mhz(self):
        t = np.arange(40_000) / RATE_40  # 1 ms
        for f in (7_614_286.0, 8_101_234.5, 9_314_286.0, 12_150_000.0):
            u = quantise_10bit(0.375 * np.cos(2 * np.pi * f * t + 0.3))
            measured = ra.window_frequency(u, RATE_40, 0.0, 100e-6, 900e-6,
                                           (3e6, 16e6))
            self.assertLess(abs(measured - f), TONE_TOLERANCE_HZ, f)

    def test_recovers_carrier_from_short_window(self):
        # 12 µs, the shortest window the stage checks use.
        t = np.arange(4000) / RATE_80
        f = 9_314_286.0
        x = np.cos(2 * np.pi * f * t)
        measured = ra.window_frequency(x, RATE_80, 0.0, 10e-6, 22e-6, (3e6, 16e6))
        self.assertLess(abs(measured - f), TONE_TOLERANCE_HZ)

    def test_follows_sinusoidal_fm(self):
        t = np.arange(80_000) / RATE_80
        fc, dev, fm = 8.1e6, 1.0e6, 1.0e6
        x = np.cos(2 * np.pi * fc * t + dev / fm * np.sin(2 * np.pi * fm * t))
        f = ra.instantaneous_frequency(x, RATE_80, (1e6, 20e6))
        mid = t[:-1] + 0.5 / RATE_80
        expected = fc + dev * np.cos(2 * np.pi * fm * mid)
        core = slice(2000, -2000)
        self.assertLess(np.max(np.abs(f[core] - expected[core])), TONE_TOLERANCE_HZ)

    def test_fits_sinusoid_amplitude(self):
        t = np.arange(10_000) / RATE_80
        x = 0.2 + 0.137 * np.cos(2 * np.pi * 3.58e6 * t + 1.0)
        amplitude, _, offset = ra.fit_sinusoid(t, x, 3.58e6)
        self.assertAlmostEqual(amplitude, 0.137, places=12)
        self.assertAlmostEqual(offset, 0.2, places=12)


class EdgeTest(unittest.TestCase):
    def test_recovers_zero_crossings_of_sampled_carrier(self):
        f, theta = 8.1e6, 0.4
        t = np.arange(4000) / RATE_40
        for x in (np.cos(2 * np.pi * f * t + theta),
                  quantise_10bit(0.375 * np.cos(2 * np.pi * f * t + theta))):
            times, directions = ra.level_crossings(x, RATE_40)
            k = np.round((2 * np.pi * f * times + theta) / np.pi - 0.5)
            exact = ((k + 0.5) * np.pi - theta) / (2 * np.pi * f)
            self.assertLess(np.max(np.abs(times - exact)), EDGE_TOLERANCE_S)
            # cos falls through zero at φ = π/2 + 2πn.
            self.assertTrue(np.all(directions[(k % 2) == 0] == -1))

    def test_recovers_phase_crossings_of_fm_carrier(self):
        # φ(t) = 2π·fc·t + (Δf/fm)·sin(2π·fm·t), sampled at Fm.
        fc, dev, fm = 9.0e6, 2.0e6, 1.0e6
        n = np.arange(20_000)
        t = n / FM_RATE
        phase = 2 * np.pi * fc * t + dev / fm * np.sin(2 * np.pi * fm * t)
        k = np.arange(40, 3000)
        targets = (k + 0.5) * np.pi
        found = ra.phase_crossings(phase, FM_RATE, 0.0, targets)
        # Exact roots by Newton on the analytic phase.
        exact = targets / (2 * np.pi * fc)
        for _ in range(30):
            value = 2 * np.pi * fc * exact + dev / fm * np.sin(2 * np.pi * fm * exact) - targets
            slope = 2 * np.pi * (fc + dev * np.cos(2 * np.pi * fm * exact))
            exact -= value / slope
        # Far tighter than Task 6.1 requires: the S7 check compares against
        # a 4 ps budget.
        self.assertLess(np.max(np.abs(found - exact)), 0.1e-12)


class TapReaderTest(unittest.TestCase):
    def test_reads_sampled_and_edge_taps(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "phase"
            np.arange(5, dtype="<f8").tofile(str(base) + ".f64")
            (Path(directory) / "phase.tap.yaml").write_text(
                "format: ld-creator-tap\nformat_version: 1\nstage: fm_phase_modulator\n"
                "representation: D3\nkind: sampled\ndata_file: \"phase.f64\"\n"
                "unit: \"rad\"\nfirst_frame: 0\nframe_count: 1\n"
                "sample_type: float64_le\nrate_hz: 80000000\nfirst_sample: 0\n"
                "time_of_first_sample_s: 1e-08\nsample_count: 5\n")
            tap = ra.read_tap(base)
            self.assertEqual(tap.samples.tolist(), [0, 1, 2, 3, 4])
            self.assertAlmostEqual(tap.times()[2], 1e-8 + 2 / 80e6)

            edges = Path(directory) / "edges"
            records = np.zeros(2, dtype=ra.EDGE_DTYPES["D4"])
            records["time_s"] = [1e-7, 2e-7]
            records["polarity"] = [-1, 1]
            records.tofile(str(edges) + ".edges")
            (Path(directory) / "edges.tap.yaml").write_text(
                "format: ld-creator-tap\nformat_version: 1\nstage: recording_limiter\n"
                "representation: D4\nkind: edges\ndata_file: \"edges.edges\"\n"
                "unit: \"s\"\nfirst_frame: 0\nframe_count: 1\nrecord_bytes: 9\n"
                "fields:\n  - {name: time_s, type: float64_le}\n"
                "  - {name: polarity, type: int8}\nrange_begin: 0\n"
                "range_end: 1\nrecord_count: 2\n")
            tap = ra.read_tap(edges)
            self.assertEqual(tap.records["polarity"].tolist(), [-1, 1])

    def test_rejects_unknown_format(self):
        with self.assertRaises(ValueError):
            ra.parse_tap_descriptor("format: other\nformat_version: 1\n")


class ReadoutTransferTest(unittest.TestCase):
    def test_uniform_pupil_matches_closed_form(self):
        for nu in (0.0, 0.3, 1.0, 1.7):
            half = nu / 2
            closed = 2 / math.pi * (math.acos(half) - half * math.sqrt(1 - half * half))
            self.assertAlmostEqual(ra.readout_transfer(nu, 1.0), closed, places=10)

    def test_reference_optics_value_at_hfmi_frequency(self):
        # S11 note: H(802 pits/mm) = 0.21317 at NA 0.40, rim intensity 0.23.
        self.assertAlmostEqual(ra.readout_transfer_spatial(802e3), 0.21317, places=5)
        self.assertAlmostEqual(ra.readout_transfer(0.0), 1.0, places=12)
        self.assertEqual(ra.readout_transfer(2.0), 0.0)


class SpectrumTest(unittest.TestCase):
    def test_transfer_function_of_known_filter(self):
        rng = np.random.default_rng(1)
        x = rng.standard_normal(1 << 18)
        delay = 7
        taps = np.array([0.25, 0.5, 0.25])
        y = np.convolve(np.roll(x, delay), taps, mode="same")
        f, h, pxx = ra.transfer_function(x, y, RATE_80, segment=1 << 13)
        # 'same' centres the 3 taps, so the total delay is `delay` samples.
        expected = 0.5 + 0.5 * np.cos(2 * np.pi * f / RATE_80)
        sel = f < 0.3 * RATE_80
        self.assertLess(np.max(np.abs(np.abs(h[sel]) - expected[sel])), 2e-3)
        tau = ra.group_delay_at(f, h, pxx, 5e6, 0.5e6)
        self.assertAlmostEqual(tau * RATE_80, delay, places=2)

    def test_occupied_band_of_flat_spectrum(self):
        f = np.linspace(0, 10e6, 10_001)
        psd = np.where((f >= 2e6) & (f <= 6e6), 1.0, 0.0)
        lo, hi = ra.occupied_band(f, psd, 0.99)
        self.assertAlmostEqual(lo, 2.02e6, delta=2e3)
        self.assertAlmostEqual(hi, 5.98e6, delta=2e3)


if __name__ == "__main__":
    unittest.main()
