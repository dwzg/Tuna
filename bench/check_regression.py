#!/usr/bin/env python3
# This is free and unencumbered software released into the public domain.
# For more information, please refer to <http://unlicense.org/>
"""Gate Tuna's flash / RAM / DSP-cycle metrics against a committed baseline.

Runs the benchmark for the shipped optimization config (by default) and compares
each metric to bench/baseline.json. Two independent checks:

  * Relative regression -- current value must not exceed baseline * (1 + tol).
    Catches "this change made it bigger/slower". Tolerances live in the baseline
    so they are tunable without editing code. Skipped (with a warning) when the
    avr-gcc version differs from the baseline's, because the numbers then shift
    for reasons unrelated to the source change -- re-baseline instead.

  * Absolute ceiling -- flash <= 64 KB, and static RAM <= SRAM minus a stack
    reserve (not the full 8 KB; the call stack and ISR frames need the rest).
    Toolchain-independent, so it is always enforced. This is what catches a
    constant table accidentally landing in RAM instead of flash.

Exit status is non-zero if any enforced check fails, so CI can gate on it.

  ./check_regression.py                 # check shipped config, both methods
  ./check_regression.py --update        # rewrite baseline.json from a fresh run
  ./check_regression.py --no-run        # reuse an existing out/results.tsv
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TSV = os.path.join(HERE, "out", "results.tsv")
DEFAULT_BASELINE = os.path.join(HERE, "baseline.json")
METRICS = ("flash", "ram", "cycles")
BUDGET_CYCLES = 6_000_000   # 250 ms @ 24 MHz; informational for cycle reporting

# Absolute ceilings (toolchain-independent). The RAM ceiling is the SRAM size
# minus a stack reserve, NOT the full 8 KB: static data must leave room for the
# call stack and the sample-counter ISR frames. With ~1.5 KB reserved the FFT
# path (5217 B static) keeps ~1.4 KB of headroom. A constant table that lands in
# RAM instead of flash (plain `const` without FLASH_RODATA) is exactly what this
# guards against -- it is how the FFT path once reached 7777 B / 95% of SRAM.
SRAM_BYTES = 8192
STACK_RESERVE = 1536
DEFAULT_CEILINGS = {"flash": 65536, "ram": SRAM_BYTES - STACK_RESERVE}


def avr_gcc_version():
    try:
        out = subprocess.run(["avr-gcc", "-dumpversion"],
                             capture_output=True, text=True, check=True)
        return out.stdout.strip()
    except Exception:
        return "unknown"


def run_benchmark(configs, methods):
    cmd = [os.path.join(HERE, "run_bench.sh"),
           "--configs", configs, "--methods", methods]
    print(f"running: {' '.join(cmd)}", flush=True)
    subprocess.run(cmd, check=True)


def read_tsv(path):
    """Return {'YIN/Os+lto': {'flash':..,'ram':..,'cycles':..}, ...}."""
    rows = {}
    with open(path) as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 5:
                continue
            method, config, flash, ram, cycles = parts
            if "ERR" in (flash, ram, cycles):
                raise SystemExit(f"benchmark reported ERR for {method}/{config} "
                                 f"-- a build or sim run failed; see bench/out/*.log")
            rows[f"{method}/{config}"] = {
                "flash": int(flash), "ram": int(ram), "cycles": int(cycles),
            }
    return rows


def fmt_pct(cur, base):
    if base == 0:
        return "n/a"
    return f"{100.0 * (cur - base) / base:+.1f}%"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--baseline", default=DEFAULT_BASELINE)
    ap.add_argument("--configs", default="Os+lto",
                    help="optimization labels to check (default: shipped Os+lto)")
    ap.add_argument("--methods", default="YIN,FFT")
    ap.add_argument("--update", action="store_true",
                    help="rewrite the baseline from a fresh run instead of checking")
    ap.add_argument("--no-run", action="store_true",
                    help="reuse existing out/results.tsv instead of rebuilding")
    args = ap.parse_args()

    if not args.no_run:
        run_benchmark(args.configs, args.methods)
    cur = read_tsv(TSV)
    toolchain = avr_gcc_version()

    # ---- update mode: capture current numbers as the new baseline ----------
    if args.update:
        prev = {}
        if os.path.exists(args.baseline):
            with open(args.baseline) as f:
                prev = json.load(f)
        baseline = {
            "_comment": "Regression baseline for bench/check_regression.py. "
                        "Regenerate with: ./check_regression.py --update "
                        "(required after an avr-gcc version bump).",
            "toolchain": toolchain,
            "ceilings": prev.get("ceilings", DEFAULT_CEILINGS),
            "tolerances": prev.get("tolerances",
                                   {"flash": 0.02, "ram": 0.02, "cycles": 0.05}),
            "configs": cur,
        }
        with open(args.baseline, "w") as f:
            json.dump(baseline, f, indent=2)
            f.write("\n")
        print(f"wrote baseline {args.baseline} (toolchain {toolchain}, "
              f"{len(cur)} configs)")
        return 0

    # ---- check mode --------------------------------------------------------
    if not os.path.exists(args.baseline):
        raise SystemExit(f"no baseline at {args.baseline}; create one with --update")
    with open(args.baseline) as f:
        base = json.load(f)

    ceilings = base.get("ceilings", DEFAULT_CEILINGS)
    tol = base.get("tolerances", {"flash": 0.02, "ram": 0.02, "cycles": 0.05})
    base_cfg = base.get("configs", {})
    base_tc = base.get("toolchain", "unknown")

    relative_enabled = (toolchain == base_tc)
    lines = []
    failures = []
    warnings = []

    if not relative_enabled:
        warnings.append(
            f"avr-gcc is {toolchain} but the baseline was captured with {base_tc}; "
            f"relative regression checks are SKIPPED (numbers shift with the "
            f"toolchain). Re-baseline with `./check_regression.py --update` if this "
            f"toolchain is expected. Absolute ceilings are still enforced.")

    for key in sorted(cur):
        c = cur[key]
        b = base_cfg.get(key)
        budget = f"{100.0 * c['cycles'] / BUDGET_CYCLES:.0f}%"
        lines.append(f"### {key}")
        lines.append("")
        lines.append("| metric | current | baseline | delta | limit | status |")
        lines.append("|--------|--------:|---------:|------:|------:|:------:|")
        for m in METRICS:
            cv = c[m]
            bv = b[m] if b else None
            status = "ok"
            limit = "-"

            # absolute ceiling (flash/ram only)
            if m in ceilings:
                limit = str(ceilings[m])
                if cv > ceilings[m]:
                    status = "OVER LIMIT"
                    failures.append(f"{key}: {m} {cv} exceeds ceiling {ceilings[m]}")

            # relative regression
            delta = fmt_pct(cv, bv) if bv is not None else "new"
            if bv is not None and relative_enabled:
                allowed = bv * (1.0 + tol.get(m, 0.05)) + 8
                if cv > allowed and status == "ok":
                    status = "REGRESSED"
                    failures.append(
                        f"{key}: {m} {cv} exceeds baseline {bv} "
                        f"+{tol.get(m, 0.05) * 100:.0f}% ({delta})")
            elif b is None:
                status = "untracked"

            lines.append(f"| {m} | {cv} | {bv if bv is not None else '-'} "
                         f"| {delta} | {limit} | {status} |")
        lines.append(f"| _frame budget_ | {budget} of {BUDGET_CYCLES} cyc "
                     f"| | | | |")
        lines.append("")

    report = []
    report.append(f"## Benchmark regression gate (avr-gcc {toolchain})")
    report.append("")
    for w in warnings:
        report.append(f"> ⚠️ {w}")
        report.append("")
    report.extend(lines)
    if failures:
        report.append("**Result: FAIL**")
        report.append("")
        for fl in failures:
            report.append(f"- ❌ {fl}")
    else:
        report.append("**Result: PASS** — no flash/RAM/cycle regressions.")
    text = "\n".join(report)

    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as f:
            f.write(text + "\n")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
