# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""Measure the same excerpt of several NTSC captures side by side.

The multi-disc comparison of docs-tech/user/ld-decode-findings.md ("Other
discs"). For each source it cuts an excerpt (a DomesdayDuplicator `.ldf` or
`.raw.oga`), or takes an `.s16` RF as it is (ld-creator's RF, or an excerpt
already cut). It then:

1. decodes it with `ld-decode --NTSC`, and again with `--no_auto_chroma`;
2. measures both decodes with `vits_measure.py cvbs`;
3. measures the RF itself through the IEC chain with
   `vits_measure.py rf --playback-equaliser`;
4. reads ld-decode's calibration and subcarrier lock from its log.

It writes one table, a column per source, to `<output-dir>/survey.md`.

    nix develop "git+file://$PWD" --command python3 -E -B scripts/capture_survey.py \\
        --output-dir build/captures --start 1000 --frames 40 \\
        "GGV1069 2020=<share>/GGV1069_...ldf" "ld-creator 55 mm=build/round_trip/cav_inner.s16"

Each source is `<label>=<path>`. Steps whose output already exists are
skipped, so an interrupted survey resumes; delete the output directory to
start again.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent

# (row label, measurement file, quantity in vits_measure's table, decimals)
ROWS = (
    ("RF: envelope, blanking / sync tip", "rf", "Envelope, blanking / sync tip", 2),
    ("RF: burst (IRE p-p)", "rf", "Burst (IRE p-p)", 1),
    ("RF: VIRS chroma (IRE p-p)", "rf", "VIRS chroma (IRE p-p)", 1),
    ("RF: multiburst 2 MHz re 1 MHz (dB)", "rf", "Multiburst 2 MHz re 1 MHz (dB)", 2),
    ("RF: multiburst 3.58 MHz re 1 MHz (dB)", "rf", "Multiburst 3.58 MHz re 1 MHz (dB)", 2),
    ("RF: multiburst 4.2 MHz re 1 MHz (dB)", "rf", "Multiburst 4.2 MHz re 1 MHz (dB)", 2),
    ("RF: 12.5T chroma / luma", "rf", "12.5T chroma / luma", 2),
    ("RF: 12.5T chroma − luma delay (ns)", "rf", "12.5T chroma − luma delay (ns)", 1),
    ("RF: DP, max − min (deg)", "rf", "DP, max − min (deg)", 1),
    ("No auto chroma: burst (IRE p-p)", "noac", "Burst (IRE p-p)", 1),
    ("No auto chroma: multiburst 3.58 MHz re 1 MHz (dB)", "noac",
     "Multiburst 3.58 MHz re 1 MHz (dB)", 2),
    ("Decode: burst 50 % duration (cycles)", "decoded", "Burst 50 % duration (cycles)", 2),
    ("Decode: VIRS black (IRE)", "decoded", "VIRS black (IRE)", 2),
    ("Decode: staircase chroma (IRE p-p)", "decoded", "Staircase chroma, mean (IRE p-p)", 1),
    ("Decode: VIRS chroma (IRE p-p)", "decoded", "VIRS chroma (IRE p-p)", 1),
    ("Decode: multiburst 2 MHz re 1 MHz (dB)", "decoded", "Multiburst 2 MHz re 1 MHz (dB)", 2),
    ("Decode: multiburst 3.58 MHz re 1 MHz (dB)", "decoded",
     "Multiburst 3.58 MHz re 1 MHz (dB)", 2),
    ("Decode: 12.5T chroma − luma delay (ns)", "decoded", "12.5T chroma − luma delay (ns)", 1),
    ("Decode: DP, max − min (deg)", "decoded", "DP, max − min (deg)", 1),
    ("Decode: DG, max − min (%)", "decoded", "DG, max − min (% of mean)", 1),
)

CALIBRATION = re.compile(r"MTF level ([\d.]+), chroma strength ([\d.]+)")
LOCK = re.compile(r"CVBS: wrote \d+ frames \S+, (\S+),.*burst residual max [\d.]+ deg rms ([\d.]+) deg")


def slug(label: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", label.lower()).strip("_")


def run(command, log: Path) -> None:
    with log.open("w") as out:
        subprocess.run([str(c) for c in command], check=True, stdout=out,
                       stderr=subprocess.STDOUT)


def read_table(path: Path) -> dict:
    values = {}
    for line in path.read_text().splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) == 4:
            try:
                values[cells[0]] = float(cells[1])
            except ValueError:
                pass
    return values


def survey(label: str, source: Path, out: Path, start: int, frames: int,
           python: str) -> dict:
    name = slug(label)
    if source.suffix == ".s16":
        rf = source
    else:
        rf = out / f"{name}.s16"
        if not rf.exists():
            run(["ld-cut", "-n", "-s", start, "-l", frames, source, rf],
                out / f"{name}.cut.out")
    result = {}
    for variant, extra in (("decoded", ()), ("noac", ("--no_auto_chroma",))):
        stem = out / f"{name}_{variant}"
        if not stem.with_suffix(".cvbs").exists():
            run(["ld-decode", "--NTSC", *extra, rf, stem], out / f"{name}_{variant}.out")
        table = out / f"{name}_{variant}.vits.md"
        if not table.exists():
            run([python, "-E", "-B", SCRIPTS / "vits_measure.py", "cvbs",
                 stem.with_suffix(".cvbs"), "--frames", "30"], table)
        result[variant] = read_table(table)
    table = out / f"{name}.rf.vits.md"
    if not table.exists():
        run([python, "-E", "-B", SCRIPTS / "vits_measure.py", "rf", rf,
             "--frames", "10", "--playback-equaliser"], table)
    result["rf"] = read_table(table)

    log = (out / f"{name}_decoded.log").read_text()
    calibration = CALIBRATION.findall(log)
    lock = LOCK.findall(log)
    result["mtf"], result["strength"] = calibration[-1] if calibration else ("-", "-")
    result["lock"] = (f"{lock[-1][0].removeprefix('STANDARD_STABLE_').lower()}, "
                      f"{lock[-1][1]}°") if lock else "-"
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sources", nargs="+", metavar="LABEL=PATH")
    parser.add_argument("--output-dir", type=Path, default=Path("build/captures"))
    parser.add_argument("--start", type=int, default=1000,
                        help="file frame the excerpt starts at (default 1000)")
    parser.add_argument("--frames", type=int, default=40,
                        help="excerpt length in frames (default 40)")
    args = parser.parse_args(argv)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    columns = []
    for item in args.sources:
        label, sep, path = item.partition("=")
        if not sep:
            parser.error(f"{item!r} is not LABEL=PATH")
        print(f"{label}: {path}", flush=True)
        try:
            columns.append((label, survey(label, Path(path), args.output_dir,
                                          args.start, args.frames, sys.executable)))
        except (subprocess.CalledProcessError, OSError) as error:
            print(f"  failed: {error}", file=sys.stderr)
            return 2

    lines = ["| Quantity | " + " | ".join(label for label, _ in columns) + " |",
             "|---" * (len(columns) + 1) + "|",
             "| ld-decode: MTF level | " + " | ".join(c["mtf"] for _, c in columns) + " |",
             "| ld-decode: chroma strength | "
             + " | ".join(c["strength"] for _, c in columns) + " |",
             "| ld-decode: lock, burst residual rms | "
             + " | ".join(c["lock"] for _, c in columns) + " |"]
    for row, measured, quantity, decimals in ROWS:
        cells = []
        for _, column in columns:
            value = column[measured].get(quantity)
            cells.append("-" if value is None else f"{value:.{decimals}f}")
        lines.append(f"| {row} | " + " | ".join(cells) + " |")
    text = "\n".join(lines) + "\n"
    (args.output_dir / "survey.md").write_text(text)
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
