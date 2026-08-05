#!/usr/bin/env bash
set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE="${IMAGE:-mcc-qemu}"
PLATFORM_ARG=()
[ -n "${PLATFORM:-}" ] && PLATFORM_ARG=(--platform "$PLATFORM")
GATES="${MCC_GATES:--fpromote-locals}"
CTR="${CTEST_R:-^exec/}"

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
if ! command -v cmake >/dev/null 2>&1 || ! command -v gcc >/dev/null 2>&1; then
    echo "=== installing build toolchain ==="
    apt-get update >/tmp/apt.log 2>&1 && \
    apt-get install -y --no-install-recommends cmake ninja-build gcc g++ make git ca-certificates >>/tmp/apt.log 2>&1 \
        || { echo PROVISION-FAIL; tail -20 /tmp/apt.log; exit 1; }
fi
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

failcount() { sed -nE 's/.*tests passed, ([0-9]+) tests failed.*/\1/p' | tail -1; }
echo "=== exec suite: default ==="
d_out=$(ctest --test-dir "$B" -R "$CTR" -j"$(nproc)" --output-on-failure 2>&1); echo "$d_out" | grep -iE '% tests passed'
d_fail=$(printf '%s\n' "$d_out" | failcount); d_fail=${d_fail:-0}
if [ "${d_fail:-0}" -gt 0 ]; then
    echo "=== DEFAULT FAILURES (names) ==="
    printf '%s\n' "$d_out" | grep -iE '\*\*\*Failed|\*\*\*Exception|Failed ' | grep -viE 'tests failed out|Skipped' | head -20
fi
echo "=== exec suite: with gates [$GATES] ==="
g_out=$(env $GATES ctest --test-dir "$B" -R "$CTR" -j"$(nproc)" 2>&1); echo "$g_out" | grep -iE '% tests passed'
g_fail=$(printf '%s\n' "$g_out" | failcount); g_fail=${g_fail:-0}

if [ "$g_fail" -gt "$d_fail" ]; then
    echo "GATE-REGRESSION: gated failures=$g_fail > default failures=$d_fail"
    echo "$g_out" | grep -iE 'failed|\*\*\*' | grep -viE 'Skipped|tests failed out' | head -20
    echo "=== DONE ==="; exit 1
fi
echo "OK: gated failures=$g_fail (not worse than default=$d_fail)"
echo "=== DONE ==="
INNER
