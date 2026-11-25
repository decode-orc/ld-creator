# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns
"""NTSC round trip: videosynth → ld-creator → ld-decode → comparator.

Plan Tasks 7.2 and 7.4. Runs the committed projects in
tests/functional/projects/ into the build tree only, decodes each RF with
the pinned ld-decode, and compares the result with the videosynth truth by
docs-tech/user/ntsc-round-trip-metrics.md (scripts/cvbs_compare.py). Each
RF is also decoded by scripts/reference_demod.py, a demodulator written from
IEC 60857 alone, to show whether the RF itself carries the right signal.

    nix develop "git+file://$PWD" --command python3 -E scripts/round_trip.py

Exit status: 0 when every judged prediction is confirmed, 1 when one is
refuted, 2 when a tool fails or the comparator's own check fails.
"""

from __future__ import annotations

import argparse
import datetime
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import cvbs_compare as cc
import reference_demod as rd

REPO = Path(__file__).resolve().parent.parent
PROJECTS = REPO / "tests" / "functional" / "projects"
VIDEOSYNTH_PROJECT = PROJECTS / "ntsc_round_trip_videosynth.yaml"
TRUTH = "round_trip.cvbs"


@dataclass(frozen=True)
class Baseline:
    name: str
    title: str
    ld_decode_args: tuple
    # Whether the RF carries the S11 readout MTF, which the reference
    # demodulator then divides out.
    has_mtf: bool = True


# Metrics document, "ld-decode settings": disc RF with ld-decode's defaults;
# RF that carries no readout MTF with -m 0.
BASELINES = {
    b.name: b for b in (
        Baseline("carrier", "Carrier diagnostic", ("-m", "0"), has_mtf=False),
        Baseline("cav_inner", "Ideal disc, CAV, 55 mm", ()),
        Baseline("cav_outer", "Ideal disc, CAV, 144.83-145 mm", ()),
        Baseline("clv", "Ideal disc, CLV, 10.1 m/s from 55 mm", ()),
        Baseline("cav_inner_equalised",
                 "Ideal disc, CAV, 55 mm, S12 diagnostic equaliser (bisection run)",
                 ("-m", "0"), has_mtf=False),
    )
}
# The plan's Task 7.4 baselines. cav_inner_equalised is the metrics
# document's MTF bisection run, run by name when triage needs it.
DEFAULT_BASELINES = ("carrier", "cav_inner", "cav_outer", "clv")


class Log:
    def __init__(self, path: Path):
        self.file = path.open("w")

    def run(self, command, cwd=None):
        command = [str(c) for c in command]
        self.file.write("$ " + " ".join(command) + "\n")
        self.file.flush()
        start = time.monotonic()
        subprocess.run(command, check=True, cwd=cwd, stdout=self.file,
                       stderr=subprocess.STDOUT)
        self.file.write(f"# {time.monotonic() - start:.1f} s\n")
        self.file.flush()


def capture(command) -> str:
    out = subprocess.run([str(c) for c in command], capture_output=True, text=True)
    text = (out.stdout or out.stderr).strip()
    return text.splitlines()[-1] if text else ""


def tool_versions(ld_creator: Path) -> dict:
    lock = json.loads((REPO / "flake.lock").read_text())["nodes"]
    dirty = capture(["git", "-C", REPO, "status", "--porcelain"])
    return {
        "ld-creator commit": capture(["git", "-C", REPO, "rev-parse", "--short", "HEAD"])
        + ("-dirty" if dirty else ""),
        "ld-creator build": capture([ld_creator, "--version"]),
        "videosynth": capture(["videosynth", "--version"]),
        "videosynth flake pin": lock["videosynth"]["locked"]["rev"],
        "ld-decode": capture(["ld-decode", "--version"]),
        "ld-decode flake pin": lock["ld-decode"]["locked"]["rev"],
    }


def self_check(truth: cc.CvbsFile) -> dict:
    """Plan Task 7.2: truth against itself passes; +2 IRE fails."""
    same = cc.compare(truth, truth)
    offset = cc.compare(truth, truth, offset_ire=2.0)
    return {"truth_vs_truth": same.verdict(), "plus_2_ire": offset.verdict(),
            "ok": same.verdict() == "confirmed" and offset.verdict() == "refuted",
            "plus_2_ire_refuted": [s["key"] for s in offset.summaries()
                                   if s["verdict"] == "refuted"]}


def report(versions, check, results) -> str:
    out = ["# NTSC round trip results", "",
           f"Generated {datetime.datetime.now(datetime.timezone.utc):%Y-%m-%d %H:%M} UTC by "
           "`scripts/round_trip.py`.", "", "## Tools", "",
           "| Tool | Version |", "|------|---------|"]
    out += [f"| {k} | `{v}` |" for k, v in versions.items()]
    out += ["", "## Comparator check", "",
            f"- Truth against itself: **{check['truth_vs_truth']}**.",
            f"- Truth against truth + 2 IRE: **{check['plus_2_ire']}** "
            f"({', '.join(check['plus_2_ire_refuted'])}).", ""]
    for name, r in results.items():
        b = BASELINES[name]
        out += [f"## {b.title}", "",
                f"ld-decode command: `{r['ld_decode']}`. Frames: truth {r['truth_frames']}, "
                f"decoded {r['decoded_frames']}, paired {len(r['pairs'])}. "
                f"Verdict: **{r['verdict']}**.", ""]
        out.append(r["table"])
        if r["discontinuities"]:
            out += ["", "Discontinuities:", ""]
            out += [f"- {n}" for n in r["discontinuities"]]
        out.append("")
        if "reference" in r:
            ref = r["reference"]
            out += [f"### {b.title}: reference demodulator", "",
                    "`scripts/reference_demod.py` (IEC 60857 only) with "
                    f"`{ref['options'].replace(str(REPO) + '/', '')}`. Verdict: **{ref['verdict']}**.",
                    "", ref["table"], ""]
    return "\n".join(out)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", default=REPO / "build", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        help="where to write (default: <build-dir>/round_trip)")
    parser.add_argument("--baselines", nargs="*", default=list(DEFAULT_BASELINES),
                        choices=sorted(BASELINES))
    parser.add_argument("--no-reference", action="store_true",
                        help="skip the reference IEC demodulator")
    parser.add_argument("--threads", type=int, default=0,
                        help="ld-creator threads (0: its default); output does not depend on it")
    args = parser.parse_args(argv)

    build = args.build_dir.resolve()
    ld_creator = build / "ld-creator"
    out = (args.output_dir or build / "round_trip").resolve()
    out.mkdir(parents=True, exist_ok=True)
    log = Log(out / "runs.log")
    try:
        versions = tool_versions(ld_creator)
        log.file.write(json.dumps(versions, indent=1) + "\n")
        log.run(["videosynth", "--project", VIDEOSYNTH_PROJECT, "--output-root", out])
        truth = cc.read_cvbs(out / TRUTH)
        check = self_check(truth)
        print(f"Comparator check: truth {check['truth_vs_truth']}, "
              f"+2 IRE {check['plus_2_ire']}")
        if not check["ok"]:
            print("The comparator's own check failed", file=sys.stderr)
            return 2
        results = {}
        for name in args.baselines:
            b = BASELINES[name]
            command = [ld_creator, "--project", PROJECTS / f"ntsc_round_trip_{name}.yaml",
                       "--output-root", out]
            if args.threads:
                command += ["--threads", args.threads]
            log.run(command)
            decoded = out / f"{name}_decoded"
            ld_decode = ["ld-decode", "--NTSC", *b.ld_decode_args, f"{name}.s16", decoded.name]
            log.run(ld_decode, cwd=out)
            comparison = cc.compare(truth, cc.read_cvbs(decoded.with_suffix(".cvbs")))
            results[name] = {**comparison.to_json(), "ld_decode": " ".join(ld_decode),
                             "table": cc.markdown_table(comparison)}
            print(f"{b.title}: {comparison.verdict()}")
            if not args.no_reference:
                reference = out / f"{name}_reference.cvbs"
                options = ["--playback-equaliser"]
                if b.has_mtf:
                    options += ["--mtf", str(out / f"{name}.resolved.yaml")]
                log.file.write(f"# reference_demod {name}.s16 {' '.join(options)}\n")
                rd.main([str(out / f"{name}.s16"), str(out / TRUTH), str(reference), *options])
                ref = cc.compare(truth, cc.read_cvbs(reference))
                results[name]["reference"] = {
                    "options": " ".join(options), "verdict": ref.verdict(),
                    "metrics": ref.summaries(), "table": cc.markdown_table(ref)}
                print(f"{b.title}, reference demodulator: {ref.verdict()}")
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        print(f"Round trip failed: {error}", file=sys.stderr)
        return 2
    finally:
        log.file.close()

    (out / "results.json").write_text(json.dumps(
        {"versions": versions, "self_check": check,
         "baselines": {k: {x: (y if x != "reference" else
                               {p: q for p, q in y.items() if p != "table"})
                           for x, y in v.items() if x != "table"}
                       for k, v in results.items()}}, indent=1))
    (out / "results.md").write_text(report(versions, check, results))
    print(f"Results: {out / 'results.md'}")
    return 0 if all(r["verdict"] == "confirmed" for r in results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
