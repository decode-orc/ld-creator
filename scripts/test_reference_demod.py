# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Self-tests of the reference IEC 60857 demodulator on synthetic FM.

    python3 -E -B -m unittest discover -s scripts -p 'test_*.py'
"""

import unittest

import numpy as np

import reference_demod as rd

RATE = 40e6
N = 8000  # 200 µs: every tone and the carrier are periodic in the record
# 35 IRE moves the carrier 425 kHz: 85 more whole cycles in 200 µs.
PEDESTAL = 35.0
TONES = ((0.5e6, 20.0), (2.0e6, 15.0), (3.58e6, 20.0))


def video(t):
    return PEDESTAL + sum(a * np.cos(2 * np.pi * f * t) for f, a in TONES)


def modulate(v, equaliser=None):
    """IEC FM of video v: pre-emphasis, then the phase, in the frequency domain."""
    f = np.fft.fftfreq(N, 1 / RATE)
    spectrum = np.fft.fft(v) / rd.de_emphasis(f)
    if equaliser is not None:
        spectrum = spectrum / equaliser(f)
    frequency = rd.BLANKING_HZ + rd.HZ_PER_IRE * np.real(np.fft.ifft(spectrum))
    ac = np.fft.fft(frequency - frequency.mean())
    ac[1:] /= 2j * np.pi * f[1:]
    ac[0] = 0
    t = np.arange(N) / RATE
    phase = 2 * np.pi * frequency.mean() * t + 2 * np.pi * np.real(np.fft.ifft(ac))
    return np.cos(phase)


def tone(x, f):
    return 2 * abs(np.mean(x * np.exp(-2j * np.pi * f * np.arange(N) / RATE)))


class ReferenceDemodTest(unittest.TestCase):
    def recover(self, rf, equalise=False):
        f = np.fft.fftfreq(N, 1 / RATE)
        raw = rd.demodulate(rf, RATE)
        return np.real(np.fft.ifft(np.fft.fft(raw) * rd.video_filter(f, equalise)))

    def test_recovers_video_through_pre_emphasis_and_fm(self):
        t = np.arange(N) / RATE
        out = self.recover(modulate(video(t)))
        self.assertAlmostEqual(out.mean(), PEDESTAL, delta=1e-3)
        for f, a in TONES:
            self.assertAlmostEqual(tone(out, f) / a, 1.0, delta=1e-3)

    def test_playback_equaliser_undoes_the_predistortion_delay(self):
        t = np.arange(N) / RATE
        # Pre-distort with the inverse equaliser, as the disc does.
        out = self.recover(modulate(video(t), rd.playback_equaliser), equalise=True)
        np.testing.assert_allclose(out, video(t), atol=0.05)

    def test_equaliser_group_delay_matches_the_iec_table(self):
        df = 1e3
        for f_mhz, td_ns in rd.PREDISTORTION[2:]:
            f = np.array([f_mhz * 1e6 - df, f_mhz * 1e6 + df])
            phase = np.unwrap(np.angle(rd.playback_equaliser(f)))
            delay_ns = -(phase[1] - phase[0]) / (2 * np.pi * 2 * df) * 1e9
            self.assertAlmostEqual(delay_ns, -td_ns, delta=0.5)


if __name__ == "__main__":
    unittest.main()
