#!/usr/bin/env bash
# Native-in-container optimizer validation for a Linux arch, driven from any host
# (built for the macOS dev host, where user-mode qemu is unavailable — see
# tests/qemu/docker/README.md).
#
# The tests/qemu/docker CONFORMANCE matrix cross-builds the per-arch `<arch>-mcc`
# WITHOUT MCC_CONFIG_OPTIMIZER, so it never exercises the AST optimizer (replay,
# register promotion, COLOR, VLAT, ...). This harness fills that gap: it runs
# INSIDE a Linux container of the container's own architecture, does a NATIVE
# optimizer-enabled build of mcc, and runs the exec suite under a chosen set of
# MCC_AST_* gates. On an Apple-silicon host with colima, the default container is
# arm64 — so this is how arm64-Linux optimizer work (e.g. Tier-3 register
# promotion with real 128-bit `long double`) is validated from macOS.
#
# Usage (from the repo root, with a running Docker/colima daemon):
#   tests/qemu/native-optcheck.sh                       # arm64 (colima default), default gates
#   MCC_GATES="MCC_AST_PROMOTE=1" tests/qemu/native-optcheck.sh
#   PLATFORM=linux/amd64 tests/qemu/native-optcheck.sh  # x86_64 via colima
#   IMAGE=debian:stable-slim tests/qemu/native-optcheck.sh
#
# Env:
#   IMAGE     container image with cc/cmake/ninja/objdump (default: mcc-qemu, the
#             tests/qemu/docker image; falls back to installing on debian if bare)
#   PLATFORM  docker --platform (default: unset = the daemon's native arch)
#   MCC_GATES space-separated MCC_AST_* assignments forced on for the promote pass
#   CTEST_R   ctest -R filter (default: '^exec/')
set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE="${IMAGE:-mcc-qemu}"
PLATFORM_ARG=()
[ -n "${PLATFORM:-}" ] && PLATFORM_ARG=(--platform "$PLATFORM")
GATES="${MCC_GATES:-MCC_AST_PROMOTE=1}"
CTR="${CTEST_R:-^exec/}"

# The mcc-qemu image ships build tools but is single-arch (built for the dev
# host). When it is unavailable for the requested platform (e.g. running
# PLATFORM=linux/amd64 from an arm64 host), fall back to a plain debian image;
# INNER self-provisions the toolchain there. Any explicit IMAGE= is honoured.
if [ "$IMAGE" = "mcc-qemu" ] && ! docker image inspect ${PLATFORM_ARG[@]+"${PLATFORM_ARG[@]}"} "$IMAGE" >/dev/null 2>&1; then
    echo "note: '$IMAGE' unavailable for ${PLATFORM:-native}; falling back to debian:bookworm-slim"
    IMAGE="debian:bookworm-slim"
fi

MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm -i ${PLATFORM_ARG[@]+"${PLATFORM_ARG[@]}"} --entrypoint bash \
    -v "$REPO":/work:ro -e GATES="$GATES" -e CTR="$CTR" "$IMAGE" -s <<'INNER'
set -uo pipefail
B=/tmp/b; rm -rf "$B"; mkdir -p "$B"
echo "=== container arch: $(uname -m) ==="
# Self-provision on a bare image (mcc-qemu already has these).
if ! command -v cmake >/dev/null 2>&1 || ! command -v gcc >/dev/null 2>&1; then
    echo "=== installing build toolchain ==="
    apt-get update >/tmp/apt.log 2>&1 && \
    apt-get install -y --no-install-recommends cmake ninja-build gcc g++ make git ca-certificates >>/tmp/apt.log 2>&1 \
        || { echo PROVISION-FAIL; tail -20 /tmp/apt.log; exit 1; }
fi
# Build from a container-LOCAL copy of the source, never the RO bind mount.
# On Docker Desktop for Windows the Windows->Linux 9p mount makes cmake's
# thousands of small configure-time reads glacial (configure stuck in D-state
# for >11 min). Copying only what the build needs into a container-local /src
# turns those reads into fast tmpfs/overlay reads. We deliberately skip .git
# (huge/slow, and the MCC_GITHASH step already ERROR_QUIETs / EXISTS-guards on
# its absence) and the host build-*/cmake-* trees. On Linux/macOS the bind
# mount is fast so this copy is cheap and the RO mount keeps the host tree
# pristine either way.
SRC=/src; rm -rf "$SRC"; mkdir -p "$SRC"
echo "=== staging source copy into $SRC (skipping .git / build dirs) ==="
for item in CMakeLists.txt CMakePresets.json config-extra.cmake \
            .clang-format .gitattributes \
            cmake src include runtime tools tests examples; do
    [ -e "/work/$item" ] && cp -a "/work/$item" "$SRC/" || true
done
cmake -G Ninja -S "$SRC" -B "$B" -DCMAKE_BUILD_TYPE=Debug \
    -DMCC_CONFIG_OPTIMIZER=ON -DMCC_BUILD_TESTS=ON >/tmp/cfg.log 2>&1 \
    || { echo CONFIG-FAIL; tail -30 /tmp/cfg.log; exit 1; }
cmake --build "$B" --target mcc -j"$(nproc)" >/tmp/bld.log 2>&1 \
    || { echo BUILD-FAIL; tail -40 /tmp/bld.log; exit 1; }
"$B/mcc" -v 2>&1 | head -1

# "N tests passed, M tests failed out of T" -> echo the failed count.
failcount() { sed -nE 's/.*tests passed, ([0-9]+) tests failed.*/\1/p' | tail -1; }
echo "=== exec suite: default ==="
d_out=$(ctest --test-dir "$B" -R "$CTR" -j"$(nproc)" 2>&1); echo "$d_out" | grep -iE '% tests passed'
d_fail=$(printf '%s\n' "$d_out" | failcount); d_fail=${d_fail:-0}
echo "=== exec suite: with gates [$GATES] ==="
g_out=$(env $GATES ctest --test-dir "$B" -R "$CTR" -j"$(nproc)" 2>&1); echo "$g_out" | grep -iE '% tests passed'
g_fail=$(printf '%s\n' "$g_out" | failcount); g_fail=${g_fail:-0}

# A gate that regresses correctness fails more exec tests than the default build.
if [ "$g_fail" -gt "$d_fail" ]; then
    echo "GATE-REGRESSION: gated failures=$g_fail > default failures=$d_fail"
    echo "$g_out" | grep -iE 'failed|\*\*\*' | grep -viE 'Skipped|tests failed out' | head -20
    echo "=== DONE ==="; exit 1
fi
echo "OK: gated failures=$g_fail (not worse than default=$d_fail)"
echo "=== DONE ==="
INNER
