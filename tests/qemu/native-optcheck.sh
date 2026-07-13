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

docker run --rm -i ${PLATFORM_ARG[@]+"${PLATFORM_ARG[@]}"} --entrypoint bash \
    -v "$REPO":/work:ro -e GATES="$GATES" -e CTR="$CTR" "$IMAGE" -s <<'INNER'
set -uo pipefail
B=/tmp/b; rm -rf "$B"; mkdir -p "$B"
echo "=== container arch: $(uname -m) ==="
cmake -G Ninja -S /work -B "$B" -DCMAKE_BUILD_TYPE=Debug \
    -DMCC_CONFIG_OPTIMIZER=ON -DMCC_BUILD_TESTS=ON >/tmp/cfg.log 2>&1 \
    || { echo CONFIG-FAIL; tail -30 /tmp/cfg.log; exit 1; }
cmake --build "$B" --target mcc -j"$(nproc)" >/tmp/bld.log 2>&1 \
    || { echo BUILD-FAIL; tail -40 /tmp/bld.log; exit 1; }
"$B/mcc" -v 2>&1 | head -1

echo "=== exec suite: default ==="
ctest --test-dir "$B" -R "$CTR" -j"$(nproc)" 2>&1 | grep -iE '% tests passed'
echo "=== exec suite: with gates [$GATES] ==="
env $GATES ctest --test-dir "$B" -R "$CTR" -j"$(nproc)" 2>&1 | grep -iE '% tests passed'
echo "=== DONE ==="
INNER
