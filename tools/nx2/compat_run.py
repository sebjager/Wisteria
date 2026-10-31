#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run every Switch 2 title once, unattended, and write a compatibility table.

Usage:
  compat_run.py --eden build/bin/eden-cli.exe --games games.json [--only NAME ...]
                [--seconds 300] [--hang-seconds N] [--show] [--dump-shaders] [--out compat_runs]
  compat_run.py --summarize compat_runs/<YYYYmmdd-HHMMSS>

games.json is a list of {"name", "path", optional "seconds", "key_script", "env": {}}; see
compat_games.example.json. Each title runs with EDEN_NX2_COMPAT_REPORT and EDEN_NX2_RUN_SECONDS
in <out>/<timestamp>/<name>/; the run folder gets compat.md, and a full run also <out>/latest.md.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

LOG_TAIL_BYTES = 2 * 1024 * 1024


def parse_report(path):
    report = {"status": None, "elapsed": 0.0, "presents": 0, "first_present": -1.0,
              "last_present": -1.0, "heartbeats": [], "stubs": [], "rejects": [], "last_ipc": []}
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


def run_title(args, game, run_dir):
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


def summarize(run_dir):
    run_dir = Path(run_dir)
    titles = []
    for out_dir in sorted(p for p in run_dir.iterdir() if p.is_dir()):
        summary_path = out_dir / "summary.json"
        if not summary_path.exists():
            continue
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        report_path = out_dir / "report.txt"
        report = parse_report(report_path) if report_path.exists() else None
        titles.append((summary, report))

    lines = [f"# Compatibility run {run_dir.name}", "",
             "| Title | Outcome | Milestone | Frames | First frame (s) | fps (last min) | "
             "Auto-stubbed | Shader rejects | Last capture |",
             "|---|---|---|---|---|---|---|---|---|"]
    for s, _ in titles:
        lines.append("| " + " | ".join(cell(v) for v in (
            s["name"], s["outcome"], s["milestone"], s["presents"], s["first_frame_s"],
            s["fps_last_minute"], s["auto_stubs"], s["shader_rejects"], s["last_capture"])) + " |")

    stubs = {}
    for s, report in titles:
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
    for s, report in titles:
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
    for s, report in titles:
        lines.append(f"### {s['name']}")
        lines.append("")
        for request in report["last_ipc"] if report else []:
            lines.append(f"- {request['service']}::{request['cmd']}")
        lines.append("")

    out = run_dir / "compat.md"
    out.write_text("\n".join(lines), encoding="utf-8")
    return out


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
    parser.add_argument("--summarize")
    args = parser.parse_args()

    if args.summarize:
        print(summarize(args.summarize))
        return 0
    if not args.eden or not args.games:
        parser.error("--eden and --games are required (or --summarize)")

    games = json.loads(Path(args.games).read_text(encoding="utf-8"))
    if args.only:
        games = [g for g in games if g["name"] in args.only]
    run_dir = Path(args.out) / time.strftime("%Y%m%d-%H%M%S")
    run_dir.mkdir(parents=True, exist_ok=True)
    for game in games:
        run_title(args, game, run_dir)
    compat = summarize(run_dir)
    if not args.only:
        shutil.copyfile(compat, Path(args.out) / "latest.md")
    print(compat)
    return 0


if __name__ == "__main__":
    sys.exit(main())
