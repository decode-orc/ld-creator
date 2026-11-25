# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Self-tests of the VITS measurements and the standard-burst copy.

    python3 -E -B -m unittest discover -s scripts -p 'test_*.py'
"""

import math
import unittest

import numpy as np
from scipy import special

import cvbs_compare as cc
import reference_demod as rd
import standard_burst as sb
import vits_measure as vm

T = (np.arange(cc.LINE_SAMPLES) + cc.ORIGIN_SAMPLES) * cc.SAMPLE_US
N = np.arange(cc.LINE_SAMPLES)


def subcarrier(amplitude, phase):
    """A·cos(π·n/2 + φ): the stored-sample convention of cc.fsc_phasor."""
    return amplitude * np.cos(0.5 * np.pi * N + phase)


def line_with_burst(start_us, cycles, amplitude=20.0, phase=math.pi):
    line = np.where(T < 4.7, -40.0, 0.0)
    end_us = start_us + cycles / cc.FSC_HZ * 1e6
    u0 = np.clip((T - start_us) / sb.EDGE_US + 0.5, 0, 1)
    u1 = np.clip((end_us - T) / sb.EDGE_US + 0.5, 0, 1)
    env = np.sin(0.5 * np.pi * u0) ** 2 * np.sin(0.5 * np.pi * u1) ** 2
    return line + env * subcarrier(amplitude, phase)


class BurstTest(unittest.TestCase):
    def test_duration_of_a_standard_burst(self):
        start, duration = vm.burst_duration_us(line_with_burst(5.3, 9))
        self.assertAlmostEqual(start, 5.3, delta=0.01)
        self.assertAlmostEqual(duration * cc.FSC_HZ * 1e-6, 9.0, delta=0.05)

    def test_duration_of_a_short_burst(self):
        start, duration = vm.burst_duration_us(line_with_burst(5.63, 7.3))
        self.assertAlmostEqual(start, 5.63, delta=0.01)
        self.assertAlmostEqual(duration * cc.FSC_HZ * 1e-6, 7.3, delta=0.05)

    def test_standard_burst_keeps_amplitude_and_phase(self):
        short = line_with_burst(5.63, 7.3, amplitude=19.7, phase=2.0)
        frame, replaced = sb.standard_burst(np.tile(short, (cc.FRAME_LINES, 1)))
        self.assertEqual(replaced, cc.FRAME_LINES)
        line = frame[100]
        before, after = cc.fsc_phasor(short, *cc.BURST), cc.fsc_phasor(line, *cc.BURST)
        self.assertAlmostEqual(abs(after), abs(before), delta=0.01)
        self.assertAlmostEqual(np.angle(after / before), 0.0, delta=1e-3)
        start, duration = vm.burst_duration_us(line)
        self.assertAlmostEqual(start, sb.BURST_START_US, delta=0.01)
        self.assertAlmostEqual(duration * cc.FSC_HZ * 1e-6, sb.BURST_CYCLES, delta=0.05)
        outside = (T < sb.REGION_US[0]) | (T > sb.REGION_US[1])
        np.testing.assert_array_equal(line[outside], short[outside])

    def test_standard_edge_rise_is_300_ns(self):
        env = sb.envelope(np.linspace(4.5, 6.0, 15001))
        t = np.linspace(4.5, 6.0, 15001)
        rise = t[np.argmax(env >= 0.9)] - t[np.argmax(env >= 0.1)]
        self.assertAlmostEqual(rise, 0.300, delta=0.001)


class LineTest(unittest.TestCase):
    def virs_line(self, chroma_phase):
        line = line_with_burst(5.3, 9)
        # IEC 60857 Figure 6b: chroma reference on a 70 IRE pedestal
        # 12-36 µs, luminance reference 36-48 µs, black reference 48-60 µs.
        line[(T >= 12.0) & (T < 36.0)] += 70.0
        chroma = (T >= 12.0) & (T < 36.0)
        line[chroma] += subcarrier(20.0, chroma_phase)[chroma]
        line[(T >= 36.0) & (T < 48.0)] += 50.0
        line[(T >= 48.0) & (T < 60.0)] += 7.5
        return line

    def test_virs_is_found_and_measured(self):
        line = self.virs_line(math.pi)
        self.assertEqual(vm.classify(line), "virs")
        res = vm.Results()
        vm.measure_line(line, res)
        self.assertAlmostEqual(res.values["VIRS chroma (IRE p-p)"][0], 40.0, delta=0.05)
        self.assertAlmostEqual(res.values["VIRS luminance (IRE)"][0], 50.0, delta=0.05)
        self.assertAlmostEqual(res.values["VIRS black (IRE)"][0], 7.5, delta=0.05)

    def test_phase_is_folded_to_90_degrees(self):
        # In phase with the burst, and in antiphase, both read 0°.
        for phase in (math.pi, 0.0):
            self.assertAlmostEqual(vm.phase_to_burst(self.virs_line(phase), cc.VIRS_CHROMA),
                                   0.0, delta=0.05)
        self.assertAlmostEqual(vm.phase_to_burst(self.virs_line(math.pi + 0.1),
                                                 cc.VIRS_CHROMA),
                               math.degrees(0.1), delta=0.05)

    def test_a_white_line_is_not_a_vits_line(self):
        line = line_with_burst(5.3, 9)
        line[T > 9.5] = 100.0
        self.assertIsNone(vm.classify(line))


class RfTest(unittest.TestCase):
    def test_timebase_ignores_one_biased_edge(self):
        period = vm.LINE_RF * (1 + 2e-4)
        edges = 1000.0 + period * np.arange(9)
        edges[5] += 0.86  # the bias a line's own content once put on its 0H
        h, slope = vm.local_timebase(edges, 4)
        self.assertAlmostEqual(slope, period, delta=0.2)
        self.assertAlmostEqual(h, edges[4], delta=0.2)

    def test_sidebands_of_tone_fm_match_bessel(self):
        fc, fm, beta = 8.7e6, 2.0e6, 0.35
        t = np.arange(4000) / rd.RF_RATE_HZ
        x = np.cos(2 * np.pi * fc * t + beta * np.sin(2 * np.pi * fm * t))
        line = np.full(cc.LINE_SAMPLES, (fc - rd.BLANKING_HZ) / rd.HZ_PER_IRE)
        got_fc, lower, upper, lower2, upper2 = vm.sidebands(x, 0.0, 1.0, line, fm, (10.0, 60.0))
        self.assertAlmostEqual(got_fc, fc, delta=1.0)
        first = 20 * math.log10(special.jv(1, beta) / special.jv(0, beta))
        second = 20 * math.log10(special.jv(2, beta) / special.jv(0, beta))
        for value, want in ((lower, first), (upper, first), (lower2, second), (upper2, second)):
            self.assertAlmostEqual(value, want, delta=0.05)


if __name__ == "__main__":
    unittest.main()
