# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Stage-by-stage checks and RF occupancy survey, without ld-decode.

Plan Tasks 6.2 and 6.3. Runs the pinned videosynth and ld-creator on the
projects in scripts/projects/, with diagnostic taps, into
<build>/verification/, then checks each tap against its stage note using
rf_analysis.py. Writes results.md and results.json there, and exits with
status 1 if any check fails.

    nix develop "git+file://$PWD" --command python3 -E scripts/verify_stages.py

Options: --build-dir (default build), --skip-patterns (Task 6.2 only).
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

import rf_analysis as ra

REPO = Path(__file__).resolve().parents[1]
PROJECTS = REPO / "scripts" / "projects"

# Frames of patterns_videosynth.yaml, in order: the legal-range patterns.
PATTERNS = [
    "BLACK", "75_BARS", "SMPTE_BARS_001", "PLUGE", "GREY_5H_STEP",
    "LUMA_RAMP", "LEGAL_RAMP", "MULTIBURST", "LUMA_ZONE_PLATE",
    "CHROMA_ZONE_PLATE", "GREEN_MAGENTA_EDGES", "TARTAN", "PHOTO_BUTTERFLY",
]

# --- Nominal model values, from the stage notes ---------------------------

# S5 note: f = f_b + k·v, f_b = 8.1 MHz, k = 1.7 MHz / 140 IRE.
BLANKING_HZ = 8.1e6
DEVIATION_HZ = 1.7e6
HZ_PER_IRE = DEVIATION_HZ / 140.0
S5_TOLERANCE_HZ = 5e3  # plan Task 6.2

FSC_HZ = 315e6 / 88  # SMPTE 170M
LINE_S = ra.NTSC_LINE_PERIOD_S

# S7 note: 4 ps edge-time budget for S2 + S5 + S7.
S7_BUDGET_S = 4e-12
# The reference crossings (Lagrange on φ alone) lose accuracy across the
# step to 0 IRE at the run's ends; S7 is checked away from them.
RUN_END_MARGIN_S = 1e-6

# S3 note / IEC 60857 §9.1.7: (MHz, nominal ns, tolerance ns), relative to 0.5 MHz.
S3_TABLE = [(2.0, -15, 15), (3.0, -45, 15), (3.58, -80, 15), (4.0, -135, 30),
            (4.2, -200, 50)]
S3_REALISED_NS = {2.0: -15.6, 3.0: -46.3, 3.58: -81.7, 4.0: -141.1, 4.2: -193.2}
S3_MAGNITUDE_DB = 0.1  # plan Task 3.2, to 4.2 MHz
# S4 note: (1 + s·t1)/(1 + s·t2), within 0.05 dB from 0.1 to 6 MHz (plan Task 3.3).
S4_T1_S, S4_T2_S = 320e-9, 120e-9
S4_MAGNITUDE_DB = 0.05

# S11 note: kernel transfer within 1.1e-4 of H from 110 cycles/mm.
S11_TRANSFER_BOUND = 1.1e-4
HFMI_REFERENCE_PER_M = 802e3  # IEC 60857 §12.3

# S13 note: stop-band attenuation from 20 MHz.
CAPTURE_NYQUIST_HZ = 20e6
STOPBAND_DB = {"domesday_duplicator": 47.3, "ideal": 98.0}

# S4 note: instantaneous carrier limits for an ideal step.
OVERSHOOT_LIMITS_HZ = (4.78e6, 12.15e6)

# fcc-multiburst packets on line 18 (videosynth's VITS definition): MHz, µs from 0H.
MULTIBURST = [(0.5, 18.2, 26.7), (1.25, 28.2, 34.2), (2.0, 35.2, 40.2),
              (3.0, 41.2, 46.2), (FSC_HZ / 1e6, 47.2, 52.2), (4.1, 53.2, 58.2)]
MULTIBURST_LINE = 18
PACKET_MARGIN_US = 1.0

TRACK_PITCH_UM = 1.6667
INNER_MM = 55.0
OUTER_MM = 144.99  # one frame must end at or inside 145 mm (IEC 60857 §4.12)


def level_windows():
    """Constant-level windows of one NTSC frame: (name, IRE, [(t0, t1)...]).

    Times are from 0H of frame line 1. Sync tip: the broad pulses of field 1
    lines 4–6 and field 2 lines 267–268, 3 to 24 µs into each half-line.
    Blanking: the blank VBI lines 10–16 and 273–279, 12–60 µs. White: the
    NTC-7 white bar on line 17, 15–27 µs (bar 12–30 µs). The margins let S3's
    ±1.7 µs FIR and S4's 120 ns relaxation settle.
    """
    def line(n):
        return (n - 1) * LINE_S

    sync = [(line(n) + h + 3e-6, line(n) + h + 24e-6)
            for n in (4, 5, 6, 267, 268) for h in (0.0, LINE_S / 2)]
    blank = [(line(n) + 12e-6, line(n) + 60e-6)
             for n in list(range(10, 17)) + list(range(273, 280))]
    white = [(line(17) + 15e-6, line(17) + 27e-6)]
    return [("sync tip", -40.0, sync), ("blanking", 0.0, blank),
            ("white", 100.0, white)]


# --- Running the tools ---------------------------------------------------------


def run(command, log):
    log.write("$ " + " ".join(str(c) for c in command) + "\n")
    log.flush()
    subprocess.run([str(c) for c in command], check=True, stdout=log,
                   stderr=subprocess.STDOUT)


def tool_version(command) -> str:
    out = subprocess.run([str(c) for c in command] + ["--version"],
                         capture_output=True, text=True)
    return (out.stdout or out.stderr).strip().splitlines()[-1]


@dataclass
class Run:
    name: str
    directory: Path
    geometry: ra.DiscGeometry
    profile: str
    taps: dict

    def tap(self, stage):
        return ra.read_tap(self.taps[stage])

    @property
    def rf_path(self):
        return self.directory / "rf.s16"

    def hfmi(self) -> float:
        text = (self.directory / "rf.resolved.yaml").read_text()
        return float(re.search(r"^\s+hfmi: (\S+)$", text, re.M).group(1))


class Runner:
    def __init__(self, build_dir: Path):
        self.out = build_dir / "verification"
        self.out.mkdir(parents=True, exist_ok=True)
        self.ld_creator = build_dir / "ld-creator"
        self.log = open(self.out / "runs.log", "w")

    def videosynth(self, project: Path) -> None:
        run(["videosynth", "--project", project, "--output-root", self.out],
            self.log)

    def disc(self, name, cvbs, frame, geometry, profile, stages) -> Run:
        directory = self.out / name
        (directory / "taps").mkdir(parents=True, exist_ok=True)
        taps = {s: directory / "taps" / s for s in stages}
        cutter = {"format": geometry.format,
                  "start_radius_mm": geometry.start_radius_m * 1e3,
                  "track_pitch_um": geometry.track_pitch_m * 1e6}
        if geometry.format == "clv":
            cutter["linear_velocity_m_s"] = geometry.linear_velocity_m_s
        project = {
            "project": {"name": name, "description": "plan Task 6 verification run"},
            "input": {"video_path": str(cvbs), "first_frame": frame,
                      "frame_count": 1},
            "disc_cutter": cutter,
            "rf_capture": {"profile": profile},
            "output": {"rf_path": str(directory / "rf.s16")},
            "diagnostics": {"taps": [{"stage": s, "path": str(p)}
                                     for s, p in taps.items()]},
        }
        # JSON is a subset of YAML, so the project is written as JSON.
        path = directory / "project.yaml"
        path.write_text(json.dumps(project, indent=2) + "\n")
        run([self.ld_creator, "--project", path], self.log)
        return Run(name, directory, geometry, profile, taps)


def cav(radius_mm):
    return ra.DiscGeometry("cav", radius_mm * 1e-3, TRACK_PITCH_UM * 1e-6)


def clv(velocity):
    return ra.DiscGeometry("clv", INNER_MM * 1e-3, TRACK_PITCH_UM * 1e-6, velocity)


# --- Checks (plan Task 6.2) --------------------------------------------------------


@dataclass
class Check:
    name: str
    passed: bool


def check_s5(d1, rf_run):
    """Carrier at sync tip, blanking and white, from φ, the photocurrent and the RF."""
    d3 = rf_run.tap("fm_phase_modulator")
    d8 = rf_run.tap("optical_readout")
    rf = ra.read_rf_s16(rf_run.rf_path, rf_run.profile)
    band = (2.5e6, 17e6)
    z8 = ra.unwrapped_phase(ra.analytic_signal(d8.samples, d8.rate_hz, band))
    zrf = ra.unwrapped_phase(ra.analytic_signal(rf, 40e6, band))
    rows, ok = [], True
    for name, ire, windows in level_windows():
        nominal = BLANKING_HZ + HZ_PER_IRE * ire
        worst = {"d1": 0.0, "d3": 0.0, "d8": 0.0, "rf": 0.0}
        for t0, t1 in windows:
            _, v = d1.window(t0, t1)
            worst["d1"] = max(worst["d1"], abs(v.mean() - ire), key=abs)
            t, phi = d3.window(t0, t1)
            dev = ra.phase_slope_frequency(t, phi) - nominal
            worst["d3"] = max(worst["d3"], dev, key=abs)
            t, _ = d8.window(t0, t1)
            n = np.arange(d8.index_of(t[0]), d8.index_of(t[0]) + t.size)
            dev = ra.phase_slope_frequency(t, z8[n]) - nominal
            worst["d8"] = max(worst["d8"], dev, key=abs)
            n = np.arange(math.ceil(t0 * 40e6), math.ceil(t1 * 40e6))
            dev = ra.phase_slope_frequency(n / 40e6, zrf[n]) - nominal
            worst["rf"] = max(worst["rf"], dev, key=abs)
        passed = all(abs(worst[k]) <= S5_TOLERANCE_HZ for k in ("d3", "d8", "rf"))
        ok &= passed
        rows.append({"level": name, "ire": ire, "windows": len(windows),
                     "nominal_hz": nominal, "d1_level_error_ire": worst["d1"],
                     "phase_error_hz": worst["d3"],
                     "photocurrent_error_hz": worst["d8"],
                     "rf_error_hz": worst["rf"], "pass": passed})
    return rows, ok


def check_filter(x_tap, y_tap, magnitude_target, f_points):
    f, h, pxx = ra.transfer_function(x_tap.samples, y_tap.samples, x_tap.rate_hz)
    out = []
    for fp in f_points:
        avg = ra.band_average_response(f, h, pxx, fp, 20e3)
        out.append((fp, 20 * math.log10(abs(avg)) - magnitude_target(fp),
                    math.degrees(math.atan2(avg.imag, avg.real))))
    return f, h, pxx, out


def check_s3(d2a, d2b):
    f, h, pxx, mags = check_filter(
        d2a, d2b, lambda _: 0.0, np.arange(0.1e6, 4.2e6 + 1, 0.1e6))
    worst_mag = max((m for _, m, _ in mags), key=abs)
    ref = ra.group_delay_at(f, h, pxx, 0.5e6, 50e3)
    rows, ok = [], abs(worst_mag) <= S3_MAGNITUDE_DB
    for mhz, nominal, tol in S3_TABLE:
        rel = (ra.group_delay_at(f, h, pxx, mhz * 1e6, 50e3) - ref) * 1e9
        passed = abs(rel - nominal) <= tol / 3
        ok &= passed
        rows.append({"mhz": mhz, "nominal_ns": nominal, "measured_ns": rel,
                     "note_realised_ns": S3_REALISED_NS[mhz],
                     "middle_third_ns": tol / 3, "pass": passed})
    return {"delay": rows, "worst_magnitude_db": worst_mag,
            "magnitude_pass": abs(worst_mag) <= S3_MAGNITUDE_DB}, ok


def s4_response(f):
    s = 2j * np.pi * f
    return (1 + s * S4_T1_S) / (1 + s * S4_T2_S)


def check_s4(d2b, d2c):
    points = [0.1e6, 0.5e6, 1e6, 2e6, 3e6, FSC_HZ, 4.2e6, 5e6, 6e6]
    f, h, pxx, mags = check_filter(
        d2b, d2c, lambda fp: 20 * math.log10(abs(s4_response(fp))), points)
    ok = True
    rows = []
    for (fp, mag_err, phase_deg) in mags:
        analytic = s4_response(fp)
        tau = ra.group_delay_at(f, h, pxx, fp, 50e3) * 1e9
        w = 2 * np.pi * fp
        # τ = −d(arg H)/dω for arg H = atan(ωt1) − atan(ωt2).
        tau_analytic = -(S4_T1_S / (1 + (w * S4_T1_S) ** 2)
                         - S4_T2_S / (1 + (w * S4_T2_S) ** 2)) * 1e9
        passed = abs(mag_err) <= S4_MAGNITUDE_DB
        ok &= passed
        rows.append({"mhz": fp / 1e6,
                     "analytic_db": 20 * math.log10(abs(analytic)),
                     "magnitude_error_db": mag_err,
                     "phase_error_deg": phase_deg - math.degrees(np.angle(analytic)),
                     "group_delay_ns": tau, "analytic_group_delay_ns": tau_analytic,
                     "pass": passed})
    # Input content at 6 MHz, relative to the spectrum's peak.
    content_db = 10 * math.log10(
        ra.band_average_response(f, pxx, pxx, 6e6, 20e3).real / pxx.max())
    return {"points": rows, "input_level_6mhz_db": content_db}, ok


def check_s7(r: Run):
    d3 = r.tap("fm_phase_modulator")
    d4 = r.tap("recording_limiter").records
    phi = d3.samples
    # S7 note, bracketing: crossing k lies in the interval with
    # φ_m < (k + ½)π ≤ φ_m+1, so the run holds k with φ_0 < (k + ½)π ≤ φ_end.
    first_k = math.floor(phi[0] / math.pi - 0.5) + 1
    last_k = math.floor(phi[-1] / math.pi - 0.5)
    expected_count = last_k - first_k + 1
    targets = (np.arange(d4.size) + first_k + 0.5) * np.pi
    reference = ra.phase_crossings(phi, d3.rate_hz, d3.t0, targets)
    t = d4["time_s"]
    end = d3.t0 + (phi.size - 1) / d3.rate_hz
    interior = (t > d3.t0 + RUN_END_MARGIN_S) & (t < end - RUN_END_MARGIN_S)
    error = np.abs(t - reference)[interior]
    k = np.arange(d4.size) + first_k
    # Pit where cos φ > 0: entered at φ = (2n + 1.5)π, left at (2n + 0.5)π.
    expected_polarity = np.where(k % 2 == 0, -1, 1)
    polarity_ok = bool(np.all(d4["polarity"] == expected_polarity))
    # The tap ends at the frame's last whole Fm sample, so edges after it
    # cannot be checked from φ; the count covers t ≤ the last sample.
    checked = int(np.count_nonzero(t <= end))
    passed = (checked == expected_count and polarity_ok
              and float(error.max()) <= S7_BUDGET_S)
    return {"edges": checked, "expected_edges": expected_count,
            "edges_after_last_sample": int(d4.size) - checked,
            "polarity_ok": polarity_ok,
            "max_error_ps": float(error.max()) * 1e12,
            "rms_error_ps": float(np.sqrt(np.mean(error ** 2))) * 1e12,
            "pass": passed}, passed


def check_s8(r: Run):
    d4 = r.tap("recording_limiter").records
    d5 = r.tap("disc_cutter").records
    t = d4["time_s"]
    ds = d5["arc_length_m"] - r.geometry.arc_length(t)
    dr = d5["radius_m"] - r.geometry.radius(t)
    dt = np.abs(ds) / r.geometry.speed(t)
    passed = (bool(np.all(d5["polarity"] == d4["polarity"]))
              and float(dt.max()) <= S7_BUDGET_S)
    return {"max_arc_error_nm": float(np.abs(ds).max()) * 1e9,
            "max_radius_error_nm": float(np.abs(dr).max()) * 1e9,
            "max_time_equivalent_ps": float(dt.max()) * 1e12, "pass": passed}, passed


def check_s11_depth(r: Run):
    d3 = r.tap("fm_phase_modulator")
    d8 = r.tap("optical_readout")
    h802 = ra.readout_transfer_spatial(HFMI_REFERENCE_PER_M)
    m = r.hfmi() / (4 / math.pi * h802)
    tolerance = 2 / math.pi * m * S11_TRANSFER_BOUND
    rows, ok = [], True
    for name, _, windows in level_windows():
        worst = {"h1": 0.0, "h3": 0.0, "mean": 0.0}
        sample = None
        for t0, t1 in windows:
            t, phi = d3.window(t0, t1)
            f = ra.phase_slope_frequency(t, phi)
            tm = 0.5 * (t0 + t1)
            v = float(r.geometry.speed(tm))
            spatial = f / v
            predicted1 = 2 / math.pi * m * ra.readout_transfer_spatial(spatial)
            predicted3 = 2 / (3 * math.pi) * m * ra.readout_transfer_spatial(3 * spatial)
            ts, x = d8.window(t0, t1)
            amps, mean = ra.fit_harmonics(ts, x, f)
            worst["h1"] = max(worst["h1"], amps[1] - predicted1, key=abs)
            worst["h3"] = max(worst["h3"], amps[3] - predicted3, key=abs)
            worst["mean"] = max(worst["mean"], mean - (1 - m / 2), key=abs)
            if sample is None:
                sample = (float(r.geometry.radius(tm)) * 1e3, v, f, spatial,
                          predicted1, amps[1], predicted3, amps[3])
        passed = all(abs(worst[k]) <= tolerance for k in worst)
        ok &= passed
        rows.append({"run": r.name, "level": name,
                     "radius_mm": sample[0], "speed_m_s": sample[1],
                     "carrier_mhz": sample[2] / 1e6,
                     "cycles_per_mm": sample[3] / 1e3,
                     "nu_over_fc": sample[3] * ra.READOUT_WAVELENGTH_M / (2 * 0.40),
                     "predicted_fundamental": sample[4], "measured_fundamental": sample[5],
                     "predicted_third": sample[6], "measured_third": sample[7],
                     "worst_fundamental_error": worst["h1"],
                     "worst_third_error": worst["h3"],
                     "worst_mean_error": worst["mean"],
                     "tolerance": tolerance, "pass": passed})
    return rows, ok


def narrowband_gain_db(carrier_hz, video_hz, speed):
    """S11 note: demodulated gain (a + b)/2 of a small video component."""
    h = lambda f: ra.readout_transfer_spatial(f / speed)
    centre = h(carrier_hz)
    return 20 * math.log10(0.5 * (h(carrier_hz - video_hz) + h(carrier_hz + video_hz)) / centre)


def check_s11_video_gain(r: Run):
    """Demodulated multiburst gain through the readout, against the S11 note."""
    d2c = r.tap("video_pre_emphasis")
    d3 = r.tap("fm_phase_modulator")
    d8 = r.tap("optical_readout")
    f_inst = ra.instantaneous_frequency(d8.samples, d8.rate_hz, (1e6, 18e6))
    t_inst = d8.t0 + (np.arange(f_inst.size) + 0.5) / d8.rate_hz
    line0 = (MULTIBURST_LINE - 1) * LINE_S
    rows = []
    for mhz, b0, b1 in MULTIBURST:
        t0 = line0 + (b0 + PACKET_MARGIN_US) * 1e-6
        t1 = line0 + (b1 - PACKET_MARGIN_US) * 1e-6
        fv = mhz * 1e6
        tv, v = d2c.window(t0, t1)
        video_amp, _, _ = ra.fit_sinusoid(tv, v, fv)
        sel = (t_inst >= t0) & (t_inst < t1)
        demod_amp, _, _ = ra.fit_sinusoid(t_inst[sel], f_inst[sel], fv)
        t, phi = d3.window(t0, t1)
        carrier = ra.phase_slope_frequency(t, phi)
        speed = float(r.geometry.speed(0.5 * (t0 + t1)))
        measured = 20 * math.log10(demod_amp / (HZ_PER_IRE * video_amp))
        rows.append({"run": r.name, "mhz": mhz, "carrier_mhz": carrier / 1e6,
                     "modulation_index": HZ_PER_IRE * video_amp / fv,
                     "measured_db": measured,
                     "narrowband_db": narrowband_gain_db(carrier, fv, speed)})
    return rows


# --- Survey (plan Task 6.3) ------------------------------------------------------


def survey_pattern(index, inner: Run, outer: Run, outer_ideal: Run):
    d3 = inner.tap("fm_phase_modulator")
    f = np.diff(d3.samples) * d3.rate_hz / (2 * np.pi)
    row = {"pattern": PATTERNS[index], "f_min_mhz": f.min() / 1e6,
           "f_max_mhz": f.max() / 1e6}
    row["within_s4_limits"] = bool(OVERSHOOT_LIMITS_HZ[0] <= f.min()
                                   and f.max() <= OVERSHOOT_LIMITS_HZ[1])
    # S11 note prediction: the overshoots are strongly attenuated by the
    # readout at the inner radius. H at the peak carrier, re H at blanking.
    v = float(inner.geometry.speed(0.0))
    row["readout_at_f_max_inner_db"] = 20 * math.log10(
        ra.readout_transfer_spatial(f.max() / v)
        / ra.readout_transfer_spatial(BLANKING_HZ / v))
    for label, r in (("inner", inner), ("outer", outer)):
        d9 = r.tap("player_front_end")
        fr, p = ra.power_spectrum(d9.samples, d9.rate_hz, 1 << 14)
        lo, hi = ra.occupied_band(fr, p, 0.99)
        row[f"occupied_{label}_mhz"] = (lo / 1e6, hi / 1e6)
        row[f"player_above_nyquist_{label}_db"] = 10 * math.log10(
            ra.power_fraction_above(fr, p, CAPTURE_NYQUIST_HZ))
    for label, r in (("ddd_inner", inner), ("ddd_outer", outer),
                     ("ideal_outer", outer_ideal)):
        u = r.tap("rf_capture")
        fr, p = ra.power_spectrum(u.samples, u.rate_hz, 1 << 14)
        row[f"residual_{label}_db"] = 10 * math.log10(
            ra.power_fraction_above(fr, p, CAPTURE_NYQUIST_HZ))
    row["ddd_rejection_outer_db"] = (row["player_above_nyquist_outer_db"]
                                     - row["residual_ddd_outer_db"])
    row["ideal_rejection_outer_db"] = (row["player_above_nyquist_outer_db"]
                                       - row["residual_ideal_outer_db"])
    return row


# --- Report -----------------------------------------------------------------------


def fmt(x, digits=3):
    return f"{x:.{digits}f}"


def report(results) -> str:
    lines = ["# Stage checks and RF occupancy (plan Tasks 6.2, 6.3)", "",
             f"ld-creator `{results['versions']['ld-creator']}`, "
             f"videosynth `{results['versions']['videosynth']}`.", ""]
    lines += ["## S5 carrier frequency", "",
              "| Level | Nominal | Windows | φ (D3) | Photocurrent (D8) | RF (.s16) | D1 level | Pass |",
              "|---|---|---|---|---|---|---|---|"]
    for r in results["s5"]:
        lines.append(f"| {r['level']} ({r['ire']:g} IRE) | {r['nominal_hz'] / 1e6:.6f} MHz "
                     f"| {r['windows']} | {r['phase_error_hz']:+.3f} Hz "
                     f"| {r['photocurrent_error_hz']:+.1f} Hz | {r['rf_error_hz']:+.1f} Hz "
                     f"| {r['d1_level_error_ire']:+.4f} IRE | {'yes' if r['pass'] else 'NO'} |")
    s3 = results["s3"]
    lines += ["", "## S3 group delay relative to 0.5 MHz", "",
              "| f (MHz) | Nominal (ns) | Measured (ns) | Note's realised (ns) | Middle third | Pass |",
              "|---|---|---|---|---|---|"]
    for r in s3["delay"]:
        lines.append(f"| {r['mhz']} | {r['nominal_ns']} | {r['measured_ns']:.1f} "
                     f"| {r['note_realised_ns']} | ±{r['middle_third_ns']:.1f} "
                     f"| {'yes' if r['pass'] else 'NO'} |")
    lines += ["", f"Largest magnitude error 0.1–4.2 MHz: {s3['worst_magnitude_db']:+.4f} dB "
              f"(limit ±{S3_MAGNITUDE_DB} dB)."]
    s4 = results["s4"]
    lines += ["", "## S4 pre-emphasis against (1 + s·t₁)/(1 + s·t₂)", "",
              "| f (MHz) | Analytic | Magnitude error | Phase error | Group delay | Analytic delay | Pass |",
              "|---|---|---|---|---|---|---|"]
    for r in s4["points"]:
        lines.append(f"| {r['mhz']:.4g} | {r['analytic_db']:+.3f} dB | {r['magnitude_error_db']:+.4f} dB "
                     f"| {r['phase_error_deg']:+.3f}° | {r['group_delay_ns']:+.2f} ns "
                     f"| {r['analytic_group_delay_ns']:+.2f} ns | {'yes' if r['pass'] else 'NO'} |")
    lines += ["", f"Input level at 6 MHz: {s4['input_level_6mhz_db']:.1f} dB re the spectrum's peak."]
    lines += ["", "## S7 edge timing and S8 track position", "",
              "| Run | Edges | Count | Polarity | S7 max | S7 rms | S8 arc error | S8 radius error | S8 time equiv. | Pass |",
              "|---|---|---|---|---|---|---|---|---|---|"]
    for name, s7, s8 in results["s7_s8"]:
        lines.append(f"| {name} | {s7['edges']} | {'ok' if s7['edges'] == s7['expected_edges'] else 'NO'} "
                     f"| {'ok' if s7['polarity_ok'] else 'NO'} | {s7['max_error_ps']:.3f} ps "
                     f"| {s7['rms_error_ps']:.3f} ps | {s8['max_arc_error_nm']:.3g} nm "
                     f"| {s8['max_radius_error_nm']:.3g} nm | {s8['max_time_equivalent_ps']:.3g} ps "
                     f"| {'yes' if s7['pass'] and s8['pass'] else 'NO'} |")
    lines += ["", "## S11 modulation depth against radius", "",
              "| Run | Level | r (mm) | v (m/s) | Carrier (MHz) | cycles/mm | ν/fc | Fundamental: predicted | measured | worst error | 3rd: predicted | measured | Mean error | Pass |",
              "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in results["s11_depth"]:
        lines.append(f"| {r['run']} | {r['level']} | {r['radius_mm']:.3f} | {r['speed_m_s']:.3f} "
                     f"| {r['carrier_mhz']:.4f} | {r['cycles_per_mm']:.1f} | {r['nu_over_fc']:.3f} "
                     f"| {r['predicted_fundamental']:.5f} | {r['measured_fundamental']:.5f} "
                     f"| {r['worst_fundamental_error']:+.1e} | {r['predicted_third']:.5f} "
                     f"| {r['measured_third']:.5f} | {r['worst_mean_error']:+.1e} "
                     f"| {'yes' if r['pass'] else 'NO'} |")
    lines += ["", f"Tolerance: (2/π)·m·{S11_TRANSFER_BOUND:g} = {results['s11_depth'][0]['tolerance']:.1e}.",
              "", "## S11 demodulated video gain (multiburst, line 18)", "",
              "| Run | f (MHz) | Carrier (MHz) | Index | Measured | Narrow-band estimate |",
              "|---|---|---|---|---|---|"]
    for r in results["s11_gain"]:
        lines.append(f"| {r['run']} | {r['mhz']:.3f} | {r['carrier_mhz']:.3f} | {r['modulation_index']:.2f} "
                     f"| {r['measured_db']:+.2f} dB | {r['narrowband_db']:+.2f} dB |")
    if results.get("patterns"):
        lines += ["", "## RF occupancy by test pattern (Task 6.3)", "",
                  f"Inner radius {INNER_MM} mm, outer {OUTER_MM} mm (CAV). Occupied band: 99 % of the "
                  "player RF's AC power. Residual: capture-filter output power above 20 MHz, re its total.", "",
                  "| Pattern | f min (MHz) | f max (MHz) | In S4 limits | H(f max) re blanking, inner | Occupied, inner (MHz) | Occupied, outer (MHz) "
                  "| Player RF > 20 MHz, outer | Residual DdD inner | Residual DdD outer | Residual ideal outer "
                  "| DdD rejection | Ideal rejection |",
                  "|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in results["patterns"]:
            lines.append(
                f"| {r['pattern']} | {r['f_min_mhz']:.3f} | {r['f_max_mhz']:.3f} "
                f"| {'yes' if r['within_s4_limits'] else 'NO'} "
                f"| {r['readout_at_f_max_inner_db']:.1f} dB "
                f"| {r['occupied_inner_mhz'][0]:.2f}–{r['occupied_inner_mhz'][1]:.2f} "
                f"| {r['occupied_outer_mhz'][0]:.2f}–{r['occupied_outer_mhz'][1]:.2f} "
                f"| {r['player_above_nyquist_outer_db']:.1f} dB | {r['residual_ddd_inner_db']:.1f} dB "
                f"| {r['residual_ddd_outer_db']:.1f} dB | {r['residual_ideal_outer_db']:.1f} dB "
                f"| {r['ddd_rejection_outer_db']:.1f} dB | {r['ideal_rejection_outer_db']:.1f} dB |")
    lines += ["", "## Summary", ""]
    for name, passed in results["checks"]:
        lines.append(f"- {name}: {'pass' if passed else 'FAIL'}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=Path, default=REPO / "build")
    parser.add_argument("--skip-patterns", action="store_true")
    args = parser.parse_args()

    runner = Runner(args.build_dir.resolve())
    results = {"versions": {"ld-creator": tool_version([runner.ld_creator]),
                            "videosynth": tool_version(["videosynth"])},
               "checks": []}
    print("videosynth: stage-check frame", flush=True)
    runner.videosynth(PROJECTS / "vits_videosynth.yaml")
    vits = runner.out / "vits.cvbs"

    all_taps = ["cvbs_input", "video_rate_converter", "group_delay_predistorter",
                "video_pre_emphasis", "fm_phase_modulator", "recording_limiter",
                "disc_cutter", "optical_readout", "rf_capture"]
    sweep_taps = ["video_pre_emphasis", "fm_phase_modulator", "recording_limiter",
                  "disc_cutter", "optical_readout"]
    print("ld-creator: radius sweep", flush=True)
    base = runner.disc("cav_055", vits, 0, cav(INNER_MM), "domesday_duplicator", all_taps)
    sweep = [base] + [runner.disc(name, vits, 0, g, "domesday_duplicator", sweep_taps)
                      for name, g in (("cav_080", cav(80.0)), ("cav_110", cav(110.0)),
                                      ("cav_145", cav(OUTER_MM)),
                                      ("clv_10.1", clv(10.1)), ("clv_11.4", clv(11.4)))]

    print("checking S5, S3, S4", flush=True)
    results["s5"], ok = check_s5(base.tap("cvbs_input"), base)
    results["checks"].append(("S5 carrier at sync tip, blanking and white within ±5 kHz", ok))
    results["s3"], ok = check_s3(base.tap("video_rate_converter"),
                                 base.tap("group_delay_predistorter"))
    results["checks"].append(("S3 group delay in the middle third; magnitude ±0.1 dB", ok))
    results["s4"], ok = check_s4(base.tap("group_delay_predistorter"),
                                 base.tap("video_pre_emphasis"))
    results["checks"].append(("S4 magnitude within 0.05 dB of the analytic shelf", ok))

    print("checking S7, S8, S11", flush=True)
    results["s7_s8"], all_ok = [], True
    results["s11_depth"], depth_ok = [], True
    results["s11_gain"] = []
    for r in sweep:
        s7, ok7 = check_s7(r)
        s8, ok8 = check_s8(r)
        results["s7_s8"].append((r.name, s7, s8))
        all_ok &= ok7 and ok8
        rows, ok = check_s11_depth(r)
        results["s11_depth"] += rows
        depth_ok &= ok
        if r.name in ("cav_055", "cav_145"):
            results["s11_gain"] += check_s11_video_gain(r)
    results["checks"].append(("S7 edges within 4 ps of φ = (k + ½)π; S8 track position", all_ok))
    results["checks"].append(("S11 modulation depth against radius", depth_ok))

    if not args.skip_patterns:
        print("videosynth: test patterns", flush=True)
        runner.videosynth(PROJECTS / "patterns_videosynth.yaml")
        patterns = runner.out / "patterns.cvbs"
        results["patterns"] = []
        for i, name in enumerate(PATTERNS):
            print(f"pattern {name}", flush=True)
            inner = runner.disc(f"pattern_{i:02d}_inner", patterns, i, cav(INNER_MM),
                                "domesday_duplicator",
                                ["fm_phase_modulator", "player_front_end", "rf_capture"])
            outer = runner.disc(f"pattern_{i:02d}_outer", patterns, i, cav(OUTER_MM),
                                "domesday_duplicator", ["player_front_end", "rf_capture"])
            ideal = runner.disc(f"pattern_{i:02d}_outer_ideal", patterns, i, cav(OUTER_MM),
                                "ideal", ["rf_capture"])
            results["patterns"].append(survey_pattern(i, inner, outer, ideal))

    text = report(results)
    (runner.out / "results.md").write_text(text)
    (runner.out / "results.json").write_text(json.dumps(results, indent=2, default=str))
    print(text)
    return 0 if all(passed for _, passed in results["checks"]) else 1


if __name__ == "__main__":
    sys.exit(main())
