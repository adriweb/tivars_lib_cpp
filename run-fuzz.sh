#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build-fuzz"
TARGET="$BUILD_DIR/tivars_lib_cpp_fuzzer"
FUZZ_DIR="$ROOT_DIR/fuzz"
CORPUS_DIR="$FUZZ_DIR/corpus"
LOG_DIR="$FUZZ_DIR/logs"
ARTIFACT_DIR="$FUZZ_DIR/artifacts"
SEED_CORPUS_DIR="$ROOT_DIR/testData"

TIME_LIMIT=60
RSS_LIMIT_MB=4096
TIMEOUT_SEC=5
REBUILD=0
FRESH_CORPUS=0

usage() {
    cat <<'EOF'
Usage: ./run-fuzz.sh [options]

Options:
  --time SEC         Total fuzzing time in seconds. Default: 60
  --workers N        Number of parallel libFuzzer workers. Default: half of CPU cores, minimum 1
  --jobs N           Number of total libFuzzer jobs. Default: same as workers
  --rss-limit MB     Per-process RSS limit in MB. Default: 4096
  --timeout SEC      Per-input timeout in seconds. Default: 5
  --corpus DIR       Corpus directory to use. Default: fuzz/corpus
  --seed DIR         Seed corpus for --fresh runs. Default: testData
  --fresh            Use a temporary writable corpus and keep the main corpus unchanged
  --rebuild          Reconfigure and rebuild the fuzz target before running
  -h, --help         Show this help

Examples:
  ./run-fuzz.sh
  ./run-fuzz.sh --time 300 --workers 8 --jobs 8
  ./run-fuzz.sh --fresh --time 60
EOF
}

to_abs_path() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$ROOT_DIR" "$1" ;;
    esac
}

dir_has_files() {
    local dir="$1"
    [[ -d "$dir" ]] && find "$dir" -mindepth 1 -print -quit | grep -q .
}

unique_path() {
    local path="$1"
    local candidate="$path"
    local suffix=1

    while [[ -e "$candidate" ]]; do
        candidate="${path}.${suffix}"
        suffix=$(( suffix + 1 ))
    done

    printf '%s\n' "$candidate"
}

detect_cores() {
    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null && return 0
    fi
    if command -v nproc >/dev/null 2>&1; then
        nproc && return 0
    fi
    echo 1
}

ensure_built() {
    if [[ ! -x "$TARGET" || "$REBUILD" -eq 1 ]]; then
        cmake -GNinja -S "$ROOT_DIR" -B "$BUILD_DIR" -DTIVARS_ENABLE_FUZZING=ON
        cmake --build "$BUILD_DIR" --target tivars_lib_cpp_fuzzer -j4
    fi
}

WORKERS=""
JOBS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --time)
            TIME_LIMIT="$2"
            shift 2
            ;;
        --workers)
            WORKERS="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --rss-limit)
            RSS_LIMIT_MB="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT_SEC="$2"
            shift 2
            ;;
        --corpus)
            CORPUS_DIR="$2"
            shift 2
            ;;
        --seed)
            SEED_CORPUS_DIR="$2"
            shift 2
            ;;
        --fresh)
            FRESH_CORPUS=1
            shift
            ;;
        --rebuild)
            REBUILD=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

CORPUS_DIR="$(to_abs_path "$CORPUS_DIR")"
SEED_CORPUS_DIR="$(to_abs_path "$SEED_CORPUS_DIR")"

CPU_CORES="$(detect_cores)"
if [[ -z "$WORKERS" ]]; then
    WORKERS=$(( CPU_CORES / 2 ))
    if [[ "$WORKERS" -lt 1 ]]; then
        WORKERS=1
    fi
fi
if [[ -z "$JOBS" ]]; then
    JOBS="$WORKERS"
fi

mkdir -p "$CORPUS_DIR" "$LOG_DIR" "$ARTIFACT_DIR"

ensure_built

CMD=(
    "$TARGET"
    -workers="$WORKERS"
    -jobs="$JOBS"
    -max_total_time="$TIME_LIMIT"
    -rss_limit_mb="$RSS_LIMIT_MB"
    -timeout="$TIMEOUT_SEC"
    -artifact_prefix="$ARTIFACT_DIR/"
)

if [[ "$FRESH_CORPUS" -eq 1 ]]; then
    RUN_CORPUS="$(mktemp -d /tmp/tivars-fuzz.XXXXXX)"
    echo "Using temporary writable corpus: $RUN_CORPUS"
    if [[ -d "$SEED_CORPUS_DIR" ]]; then
        cp -R "$SEED_CORPUS_DIR"/. "$RUN_CORPUS"/
    fi
    CMD+=("$RUN_CORPUS" "$CORPUS_DIR")
else
    CMD+=("$CORPUS_DIR")
fi

echo "Running with $WORKERS worker(s), $JOBS job(s), time limit ${TIME_LIMIT}s"
echo "Corpus: $CORPUS_DIR"
echo "Logs: $LOG_DIR"
echo "Artifacts: $ARTIFACT_DIR"
printf 'Command:'
printf ' %q' "${CMD[@]}"
printf '\n'

cd "$LOG_DIR"
exec "${CMD[@]}"
