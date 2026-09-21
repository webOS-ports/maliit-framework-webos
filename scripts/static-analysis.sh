#!/bin/sh
#
# Run the static analysers over the framework sources.
#
# Two passes, because they disagree about what is worth saying and the overlap
# is small:
#
#   cppcheck     - flow-sensitive, finds leaks, uninitialised reads and
#                  out-of-bounds accesses without needing to compile
#   clang-tidy   - compiles for real, so it sees through templates and Qt
#                  macros, and runs the clang static analyser's path-sensitive
#                  checks (see .clang-tidy for the selection)
#
# Both are optional: the script reports which ones it found and skips the rest,
# so it is useful on a machine that only has one of them installed.
#
# Usage:
#   scripts/static-analysis.sh [--cppcheck-only] [--tidy-only]
#                              [--compile-commands DIR] [-- <extra tidy args>]
#
# Exit status is 0 only when every analyser that ran reported nothing.

set -eu

top_dir=$(cd "$(dirname "$0")/.." && pwd)
cd "$top_dir"

sources="common connection passthroughserver src tests"
run_cppcheck=yes
run_tidy=yes
compile_commands=""
tidy_extra=""

while [ $# -gt 0 ]; do
    case "$1" in
        --cppcheck-only) run_tidy=no ;;
        --tidy-only)     run_cppcheck=no ;;
        --compile-commands)
            shift
            compile_commands="${1:-}"
            ;;
        --) shift; tidy_extra="$*"; break ;;
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

status=0
ran_any=no

if [ "$run_cppcheck" = yes ] && command -v cppcheck >/dev/null 2>&1; then
    ran_any=yes
    echo "== cppcheck =="

    # Everything cppcheck knows how to say, including the style category: the
    # tree is expected to come out clean under it, so a finding here is a
    # finding to fix rather than a category to switch off.
    #
    # The four suppressions below are about what cppcheck cannot see, not
    # about what we would rather not hear:
    #
    #   missingInclude/missingIncludeSystem
    #       it is not given the Qt, glib or luna-service2 include paths, by
    #       design - resolving them would tie this script to a sysroot
    #   unknownMacro
    #       Q_OBJECT, Q_DECLARE_PRIVATE, Q_DISABLE_COPY and friends are moc
    #       constructs its preprocessor does not model
    #   unusedFunction
    #       most of this tree is a library; "unused" here means "no caller in
    #       this tree", which is true of every symbol a plugin links against
    #
    # Anything narrower is an inline cppcheck-suppress comment at the site,
    # with the reason next to it.
    cppcheck \
        --enable=all \
        --inconclusive \
        --std=c++17 \
        --language=c++ \
        --force \
        --quiet \
        --inline-suppr \
        --error-exitcode=1 \
        --suppress=missingInclude \
        --suppress=missingIncludeSystem \
        --suppress=unknownMacro \
        --suppress=unusedFunction \
        --suppress=unmatchedSuppression \
        --suppress=checkersReport \
        -I src -I common -I connection -I . \
        $sources || status=1
    echo
fi

if [ "$run_tidy" = yes ] && command -v clang-tidy >/dev/null 2>&1; then
    if [ -z "$compile_commands" ] && [ -f compile_commands.json ]; then
        compile_commands="$top_dir"
    fi

    if [ -z "$compile_commands" ]; then
        cat >&2 <<'EOF'
clang-tidy needs a compilation database and none was found.

Generate one with bear (or intercept-build) against a normal build, then
point this script at the directory holding compile_commands.json:

    mkdir build && cd build
    qmake6 ../maliit-framework.pro CONFIG+=wayland CONFIG+=notests
    bear -- make -j"$(nproc)"
    cd .. && scripts/static-analysis.sh --tidy-only --compile-commands build

For a cross build the database has to name the target sysroot, since that is
where the Qt, glib and luna-service2 headers live; the include paths in the
generated Makefiles under the OE build directory already do.

Skipping clang-tidy.
EOF
    else
        ran_any=yes
        echo "== clang-tidy =="
        files=$(find $sources -name '*.cpp' -not -name 'moc_*' | sort)
        # shellcheck disable=SC2086
        clang-tidy -p "$compile_commands" --quiet $tidy_extra $files || status=1
        echo
    fi
fi

if [ "$ran_any" = no ]; then
    echo "No analyser available. Install cppcheck and/or clang-tidy." >&2
    exit 2
fi

if [ "$status" -eq 0 ]; then
    echo "static analysis: clean"
else
    echo "static analysis: findings above" >&2
fi

exit "$status"
