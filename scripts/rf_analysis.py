# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Independent analysis tools for ld-creator's RF and diagnostic taps.

Everything here is written from the stage notes' equations and from first
principles. Nothing is imported from ld-creator, and nothing is taken from
ld-decode, so the checks built on it do not share the implementation's
errors (plan Task 6.1).

Run with the dev shell's analysis Python, isolated from the PYTHONPATH that
the pinned ld-decode package exports:

    nix develop "path:$PWD" --command python3 -E scripts/<script>.py
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from scipy import integrate, signal, special

# ---------------------------------------------------------------------------
# Tap reader (docs/reference/diagnostic-taps.md)
# ---------------------------------------------------------------------------

TAP_FORMAT = "ld-creator-tap"
TAP_FORMAT_VERSION = 1

# Record layouts of the edge taps, by representation.
EDGE_DTYPES = {
    "D4": np.dtype([("time_s", "<f8"), ("polarity", "i1")]),
    "D5": np.dtype(
        [("arc_length_m", "<f8"), ("radius_m", "<f8"), ("polarity", "i1")]
    ),
}

_SCALAR_LINE = re.compile(r"^([a-z_]+):(?: (.*))?$")


def _parse_scalar(text: str):
    text = text.strip()
    if text.startswith('"') and text.endswith('"'):
        return text[1:-1]
    if re.fullmatch(r"[+-]?\d+", text):
        return int(text)
    try:
        return float(text)
    except ValueError:
        return text


def parse_tap_descriptor(text: str) -> dict:
    """Parses a tap descriptor (`<path>.tap.yaml`).

    The descriptor is flat `key: value` YAML, except for the `fields` list of
    an edge tap, which is skipped because the record layout follows from the
    representation. Anything else is rejected rather than guessed at.
    """
    descriptor: dict = {}
    in_fields = False
    for line in text.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        if line.startswith("  - "):
            if not in_fields:
                raise ValueError(f"unexpected list item: {line!r}")
            continue
        match = _SCALAR_LINE.match(line)
        if not match:
            raise ValueError(f"unexpected descriptor line: {line!r}")
        key, value = match.groups(default="")
        in_fields = key == "fields"
        if not in_fields:
            descriptor[key] = _parse_scalar(value)
    if descriptor.get("format") != TAP_FORMAT:
        raise ValueError("not an ld-creator tap descriptor")
    if descriptor.get("format_version") != TAP_FORMAT_VERSION:
        raise ValueError(
            f"unsupported tap format version {descriptor.get('format_version')}"
        )
    return descriptor


@dataclass
class SampledTap:
    descriptor: dict
    samples: np.ndarray

    @property
    def rate_hz(self) -> float:
        return float(self.descriptor["rate_hz"])

    @property
    def t0(self) -> float:
        """Time of the first sample, in seconds from the D1 origin."""
        return float(self.descriptor["time_of_first_sample_s"])

    def times(self) -> np.ndarray:
        return self.t0 + np.arange(self.samples.size) / self.rate_hz

    def index_of(self, t: float) -> int:
        return int(round((t - self.t0) * self.rate_hz))

    def window(self, t_begin: float, t_end: float):
        """Samples and their times with t_begin <= t < t_end."""
        first = max(0, math.ceil((t_begin - self.t0) * self.rate_hz))
        last = min(self.samples.size, math.ceil((t_end - self.t0) * self.rate_hz))
        n = np.arange(first, last)
        return self.t0 + n / self.rate_hz, self.samples[first:last]


@dataclass
class EdgeTap:
    descriptor: dict
    records: np.ndarray


def read_tap(base_path) -> SampledTap | EdgeTap:
    """Reads a tap from its base path (without `.tap.yaml`)."""
    base = Path(base_path)
    descriptor_path = base.parent / (base.name + ".tap.yaml")
    descriptor = parse_tap_descriptor(descriptor_path.read_text())
    data_path = descriptor_path.parent / descriptor["data_file"]
    if descriptor["kind"] == "sampled":
        if descriptor["sample_type"] != "float64_le":
            raise ValueError("unsupported sample type")
        samples = np.fromfile(data_path, dtype="<f8")
        if samples.size != descriptor["sample_count"]:
            raise ValueError("sample count does not match the descriptor")
        return SampledTap(descriptor, samples)
    if descriptor["kind"] == "edges":
        dtype = EDGE_DTYPES[descriptor["representation"]]
        if dtype.itemsize != descriptor["record_bytes"]:
            raise ValueError("record size does not match the descriptor")
        records = np.fromfile(data_path, dtype=dtype)
        if records.size != descriptor["record_count"]:
            raise ValueError("record count does not match the descriptor")
        return EdgeTap(descriptor, records)
    raise ValueError(f"unknown tap kind {descriptor['kind']!r}")


# Domesday Duplicator word: (code − 512) × 64 (docs/reference/rf-output.md).
DDD_WORD_SCALE = 64
DDD_MID_CODE = 512


def read_rf_s16(path, profile: str = "domesday_duplicator") -> np.ndarray:
    """Reads an `.s16` RF file as the converter input u (±1 = full range)."""
    words = np.fromfile(path, dtype="<i2").astype(np.float64)
    if profile == "domesday_duplicator":
        return (words / DDD_WORD_SCALE) / DDD_MID_CODE
    if profile == "ideal":
        return words / 32768.0
    raise ValueError(f"unknown profile {profile!r}")


# ---------------------------------------------------------------------------
# Instantaneous frequency
# ---------------------------------------------------------------------------


def analytic_signal(x: np.ndarray, rate_hz: float, band_hz=None) -> np.ndarray:
    """The analytic signal of x, optionally limited to a pass band.

    Built in the frequency domain: negative frequencies are removed and
    positive ones doubled. A band (lo, hi) is applied with raised-cosine
    edges 10 % of the band wide, so out-of-band content (harmonics, DC) does
    not bias the phase. The input is mean-removed first.
    """
    n = x.size
    spectrum = np.fft.fft(x - np.mean(x))
    freqs = np.fft.fftfreq(n, d=1.0 / rate_hz)
    gain = np.zeros(n)
    gain[freqs > 0] = 2.0
    if band_hz is not None:
        lo, hi = band_hz
        taper = 0.1 * (hi - lo)
        f = freqs
        g = np.ones(n)
        g[f < lo - taper] = 0.0
        g[f > hi + taper] = 0.0
        rising = (f >= lo - taper) & (f < lo)
        g[rising] = 0.5 * (1 - np.cos(np.pi * (f[rising] - (lo - taper)) / taper))
        falling = (f > hi) & (f <= hi + taper)
        g[falling] = 0.5 * (1 + np.cos(np.pi * (f[falling] - hi) / taper))
        gain *= g
    return np.fft.ifft(spectrum * gain)


def unwrapped_phase(z: np.ndarray) -> np.ndarray:
    return np.unwrap(np.angle(z))


def instantaneous_frequency(x: np.ndarray, rate_hz: float, band_hz=None):
    """Instantaneous frequency of a real signal, one value per sample interval.

    Returns (f, midpoint_offsets): f[n] is the phase advance from sample n to
    n + 1, divided by 2π/rate, which is the mean frequency over that
    interval; it belongs to time (n + ½)/rate.
    """
    z = analytic_signal(x, rate_hz, band_hz)
    step = np.angle(z[1:] * np.conj(z[:-1]))
    return step * rate_hz / (2 * np.pi)


def phase_slope_frequency(times: np.ndarray, phase: np.ndarray) -> float:
    """Mean frequency over a window: least-squares slope of phase / 2π."""
    tc = times - times.mean()
    slope = np.dot(tc, phase - phase.mean()) / np.dot(tc, tc)
    return slope / (2 * np.pi)


def window_frequency(x: np.ndarray, rate_hz: float, t0: float, t_begin, t_end,
                     band_hz) -> float:
    """Mean carrier frequency of a sampled real signal over [t_begin, t_end).

    The analytic signal is formed over the whole record (so the window has no
    edge effects of its own), then the phase is fitted inside the window.
    """
    z = analytic_signal(x, rate_hz, band_hz)
    phase = unwrapped_phase(z)
    first = math.ceil((t_begin - t0) * rate_hz)
    last = math.ceil((t_end - t0) * rate_hz)
    n = np.arange(first, last)
    return phase_slope_frequency(t0 + n / rate_hz, phase[first:last])


def fit_sinusoid(times: np.ndarray, x: np.ndarray, frequency_hz: float):
    """Least-squares fit of c + a·cos(ωt) + b·sin(ωt); returns (amplitude, phase, c)."""
    w = 2 * np.pi * frequency_hz * (times - times[0])
    design = np.column_stack([np.ones_like(w), np.cos(w), np.sin(w)])
    (c, a, b), *_ = np.linalg.lstsq(design, x, rcond=None)
    return math.hypot(a, b), math.atan2(-b, a), c


def fit_harmonics(times: np.ndarray, x: np.ndarray, frequency_hz: float,
                  harmonics=(1, 2, 3, 4, 5)):
    """Joint least-squares fit of a constant and the given harmonics.

    Returns ({harmonic: amplitude}, constant). Fitting the harmonics together
    keeps each one's leakage out of the others on a window that is not a
    whole number of cycles.
    """
    w = 2 * np.pi * frequency_hz * (times - times[0])
    columns = [np.ones_like(w)]
    for h in harmonics:
        columns += [np.cos(h * w), np.sin(h * w)]
    solution, *_ = np.linalg.lstsq(np.column_stack(columns), x, rcond=None)
    amplitudes = {h: math.hypot(solution[1 + 2 * i], solution[2 + 2 * i])
                  for i, h in enumerate(harmonics)}
    return amplitudes, solution[0]


def estimate_tone_frequency(times: np.ndarray, x: np.ndarray, guess_hz: float,
                            rate_hz: float) -> float:
    """Frequency of a single tone, refined from the analytic-signal phase."""
    z = analytic_signal(x, rate_hz, (0.5 * guess_hz, 1.5 * guess_hz))
    trim = max(1, x.size // 20)
    return phase_slope_frequency(times[trim:-trim], unwrapped_phase(z)[trim:-trim])


# ---------------------------------------------------------------------------
# Edge and phase crossings
# ---------------------------------------------------------------------------


def _polynomial_roots_near(segments: np.ndarray, nodes: np.ndarray,
                           start: np.ndarray) -> np.ndarray:
    """Roots of the interpolating polynomials through (nodes, segments[i]).

    Each row of `segments` is interpolated by the polynomial of degree
    len(nodes) − 1 through those nodes, and its root is found by Newton's
    method from `start`, all rows at once.
    """
    vandermonde = np.vander(nodes, increasing=True)
    coeffs = segments @ np.linalg.inv(vandermonde).T  # c0 + c1·u + …
    u = start.astype(np.float64).copy()
    degree = nodes.size - 1
    for _ in range(50):
        value = np.zeros_like(u)
        slope = np.zeros_like(u)
        for k in range(degree, -1, -1):  # Horner, with the derivative
            slope = slope * u + value
            value = value * u + coeffs[:, k]
        step = value / slope
        u -= step
        if np.max(np.abs(step), initial=0.0) < 1e-13:
            break
    return u


def level_crossings(x: np.ndarray, rate_hz: float, t0: float = 0.0,
                    level: float = 0.0, order: int = 8):
    """Times at which a band-limited sampled signal crosses `level`.

    Each sign change is bracketed between samples i and i + 1 and refined on
    the degree-(order − 1) Lagrange polynomial through the `order` samples
    centred on the bracket. Returns (times, directions), direction +1 for a
    rising crossing. Crossings too near either end of the record are
    dropped.
    """
    d = x - level
    idx = np.nonzero((d[:-1] < 0) != (d[1:] < 0))[0]
    half = order // 2
    idx = idx[(idx - half + 1 >= 0) & (idx + half < d.size)]
    offsets = np.arange(-half + 1, half + 1)
    segments = d[idx[:, None] + offsets[None, :]]
    start = d[idx] / (d[idx] - d[idx + 1])
    u = _polynomial_roots_near(segments, offsets.astype(np.float64), start)
    times = t0 + (idx + u) / rate_hz
    directions = np.where(d[idx + 1] > d[idx], 1, -1).astype(np.int8)
    return times, directions


def phase_crossings(phase: np.ndarray, rate_hz: float, t0: float,
                    targets: np.ndarray, order: int = 6) -> np.ndarray:
    """Times at which a monotonic sampled phase reaches each target value.

    The bracket is phase[i] < target <= phase[i + 1]; the root is refined as
    in `level_crossings`. Targets too near either end give NaN.
    """
    pos = np.searchsorted(phase, targets, side="left")
    idx = pos - 1
    half = order // 2
    ok = (idx - half + 1 >= 0) & (idx + half < phase.size)
    out = np.full(targets.size, np.nan)
    i = idx[ok]
    offsets = np.arange(-half + 1, half + 1)
    segments = phase[i[:, None] + offsets[None, :]] - targets[ok][:, None]
    start = -segments[:, half - 1] / (segments[:, half] - segments[:, half - 1])
    u = _polynomial_roots_near(segments, offsets.astype(np.float64), start)
    out[ok] = t0 + (i + u) / rate_hz
    return out


# ---------------------------------------------------------------------------
# Spectra
# ---------------------------------------------------------------------------


def power_spectrum(x: np.ndarray, rate_hz: float, segment: int = 1 << 15):
    """One-sided Welch power spectral density (Hann, 50 % overlap), mean removed."""
    return signal.welch(x, fs=rate_hz, window="hann", nperseg=segment,
                        detrend="constant", scaling="density")


def transfer_function(x: np.ndarray, y: np.ndarray, rate_hz: float,
                      segment: int = 1 << 15):
    """H = Pxy / Pxx between an input and output record at the same rate.

    Returns (f, H, Pxx). Exact for a linear time-invariant system whose
    impulse response is short against the segment.
    """
    f, pxx = signal.welch(x, fs=rate_hz, window="hann", nperseg=segment,
                          detrend=False)
    _, pxy = signal.csd(x, y, fs=rate_hz, window="hann", nperseg=segment,
                        detrend=False)
    return f, pxy / pxx, pxx


def band_average_response(f, h, pxx, centre_hz, half_width_hz):
    """Power-weighted mean of H over a band (dominated by where x has energy)."""
    sel = np.abs(f - centre_hz) <= half_width_hz
    return np.sum(h[sel] * pxx[sel]) / np.sum(pxx[sel])


def group_delay_at(f, h, pxx, centre_hz, half_width_hz) -> float:
    """−dφ/dω at a frequency, from a power-weighted line fit to the phase."""
    sel = np.abs(f - centre_hz) <= half_width_hz
    phase = np.unwrap(np.angle(h[sel]))
    w = pxx[sel]
    ff = f[sel]
    fm = np.average(ff, weights=w)
    pm = np.average(phase, weights=w)
    slope = np.sum(w * (ff - fm) * (phase - pm)) / np.sum(w * (ff - fm) ** 2)
    return -slope / (2 * np.pi)


def occupied_band(f, psd, fraction: float = 0.99):
    """The band holding `fraction` of the power, with equal tails cut each side."""
    cumulative = np.cumsum(psd)
    cumulative /= cumulative[-1]
    tail = (1 - fraction) / 2
    return (float(np.interp(tail, cumulative, f)),
            float(np.interp(1 - tail, cumulative, f)))


def power_fraction_above(f, psd, cutoff_hz: float) -> float:
    return float(np.sum(psd[f > cutoff_hz]) / np.sum(psd))


# ---------------------------------------------------------------------------
# Readout optics (S11 note, from first principles)
# ---------------------------------------------------------------------------

READOUT_WAVELENGTH_M = 632.8e-9  # IEC 60857-1986 11.1.1


def readout_transfer(nu: float, rim_intensity: float = 0.23) -> float:
    """Linear readout transfer H(ν), ν in units of NA/λ (cut-off at ν = 2).

    H(ν) = ∫ a(η)·a(η − ν) d²η / ∫ a(η)² d²η over the unit pupil, with the
    amplitude a(ρ) = exp(−β·ρ²/2), β = −ln(rim_intensity) (S11 note,
    "Pupil"). The overlap of the two pupils is integrated across the
    shift direction analytically (a Gaussian chord, an erf) and along it with
    adaptive quadrature.
    """
    nu = abs(nu)
    if nu >= 2.0:
        return 0.0
    beta = -math.log(rim_intensity)

    def chord(x: float) -> float:
        half = min(math.sqrt(max(0.0, 1 - x * x)),
                   math.sqrt(max(0.0, 1 - (x - nu) ** 2)))
        along = math.exp(-0.5 * beta * (x * x + (x - nu) ** 2))
        if beta == 0.0:
            return along * 2 * half
        root = math.sqrt(beta)
        return along * math.sqrt(math.pi / beta) * special.erf(root * half)

    # The chord's half-width changes form at x = ν/2.
    left, _ = integrate.quad(chord, nu - 1, nu / 2, epsabs=1e-14, epsrel=1e-12,
                             limit=200)
    right, _ = integrate.quad(chord, nu / 2, 1, epsabs=1e-14, epsrel=1e-12,
                              limit=200)
    power = math.pi if beta == 0.0 else math.pi * (-math.expm1(-beta)) / beta
    return (left + right) / power


def readout_transfer_spatial(cycles_per_m: float, numerical_aperture: float = 0.40,
                             rim_intensity: float = 0.23) -> float:
    return readout_transfer(cycles_per_m * READOUT_WAVELENGTH_M / numerical_aperture,
                            rim_intensity)


# ---------------------------------------------------------------------------
# Disc geometry (S8 note)
# ---------------------------------------------------------------------------

NTSC_FRAME_PERIOD_S = 1001 / 30000  # IEC 60857-1986 4.7.1: one revolution per frame (CAV)
NTSC_LINE_PERIOD_S = 286 / 4.5e6  # SMPTE 170M: fH = 4.5 MHz / 286


@dataclass
class DiscGeometry:
    format: str
    start_radius_m: float
    track_pitch_m: float
    linear_velocity_m_s: float = 0.0

    def radius(self, t):
        if self.format == "cav":
            return self.start_radius_m + self.track_pitch_m * np.asarray(t) / NTSC_FRAME_PERIOD_S
        return np.sqrt(self.start_radius_m ** 2 + self.track_pitch_m *
                       self.linear_velocity_m_s * np.asarray(t) / math.pi)

    def speed(self, t):
        if self.format == "cav":
            return 2 * math.pi * self.radius(t) / NTSC_FRAME_PERIOD_S
        return np.full_like(np.asarray(t, dtype=float), self.linear_velocity_m_s)

    def arc_length(self, t):
        t = np.asarray(t)
        if self.format == "cav":
            r0, p, T = self.start_radius_m, self.track_pitch_m, NTSC_FRAME_PERIOD_S
            return 2 * math.pi / T * (r0 * t + p * t * t / (2 * T))
        return self.linear_velocity_m_s * t


@dataclass
class RunPaths:
    """Where one ld-creator run and its taps live."""
    directory: Path
    taps: dict = field(default_factory=dict)

    def tap(self, stage: str):
        return read_tap(self.taps[stage])
