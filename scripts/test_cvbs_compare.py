# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Self-tests of the round-trip comparator on synthetic NTSC frames.

    python3 -E -m unittest discover -s scripts -p 'test_*.py'
"""

import math
import unittest

import numpy as np

import cvbs_compare as cc


def sample_times_us():
    return (np.arange(cc.LINE_SAMPLES) + cc.ORIGIN_SAMPLES) * cc.SAMPLE_US


def blank_frame():
    """Sync 0-4.7 µs at -40 IRE, burst, blanking elsewhere, on every line."""
    t = sample_times_us()
    line = np.where(t < 4.7, -40.0, 0.0)
    burst = (t >= 5.3) & (t < 7.8)
    line = line + burst * 20 * np.cos(2 * np.pi * cc.FSC_HZ * t * 1e-6 + math.pi)
    return np.tile(line, (cc.FRAME_LINES, 1))


def level_line(levels, start_us, cell_us):
    """A line holding `levels` (0 or 100 IRE) in cells from `start_us`."""
    t = sample_times_us()
    line = np.zeros(cc.LINE_SAMPLES)
    for i, value in enumerate(levels):
        line[(t >= start_us + i * cell_us) & (t < start_us + (i + 1) * cell_us)] = value
    return line


def biphase_line(code):
    """IEC 60857 Figure 11: 2 µs cells, rise at the centre for 1."""
    halves = []
    for k in range(23, -1, -1):
        bit = (code >> k) & 1
        halves += [0, 100] if bit else [100, 0]
    return level_line(halves, 10.5, 1.0)


def fm_line(field_one, number, parity_error=False):
    """IEC 60857 Figure 13: 1 µs cells, transition at every boundary and at
    the centre of a 1; odd parity over the data and parity bits."""
    digits = [(number // 10 ** i) % 10 for i in range(5)]
    data = [(d >> b) & 1 for d in digits for b in range(4)]
    parity = (1 - sum(data) % 2) ^ int(parity_error)
    bits = [0, 0, 1, 1, int(field_one), 1, 1, 1, 0, 0, 1, 0, *data, parity,
            0, 0, 0, 1, 1, 0, 1]
    halves, high = [], False
    for bit in bits:
        high = not high
        first = high
        if bit:
            high = not high
        halves += [100 * first, 100 * high]
    return level_line(halves, 13.7, 0.5)


class VbiTest(unittest.TestCase):
    def test_biphase_codes_decode(self):
        for code in (0x88FFFF, 0xF12345, 0x80EEEE, 0x82CFFF):
            self.assertEqual(cc.decode_biphase(biphase_line(code)), code)

    def test_biphase_absent_on_a_blank_line(self):
        self.assertIsNone(cc.decode_biphase(np.zeros(cc.LINE_SAMPLES)))

    def test_picture_number_from_code(self):
        self.assertEqual(cc.bcd_picture_number(0xF12345), 12345)
        self.assertEqual(cc.bcd_picture_number(0xF00001), 1)
        self.assertIsNone(cc.bcd_picture_number(0x88FFFF))

    def test_fm_codes_decode_with_field_bit(self):
        for field_one, number in ((True, 1), (False, 12345), (True, 99999)):
            self.assertEqual(cc.decode_fm(fm_line(field_one, number)),
                             cc.FmCode(field_one, number))

    def test_fm_code_with_a_wrong_parity_bit_is_rejected(self):
        self.assertIsNone(cc.decode_fm(fm_line(True, 12345, parity_error=True)))

    def test_white_flag(self):
        self.assertTrue(cc.white_flag(level_line([100], 0.16 * cc.LINE_US, 0.79 * cc.LINE_US)))
        self.assertFalse(cc.white_flag(np.zeros(cc.LINE_SAMPLES)))

    def test_frame_vbi_and_pairing(self):
        frames = []
        for number in (None, None, 1, 2, 3):
            f = blank_frame()
            code = 0x88FFFF if number is None else 0xF00000 | int(str(number), 16)
            lines = (17, 18, 280, 281) if number is None else (17, 18)
            for ln in lines:
                f[cc.row(ln)] = biphase_line(code)
            frames.append(cc.read_vbi(f))
        self.assertTrue(frames[0].lead_in)
        self.assertEqual((frames[2].picture, frames[2].first_field), (1, 1))
        pairs, notes = cc._pair_frames(frames, frames[1:4])
        self.assertEqual([(t, d) for t, d, _, _ in pairs], [(1, 0), (2, 1), (3, 2)])
        self.assertEqual(notes, ["lead-in frames 0-0 not decoded (before the first decoded frame)",
                                 "pictures 3-3 not decoded (after the last decoded frame)"])


def sin_squared(t_us, centre_us, had_us):
    """A sin² pulse of unit height and half-amplitude duration `had_us`."""
    x = (t_us - centre_us) / (2 * had_us)
    return np.where(np.abs(x) < 0.5, np.cos(np.pi * x) ** 2, 0.0)


def pulse_line(scale_2t=1.0, chroma_gain=1.0, chroma_delay_ns=0.0):
    """Line 20's 2T pulse and modulated 12.5T pulse (NTC-7, T = 125 ns)."""
    t = sample_times_us()
    line = 100 * scale_2t * sin_squared(t, 34.0, 0.25)
    line += 25 * sin_squared(t, 37.0, 1.5625)
    tc = t - chroma_delay_ns * 1e-3
    line += (25 * chroma_gain * sin_squared(tc, 37.0, 1.5625)
             * np.cos(2 * np.pi * cc.FSC_HZ * tc * 1e-6))
    return line


class MeasurementTest(unittest.TestCase):
    def test_2t_amplitude_and_duration(self):
        peak, had = cc._pulse_2t(pulse_line())
        self.assertAlmostEqual(peak, 100, delta=0.5)
        self.assertAlmostEqual(had, 250, delta=2)
        scaled, had_scaled = cc._pulse_2t(pulse_line(scale_2t=1.05))
        self.assertAlmostEqual(scaled / peak, 1.05, places=6)
        self.assertAlmostEqual(had_scaled, had, places=6)

    def test_12t5_gain_and_delay_errors_are_measured(self):
        ty, tc, tty, ttc = cc._pulse_12t5(pulse_line())
        dy, dc, dty, dtc = cc._pulse_12t5(pulse_line(chroma_gain=1.1, chroma_delay_ns=50))
        self.assertAlmostEqual((dc / dy) / (tc / ty), 1.1, delta=0.002)
        self.assertAlmostEqual(1e3 * ((dtc - dty) - (ttc - tty)), 50, delta=1.0)

    def test_fsc_phasor_recovers_amplitude_and_phase(self):
        t = sample_times_us()
        line = 30 + 20 * np.cos(2 * np.pi * cc.FSC_HZ * t * 1e-6 + 0.3)
        burst = cc.fsc_phasor(line, 5.7, 7.4)
        chroma = cc.fsc_phasor(line, 12.0, 33.0)
        self.assertAlmostEqual(abs(chroma), 20, places=9)
        self.assertAlmostEqual(np.angle(chroma / burst), 0, places=9)

    def test_tone_amplitude(self):
        t = sample_times_us()
        line = 50 + 25 * np.sin(2 * np.pi * 4.2e6 * t * 1e-6 + 1.0)
        self.assertAlmostEqual(cc.tone_amplitude(line, 40.5, 42.5, 4.2e6), 25, places=6)

    def test_window_is_whole_cycles_and_inside_its_limits(self):
        s = cc.window(8.2, 9.0)
        self.assertEqual((s.stop - s.start) % 4, 0)
        t = cc.sample_times_us(s)
        self.assertGreaterEqual(t[0], 8.2)
        self.assertLessEqual(t[-1], 9.0)

    def test_zero_h_is_where_the_stored_origin_puts_it(self):
        frame = blank_frame()
        # A linear 140 ns sync edge centred on 0H, ahead of sample 0.
        flat = frame.reshape(-1)
        for ln in (10, 11):
            base = cc.row(ln) * cc.LINE_SAMPLES
            for k in range(-4, 4):
                t_ns = (k + cc.ORIGIN_SAMPLES) * cc.SAMPLE_US * 1e3
                flat[base + k] = float(np.clip(-40 * (t_ns + 70) / 140, -40, 0))
        h = cc._zero_h(frame, [10, 11])
        np.testing.assert_allclose(h, -cc.ORIGIN_SAMPLES, atol=1e-9)

    def test_a_level_offset_is_measured_and_refutes(self):
        truth = blank_frame()
        lines = cc.NORMAL_SYNC_LINES
        metric = cc.metric_table()["L1"]
        for v in (cc._levels(truth + 2.0, lines, cc.BACK_PORCH)
                  - cc._levels(truth, lines, cc.BACK_PORCH)):
            metric.add("f", v)
        s = metric.summary()
        self.assertAlmostEqual(s["mean"], 2.0, places=9)
        self.assertEqual(s["verdict"], "refuted")


if __name__ == "__main__":
    unittest.main()
