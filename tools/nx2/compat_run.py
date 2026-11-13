#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run every Switch 2 title once, unattended, and write a compatibility table.

Usage:
  compat_run.py --eden build/bin/wisteria-cli.exe --games games.json [--only NAME ...]
                [--seconds 300] [--hang-seconds N] [--show] [--dump-shaders] [--out compat_runs]
                [--label NAME] [--baseline RUN] [--env NAME=VALUE ...]
  compat_run.py --summarize compat_runs/<YYYYmmdd-HHMMSS> [--baseline RUN]

games.json is a list of {"name", "path", optional "seconds", "key_script", "env": {}}; see
compat_games.example.json. Each title runs with EDEN_NX2_COMPAT_REPORT and EDEN_NX2_RUN_SECONDS
in <out>/<timestamp>[-<label>]/<name>/; the run folder gets compat.md, and a full run also
<out>/latest.md.

A run is compared with --baseline, or else with the newest earlier run in --out that has the
same label. Regressions are printed as "REGRESSION <title>: ..." and make the exit code 1.
"""
import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

LOG_TAIL_BYTES = 2 * 1024 * 1024
FRAMETIME_KEYS = ("count", "mean", "p50", "p95", "p99", "p999", "max", "over50")
GRID_W, GRID_H = 32, 18


def parse_report(path):
    report = {"status": None, "elapsed": 0.0, "presents": 0, "first_present": -1.0,
              "last_present": -1.0, "frametime": None, "heartbeats": [], "stubs": [],
              "rejects": [], "last_ipc": []}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        key, _, rest = line.partition(" ")
        if key == "status":
            report["status"] = rest
        elif key in ("elapsed", "first_present", "last_present"):
            report[key] = float(rest)
        elif key == "presents":
            report["presents"] = int(rest)
        elif key == "frametime":
            values = rest.split()
            report["frametime"] = {k: (int(v) if k in ("count", "over50") else float(v))
                                   for k, v in zip(FRAMETIME_KEYS, values)}
        elif key == "heartbeat":
            t, n = rest.split()
            report["heartbeats"].append((float(t), int(n)))
        elif key == "stub":
            parts = rest.split(" ", 4)
            report["stubs"].append({"count": int(parts[0]), "first": float(parts[1]),
                                    "service": parts[2], "cmd": int(parts[3]),
                                    "name": parts[4] if len(parts) > 4 else ""})
        elif key == "reject":
            parts = rest.split(" ", 3)
            report["rejects"].append({"count": int(parts[0]), "first": float(parts[1]),
                                      "kind": parts[2], "reason": parts[3] if len(parts) > 3 else ""})
        elif key == "last_ipc":
            t, service, cmd = rest.split()
            report["last_ipc"].append({"time": float(t), "service": service, "cmd": int(cmd)})
    return report


def fps_last_minute(heartbeats):
    if len(heartbeats) < 2:
        return 0.0
    end_t, end_n = heartbeats[-1]
    start = next((hb for hb in heartbeats if hb[0] >= end_t - 60.0), heartbeats[0])
    if start[0] >= end_t:
        start = heartbeats[-2]
    return (end_n - start[1]) / (end_t - start[0]) if end_t > start[0] else 0.0


def copy_log_tail(eden_dir, out_path):
    log = eden_dir / "user" / "log" / "eden_log.txt"
    if not log.exists():
        return
    with open(log, "rb") as src:
        src.seek(0, os.SEEK_END)
        size = src.tell()
        src.seek(max(0, size - LOG_TAIL_BYTES))
        out_path.write_bytes(src.read())


def git_commit(eden_dir):
    try:
        return subprocess.run(["git", "-C", str(eden_dir), "rev-parse", "--short", "HEAD"],
                              capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def run_title(args, game, run_dir, extra_env):
    name = game["name"]
    exe = Path(game["path"])
    if not exe.exists():
        print(f"{name}: skipped, {exe} does not exist")
        return None
    seconds = float(game.get("seconds", args.seconds))
    out_dir = run_dir / name
    out_dir.mkdir(parents=True, exist_ok=True)
    report_path = out_dir / "report.txt"
    if report_path.exists():
        report_path.unlink()

    eden = Path(args.eden).resolve()
    env = os.environ.copy()
    env["EDEN_NX2_COMPAT_REPORT"] = str(report_path)
    env["EDEN_NX2_RUN_SECONDS"] = f"{seconds:g}"
    env["EDEN_NX2_FRAME_CAPTURE"] = (out_dir / "frames").as_posix()
    env["EDEN_NX2_CRASH_DUMP"] = str(out_dir)
    if not args.show:
        env["EDEN_NX2_HIDE_WINDOW"] = "1"
    if game.get("key_script"):
        env["EDEN_NX2_KEY_SCRIPT"] = game["key_script"]
    if args.hang_seconds is not None:
        env["EDEN_NX2_HANG_SECONDS"] = f"{args.hang_seconds:g}"
    if args.dump_shaders:
        env["EDEN_NX2_SHADER_DUMP"] = (Path(args.out) / "shaders" / name).resolve().as_posix()
    env.update(extra_env)
    env.update({k: str(v) for k, v in game.get("env", {}).items()})

    print(f"{name}: running {exe} for {seconds:g} s", flush=True)
    killed = False
    begin = time.monotonic()
    with open(out_dir / "console.txt", "wb") as console:
        proc = subprocess.Popen([str(eden), "-g", str(exe)], cwd=eden.parent, env=env,
                                stdout=console, stderr=subprocess.STDOUT)
        try:
            exit_code = proc.wait(timeout=seconds * 2 + 180)
        except subprocess.TimeoutExpired:
            proc.kill()
            exit_code = proc.wait()
            killed = True
    wall = time.monotonic() - begin

    copy_log_tail(eden.parent, out_dir / "log_tail.txt")
    report = parse_report(report_path) if report_path.exists() else None

    if report is None:
        outcome = "no_report"
    elif killed:
        outcome = "killed"
    elif report["status"] in (None, "running"):
        outcome = "crash" if exit_code != 0 else "exit"
    else:
        outcome = report["status"]
    presents = report["presents"] if report else 0
    if report is None:
        milestone = "did not start"
    elif presents == 0:
        milestone = "booted, no frame"
    else:
        milestone = "frames shown"
    captures = sorted((out_dir / "frames").glob("*.tga")) if (out_dir / "frames").exists() else []

    summary = {
        "name": name,
        "path": str(exe),
        "exit_code": exit_code,
        "killed": killed,
        "wall_seconds": round(wall, 1),
        "outcome": outcome,
        "milestone": milestone,
        "presents": presents,
        "first_frame_s": report["first_present"] if report and report["first_present"] >= 0 else None,
        "fps_last_minute": round(fps_last_minute(report["heartbeats"]), 1) if report else 0.0,
        "frametime": report["frametime"] if report else None,
        "auto_stubs": len(report["stubs"]) if report else 0,
        "shader_rejects": len(report["rejects"]) if report else 0,
        "last_capture": captures[-1].name if captures else None,
    }
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"{name}: {outcome}, {milestone}, {presents} frames, exit {exit_code}, "
          f"{summary['fps_last_minute']} fps last minute, {summary['auto_stubs']} auto-stubs, "
          f"{summary['shader_rejects']} shader rejects, {wall:.0f} s", flush=True)
    return summary


def cell(value):
    if value is None:
        return "-"
    text = str(value)
    return text.replace("|", "\\|").replace("<", "&lt;").replace(">", "&gt;")


def fold(reason):
    reason = re.sub(r"0x[0-9a-fA-F]+", "#", reason)
    return re.sub(r"\b\d+\b", "N", reason)


def load_titles(run_dir):
    titles = {}
    for out_dir in sorted(p for p in Path(run_dir).iterdir() if p.is_dir()):
        summary_path = out_dir / "summary.json"
        if not summary_path.exists():
            continue
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        report_path = out_dir / "report.txt"
        report = parse_report(report_path) if report_path.exists() else None
        titles[summary["name"]] = (summary, report, out_dir)
    return titles


def read_tga_grid(path):
    """Average colour of each block of a GRID_W x GRID_H grid, sampling every 4th pixel."""
    data = path.read_bytes()
    id_length, _, image_type = data[0], data[1], data[2]
    width, height = struct.unpack_from("<HH", data, 12)
    bpp, descriptor = data[16], data[17]
    if image_type != 2 or bpp not in (24, 32) or width == 0 or height == 0:
        return None
    step = bpp // 8
    pixels = memoryview(data)[18 + id_length:]
    top_down = bool(descriptor & 0x20)
    sums = [[0, 0, 0, 0] for _ in range(GRID_W * GRID_H)]
    for y in range(0, height, 4):
        row = y if top_down else height - 1 - y
        block_y = y * GRID_H // height
        base = row * width * step
        for x in range(0, width, 4):
            offset = base + x * step
            b, g, r = pixels[offset], pixels[offset + 1], pixels[offset + 2]
            entry = sums[block_y * GRID_W + x * GRID_W // width]
            entry[0] += r
            entry[1] += g
            entry[2] += b
            entry[3] += 1
    return [(s[0] / s[3], s[1] / s[3], s[2] / s[3]) if s[3] else (0.0, 0.0, 0.0) for s in sums]


def compare_captures(before_dir, after_dir):
    """Returns (compared, went_black names, different names)."""
    before_frames = before_dir / "frames"
    after_frames = after_dir / "frames"
    if not before_frames.exists() or not after_frames.exists():
        return 0, [], []
    compared, black, different = 0, [], []
    for after in sorted(after_frames.glob("*.tga")):
        before = before_frames / after.name
        if not before.exists():
            continue
        grid_before = read_tga_grid(before)
        grid_after = read_tga_grid(after)
        if grid_before is None or grid_after is None:
            continue
        compared += 1
        newly_black = sum(1 for a, b in zip(grid_after, grid_before) if max(a) < 8 and max(b) >= 8)
        if newly_black > len(grid_after) * 0.25:
            black.append(after.name)
            continue
        diff = sum(abs(a[i] - b[i]) for a, b in zip(grid_after, grid_before) for i in range(3))
        if diff / (len(grid_after) * 3) > 40:
            different.append(after.name)
    return compared, black, different


def percent(after, before):
    return (after - before) / before * 100.0 if before else 0.0


def compare_runs(run_dir, baseline_dir):
    """Per title in both runs: (title, list of regressions, list of other changes)."""
    run = load_titles(run_dir)
    baseline = load_titles(baseline_dir)
    results = []
    for name, (summary, report, out_dir) in run.items():
        if name not in baseline:
            continue
        old_summary, old_report, old_dir = baseline[name]
        worse, notes = [], []
        if old_summary["outcome"] in ("limit", "exit") and summary["outcome"] not in ("limit", "exit"):
            worse.append(f"outcome {old_summary['outcome']} -> {summary['outcome']}")
        elif summary["outcome"] != old_summary["outcome"]:
            notes.append(f"outcome {old_summary['outcome']} -> {summary['outcome']}")
        if old_summary["presents"] and summary["presents"] < old_summary["presents"] * 0.5:
            worse.append(f"frames {old_summary['presents']} -> {summary['presents']}")

        old_ft, ft = old_summary.get("frametime"), summary.get("frametime")
        if old_ft and ft:
            limits = {"mean": 10.0, "p95": 15.0, "p99": 15.0}
            for key in ("mean", "p95", "p99"):
                delta = percent(ft[key], old_ft[key])
                text = f"{key} {old_ft[key]:.2f} -> {ft[key]:.2f} ms ({delta:+.0f}%)"
                (worse if delta > limits[key] else notes).append(text)

        if report and old_report:
            old_stubs = {(s["service"], s["cmd"]) for s in old_report["stubs"]}
            new_stubs = sorted({(s["service"], s["cmd"]) for s in report["stubs"]} - old_stubs)
            if new_stubs:
                worse.append("new auto-stubs " + ", ".join(f"{s}::{c}" for s, c in new_stubs))
            old_rejects = {fold(r["reason"]) for r in old_report["rejects"]}
            new_rejects = sorted({fold(r["reason"]) for r in report["rejects"]} - old_rejects)
            if new_rejects:
                worse.append("new shader rejects: " + "; ".join(new_rejects))

        compared, black, different = compare_captures(old_dir, out_dir)
        if black:
            worse.append("went black: " + ", ".join(black))
        if len(different) > max(1, compared // 4):
            worse.append(f"{len(different)} of {compared} captures look different")
        elif different:
            notes.append(f"{len(different)} of {compared} captures differ (animation?)")
        results.append((name, worse, notes))
    return results


def find_baseline(out_dir, run_dir, label):
    candidates = []
    for folder in Path(out_dir).iterdir():
        if not folder.is_dir() or folder.resolve() == Path(run_dir).resolve():
            continue
        run_json = folder / "run.json"
        if not run_json.exists():
            continue
        if json.loads(run_json.read_text(encoding="utf-8")).get("label") == label:
            candidates.append(folder)
    return max(candidates, key=lambda f: f.name) if candidates else None


def summarize(run_dir, baseline_dir=None):
    """Writes compat.md; returns (its path, titles that regressed)."""
    run_dir = Path(run_dir)
    titles = load_titles(run_dir)

    lines = [f"# Compatibility run {run_dir.name}", ""]
    regressed = []
    if baseline_dir is not None:
        results = compare_runs(run_dir, baseline_dir)
        regressed = [(name, worse) for name, worse, _ in results if worse]
        lines += [f"## Regressions against {Path(baseline_dir).name}", ""]
        if not regressed:
            lines += ["No regressions.", ""]
        lines += ["| Title | Worse | Other changes |", "|---|---|---|"]
        for name, worse, notes in results:
            lines.append("| " + " | ".join(cell(v) for v in (
                name, "; ".join(worse) or "-", "; ".join(notes) or "-")) + " |")
        lines.append("")

    lines += ["| Title | Outcome | Milestone | Frames | First frame (s) | fps (last min) | "
              "Frame ms mean / p95 / p99 | >50 ms | Auto-stubbed | Shader rejects | Last capture |",
              "|---|---|---|---|---|---|---|---|---|---|---|"]
    for s, _, _ in titles.values():
        ft = s.get("frametime")
        frame_ms = f"{ft['mean']:.2f} / {ft['p95']:.2f} / {ft['p99']:.2f}" if ft else None
        lines.append("| " + " | ".join(cell(v) for v in (
            s["name"], s["outcome"], s["milestone"], s["presents"], s["first_frame_s"],
            s["fps_last_minute"], frame_ms, ft["over50"] if ft else None, s["auto_stubs"],
            s["shader_rejects"], s["last_capture"])) + " |")

    stubs = {}
    for s, report, _ in titles.values():
        for stub in report["stubs"] if report else []:
            entry = stubs.setdefault((stub["service"], stub["cmd"], stub["name"]), {})
            entry[s["name"]] = (stub["count"], stub["first"])
    lines += ["", "## Auto-stubbed commands", "",
              "| Service | Cmd | Name | Titles | Calls | First call (s) |", "|---|---|---|---|---|---|"]
    for key, per in sorted(stubs.items(),
                           key=lambda kv: (-len(kv[1]), min(v[1] for v in kv[1].values()))):
        service, cmd, name = key
        lines.append("| " + " | ".join(cell(v) for v in (
            service, cmd, name, ", ".join(per),
            ", ".join(f"{t}: {c}" for t, (c, _) in per.items()),
            ", ".join(f"{t}: {f:.1f}" for t, (_, f) in per.items()))) + " |")

    rejects = {}
    for s, report, _ in titles.values():
        for reject in report["rejects"] if report else []:
            entry = rejects.setdefault(fold(reject["reason"]), {"kinds": set(), "titles": set(), "count": 0})
            entry["kinds"].add(reject["kind"])
            entry["titles"].add(s["name"])
            entry["count"] += 1
    lines += ["", "## Shader rejects (hex and numbers folded)", "",
              "| Reason | Kind | Titles | Programs |", "|---|---|---|---|"]
    for reason, entry in sorted(rejects.items(),
                                key=lambda kv: (-len(kv[1]["titles"]), -kv[1]["count"])):
        lines.append("| " + " | ".join(cell(v) for v in (
            reason, ", ".join(sorted(entry["kinds"])), ", ".join(sorted(entry["titles"])),
            entry["count"])) + " |")

    lines += ["", "## Last IPC requests per title", ""]
    for s, report, _ in titles.values():
        lines.append(f"### {s['name']}")
        lines.append("")
        for request in report["last_ipc"] if report else []:
            lines.append(f"- {request['service']}::{request['cmd']}")
        lines.append("")

    out = run_dir / "compat.md"
    out.write_text("\n".join(lines), encoding="utf-8")
    for name, worse in regressed:
        print(f"REGRESSION {name}: " + "; ".join(worse))
    return out, regressed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--eden")
    parser.add_argument("--games")
    parser.add_argument("--only", action="append")
    parser.add_argument("--seconds", type=float, default=300)
    parser.add_argument("--hang-seconds", type=float)
    parser.add_argument("--show", action="store_true")
    parser.add_argument("--dump-shaders", action="store_true")
    parser.add_argument("--out", default="compat_runs")
    parser.add_argument("--label")
    parser.add_argument("--baseline")
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--summarize")
    args = parser.parse_args()

    if args.summarize:
        compat, regressed = summarize(args.summarize, args.baseline)
        print(compat)
        return 1 if regressed else 0
    if not args.eden or not args.games:
        parser.error("--eden and --games are required (or --summarize)")
    extra_env = {}
    for item in args.env:
        key, sep, value = item.partition("=")
        if not sep:
            parser.error(f"--env {item}: expected NAME=VALUE")
        extra_env[key] = value

    games = json.loads(Path(args.games).read_text(encoding="utf-8"))
    if args.only:
        games = [g for g in games if g["name"] in args.only]
    stamp = time.strftime("%Y%m%d-%H%M%S")
    run_dir = Path(args.out) / (f"{stamp}-{args.label}" if args.label else stamp)
    run_dir.mkdir(parents=True, exist_ok=True)
    run_info = {"label": args.label, "env": extra_env,
                "commit": git_commit(Path(args.eden).resolve().parent), "seconds": args.seconds}
    (run_dir / "run.json").write_text(json.dumps(run_info, indent=2), encoding="utf-8")

    for game in games:
        run_title(args, game, run_dir, extra_env)
    baseline = Path(args.baseline) if args.baseline else find_baseline(args.out, run_dir, args.label)
    compat, regressed = summarize(run_dir, baseline)
    if not args.only:
        shutil.copyfile(compat, Path(args.out) / "latest.md")
    print(compat)
    return 1 if regressed else 0


if __name__ == "__main__":
    sys.exit(main())
