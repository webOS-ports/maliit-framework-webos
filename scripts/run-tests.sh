#!/bin/sh
#
# Configure, build and run the unit test suite.
#
# By default the tests are built with AddressSanitizer and
# UndefinedBehaviorSanitizer, because most of what they cover is memory
# lifetime and integer/shift behaviour: a test that passes without the
# sanitizers proves much less than the same test passing with them. Pass
# --no-sanitizers for a plain build.
#
# Usage:
#   scripts/run-tests.sh [--no-sanitizers] [--build-dir DIR] [--keep]
#                        [--qmake PATH] [-- <extra qmake args>]
#
# Environment:
#   QMAKE           qmake binary to use (default: qmake6, then qmake)
#   QT_QPA_PLATFORM forced to "offscreen" unless already set
#
# Cross builds: this script runs the binaries it builds, so it is for a native
# build. To build the suite for a device, add CONFIG-=notests to the qmake
# invocation the recipe makes and run "make check" on the target.

set -eu

top_dir=$(cd "$(dirname "$0")/.." && pwd)
build_dir="$top_dir/build-tests"
sanitizers=yes
keep=no
qmake_bin="${QMAKE:-}"
qmake_extra=""

while [ $# -gt 0 ]; do
    case "$1" in
        --no-sanitizers) sanitizers=no ;;
        --keep)          keep=yes ;;
        --build-dir)     shift; build_dir="${1:-}" ;;
        --qmake)         shift; qmake_bin="${1:-}" ;;
        --)              shift; qmake_extra="$*"; break ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 2
            ;;
    esac
    shift
done

if [ -z "$qmake_bin" ]; then
    for candidate in qmake6 qmake; do
        if command -v "$candidate" >/dev/null 2>&1; then
            qmake_bin="$candidate"
            break
        fi
    done
fi

if [ -z "$qmake_bin" ]; then
    echo "No qmake found. Set QMAKE or pass --qmake." >&2
    exit 2
fi

sanitizer_args=""
if [ "$sanitizers" = yes ]; then
    sanitizer_args="CONFIG+=sanitizer CONFIG+=sanitize_address CONFIG+=sanitize_undefined"

    # Without this an overflow that UBSan merely prints keeps the exit status
    # at zero and the run looks clean.
    UBSAN_OPTIONS="${UBSAN_OPTIONS:-}:print_stacktrace=1:halt_on_error=1"
    UBSAN_OPTIONS="${UBSAN_OPTIONS#:}"
    export UBSAN_OPTIONS

    # Qt allocates a fair amount it never frees at exit by design; leak
    # reporting here is noise, while the use-after-free and overflow detection
    # is exactly what we want.
    ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
    ASAN_OPTIONS="${ASAN_OPTIONS#:}"
    export ASAN_OPTIONS
fi

if [ "$keep" = no ]; then
    rm -rf "$build_dir"
fi
mkdir -p "$build_dir"

: "${QT_QPA_PLATFORM:=offscreen}"
export QT_QPA_PLATFORM

echo "== configuring in $build_dir =="
cd "$build_dir"
# shellcheck disable=SC2086
"$qmake_bin" "$top_dir/maliit-framework.pro" \
    CONFIG+=wayland \
    $sanitizer_args \
    $qmake_extra

echo "== building =="
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo "== running tests =="
# "make check" recurses into tests/ and runs each binary in place.
make -C tests check
