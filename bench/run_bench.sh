#!/usr/bin/env bash
# This is free and unencumbered software released into the public domain.
# For more information, please refer to <http://unlicense.org/>
#
# run_bench.sh -- compiler-optimization benchmark for Tuna.
#
# Two measurements per (optimization config x pitch method):
#   1. Code size:  the *real* firmware cross-compiled for the avr64dd14 target,
#                  reported as flash (text+data) and RAM (data+bss) by avr-size.
#   2. CPU cycles: the bench_dsp.c harness (one pitch pass over a fixed frame)
#                  run under simavr and counted between its GPIOR0 markers.
#
# simavr has no AVR-Dx core, so cycles are measured on an atmega1284p (classic
# AVRe+ core, plenty of SRAM). Treat the cycle numbers as a consistent proxy for
# comparing optimization configs, not as absolute avr64dd14 timings. Code size,
# by contrast, is measured on the true target and is exact. See README.md.
#
# Requires: avr-gcc, avr-size, an extracted AVR-Dx DFP, libsimavr (for simrun).
#
# Usage:
#   run_bench.sh [--configs L1,L2,...] [--methods YIN,FFT]
#
#   --configs   restrict the optimization labels swept (default: all). The
#               regression gate (check_regression.py) passes "--configs Os+lto"
#               to time just the shipped configuration in CI.
#   --methods   restrict the pitch methods (default: YIN,FFT).
#
# Always writes a machine-readable TSV to out/results.tsv:
#   method<TAB>config<TAB>flash<TAB>ram<TAB>cycles
# and, when the full default sweep runs, the human report to results.md.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src"
OUT="$HERE/out"
RESULTS="$HERE/results.md"
TSV="$OUT/results.tsv"
MCU_SIZE="avr64dd14"
MCU_SIM="atmega1284p"
F_CPU_TARGET="24000000UL"   # real target clock (for the size build's F_CPU)
F_CPU_SIM="16000000UL"      # arbitrary; the sim counts cycles, not seconds

# ---- argument parsing ------------------------------------------------------
WANT_CONFIGS=""   # comma-separated labels, empty = all
WANT_METHODS="YIN,FFT"
while [ $# -gt 0 ]; do
    case "$1" in
        --configs) WANT_CONFIGS="$2"; shift 2 ;;
        --methods) WANT_METHODS="$2"; shift 2 ;;
        -h|--help) sed -n '5,18p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

# csv membership test: contains <csv> <item>
contains() { case ",$1," in *",$2,"*) return 0 ;; *) return 1 ;; esac; }

# ---- locate the AVR-Dx device family pack (for the avr64dd14 size build) ----
DFP="${AVR_DFP:-}"
if [ -z "$DFP" ]; then
    for cand in "$HERE/../build/_deps/avr_dfp-src" "$HERE/../build"/_deps/*/; do
        [ -f "$cand/gcc/dev/$MCU_SIZE/device-specs/specs-$MCU_SIZE" ] && DFP="$cand" && break
        [ -d "$cand/gcc/dev/$MCU_SIZE" ] && DFP="$cand" && break
    done
fi
if [ -z "$DFP" ] || [ ! -d "$DFP/gcc/dev/$MCU_SIZE" ]; then
    echo "error: AVR-Dx DFP not found. Configure the CMake build once (which"
    echo "       auto-downloads it) or pass AVR_DFP=/path/to/extracted/pack." >&2
    exit 1
fi
echo "Using DFP: $DFP"

mkdir -p "$OUT"

# ---- build the simavr runner if needed -------------------------------------
if [ ! -x "$HERE/simrun" ] || [ "$HERE/simrun.c" -nt "$HERE/simrun" ]; then
    echo "Building simrun ..."
    gcc -O2 -Wall "$HERE/simrun.c" -o "$HERE/simrun" -lsimavr || exit 1
fi

# ---- regenerate the input frame if the generator changed -------------------
if [ ! -f "$HERE/bench_frame.h" ] || [ "$HERE/gen_frame.py" -nt "$HERE/bench_frame.h" ]; then
    ( cd "$HERE" && python3 gen_frame.py )
fi

# ---- optimization configurations to sweep ----------------------------------
# label            -> compiler flags
CONFIG_LABELS=( "O0" "O1" "O2" "O3" "Os" "Os+lto" "O2+lto" "O3+lto" )
CONFIG_FLAGS=( "-O0" "-O1" "-O2" "-O3" "-Os" "-Os -flto" "-O2 -flto" "-O3 -flto" )
SHIPPED="Os+lto"   # what CMakeLists.txt currently uses

COMMON_WARN="-funsigned-char -funsigned-bitfields"

# ---- size build: real firmware for avr64dd14 -------------------------------
# Echoes "flash ram" (bytes) or "ERR".
build_size() {
    local flags="$1" method="$2" elf="$OUT/fw_$3.elf"
    avr-gcc -mmcu="$MCU_SIZE" -B "$DFP/gcc/dev/$MCU_SIZE" \
        $flags -ffunction-sections -fdata-sections $COMMON_WARN \
        -I"$SRC" -isystem "$DFP/include" -DF_CPU=$F_CPU_TARGET -D"$method" \
        "$SRC"/*.c -o "$elf" \
        -Wl,--gc-sections -Wl,--relax -lm 2>"$OUT/size_$3.log"
    if [ $? -ne 0 ]; then echo "ERR ERR"; return; fi
    avr-size "$elf" | awk 'NR==2 {printf "%d %d\n", $1+$2, $2+$3}'
}

# ---- cycle build: bench harness for the simavr target ----------------------
# Echoes a cycle count or "ERR".
build_cycles() {
    local flags="$1" method="$2" tag="$3" elf="$OUT/bench_$3.elf"
    local extra=""
    [ "$method" = "PITCH_METHOD_FFT" ] && extra="-DBENCH_FFT"
    local dsp
    if [ "$method" = "PITCH_METHOD_FFT" ]; then
        dsp="$SRC/spectral.c $SRC/fft.c $SRC/window.c"
    else
        dsp="$SRC/yin.c"
    fi
    avr-gcc -mmcu="$MCU_SIM" $flags $COMMON_WARN $extra \
        -I"$SRC" -I"$HERE" -DF_CPU=$F_CPU_SIM -D"$method" \
        "$HERE/bench_dsp.c" $dsp -o "$elf" -lm 2>"$OUT/cyc_$3.log"
    if [ $? -ne 0 ]; then echo "ERR"; return; fi
    # simavr prints "Loaded ..." banners to stdout; keep only the cycle line.
    "$HERE/simrun" "$elf" "$MCU_SIM" 2>/dev/null \
        | awk '/^[0-9]+$/{n=$0} END{print (n==""?"ERR":n)}'
}

# 24 MHz, 250 ms frame budget = 6,000,000 cycles (see config.h).
BUDGET=6000000

# Append a TSV row and emit one markdown table row to stdout.
run_method() {
    local method="$1" name="$2"
    echo
    echo "### $name pitch method"
    echo
    printf '| config | flash (B) | RAM (B) | DSP cycles | %% of frame budget |\n'
    printf '|--------|----------:|--------:|-----------:|------------------:|\n'
    local i
    for i in "${!CONFIG_LABELS[@]}"; do
        local label="${CONFIG_LABELS[$i]}" flags="${CONFIG_FLAGS[$i]}"
        [ -n "$WANT_CONFIGS" ] && ! contains "$WANT_CONFIGS" "$label" && continue
        local tag="${name}_${label//+/_}"
        read flash ram <<<"$(build_size "$flags" "$method" "$tag")"
        local cyc; cyc="$(build_cycles "$flags" "$method" "$tag")"
        printf '%s\t%s\t%s\t%s\t%s\n' "$name" "$label" "$flash" "$ram" "$cyc" >>"$TSV"
        local pct="-"
        if [ "$cyc" != "ERR" ]; then
            pct="$(awk -v c="$cyc" -v b="$BUDGET" 'BEGIN{printf "%.1f%%", 100*c/b}')"
        fi
        local mark=""
        [ "$label" = "$SHIPPED" ] && mark=" **(shipped)**"
        printf '| %s%s | %s | %s | %s | %s |\n' \
            "$label" "$mark" "$flash" "$ram" "$cyc" "$pct"
    done
}

: >"$TSV"   # fresh TSV each run

# A filtered run (CI gate) must not clobber the committed full-sweep report.
FULL=0
[ -z "$WANT_CONFIGS" ] && [ "$WANT_METHODS" = "YIN,FFT" ] && FULL=1

report() {
    echo "# Tuna compiler-optimization benchmark"
    echo
    echo "_Generated by \`bench/run_bench.sh\`._"
    echo
    echo "- **Flash / RAM**: real firmware built for \`$MCU_SIZE\` ($(avr-gcc -dumpversion 2>/dev/null) avr-gcc), measured with \`avr-size\`. Exact for the target."
    echo "  Flash = text+data, RAM = data+bss."
    echo "- **DSP cycles**: one pitch pass over a fixed 220 Hz frame, counted under simavr on"
    echo "  \`$MCU_SIM\` (classic AVRe+ core -- simavr has no AVR-Dx core). Relative proxy, not absolute Dx timing."
    echo "- **Frame budget**: $BUDGET cycles (250 ms @ 24 MHz). A method must finish a frame within this to keep up with acquisition."
    echo "- Shipped config (\`CMakeLists.txt\`): \`$SHIPPED\` (\`-Os -flto\`)."
    contains "$WANT_METHODS" "YIN" && run_method "PITCH_METHOD_YIN" "YIN"
    contains "$WANT_METHODS" "FFT" && run_method "PITCH_METHOD_FFT" "FFT"
    echo
    echo "_Cycle counts are deterministic; flash/RAM depend on the avr-gcc version._"
}

if [ "$FULL" = 1 ]; then
    report | tee "$RESULTS"
    echo
    echo "Wrote $RESULTS and $TSV"
else
    report
    echo
    echo "Wrote $TSV (filtered run; results.md left untouched)"
fi
