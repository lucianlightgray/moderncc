# qemu-user cross-conformance runner (Docker)

Runs the `MCC_QEMU_TESTS` matrix (`CMakeLists.txt`) inside Linux, where
user-mode QEMU exists. **macOS cannot run this matrix natively** — Homebrew's
`qemu` only ships the system-mode emulators (`qemu-system-*`); the harness
needs the user-mode `qemu-x86_64` / `qemu-aarch64` / `qemu-arm` / `qemu-i386` /
`qemu-riscv64`, which build only on a Linux host. This image supplies them.

For each `(arch × libc)` the harness downloads a minimal Gentoo stage3 sysroot
(glibc + musl), cross-compiles `tests/qemu/conformance/*.c` with the matching
`<arch>-mcc`, and runs each self-checking program under `qemu-<arch> -L
<sysroot>` (default codegen and `-fPIC -pie`).

## Prerequisites on macOS

A Linux Docker daemon. With Colima:

```sh
brew install colima docker
colima start --cpu 4 --memory 6 --disk 60
```

## Build

```sh
docker build -t mcc-qemu tests/qemu/docker
```

## Run

From the repo root. Mount the repo at `/work` and your `vendor/` tree at
`/vendor`; the (large) Gentoo stage3 rootfs downloads are vendored under
`vendor/gentoo-stage3-<arch>-<libc>`, so `/vendor` doubles as their cross-run
cache — download once, reuse everywhere:

```sh
docker run --rm \
  -v "$PWD":/work \
  -v "$PWD/vendor:/vendor" -v "$PWD/dist:/dist" \
  mcc-qemu
```

The repo mount is staged to an internal `/src` copy, so no Linux build
artifacts are written back into your macOS tree. The separate `/vendor` mount is
the exception on purpose: fetched toolchains (clang/mingw/musl) are shared from
your host `vendor/` into the container — downloaded once, reused everywhere —
and any new fetch persists back to the host tree.

### Narrow the matrix

Configuration is driven by a CMake preset (`PRESET`, default `qemu` = the full
grid; the CI job passes the per-arch `qemu-<arch>`). `ARCHS` / `LIBCS` are
optional overrides applied on top of the preset; trailing args pass through to
`ctest`.

```sh
# one arch via its preset (what CI runs)
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v "$PWD/dist:/dist" \
  -e PRESET=qemu-x86_64 mcc-qemu

# one cell, fast smoke test (override the default preset's grid)
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v "$PWD/dist:/dist" \
  -e ARCHS=x86_64 -e LIBCS=glibc mcc-qemu

# everything, but only the musl rows
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v "$PWD/dist:/dist" \
  -e LIBCS=musl mcc-qemu

# pass ctest flags (e.g. a single test by name)
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v "$PWD/dist:/dist" \
  mcc-qemu -R qemu-arm64-glibc
```

| Env      | Default                          | Meaning                          |
|----------|----------------------------------|----------------------------------|
| `PRESET` | `qemu`                           | CMake preset (`qemu`, `qemu-<arch>`) |
| `ARCHS`  | *(preset)*                       | override architectures to exercise |
| `LIBCS`  | *(preset: `glibc;musl`)*         | override C libraries to exercise |

First run downloads ~250 MB per cell into `vendor/gentoo-stage3-*`; subsequent
runs (and your host) reuse them through the `/vendor` mount.

This image is the macOS-friendly wrapper: it stages the tree and execs
`ci qemu` (the `qemu` verb of `tools/ci.c`), which owns the
configure/build/fixup/test steps. Linux CI has user-mode qemu already, so it
skips Docker and runs `ci qemu` directly on the runner, one job per
`(arch × libc)` cell (see `.github/workflows/ci.yml`).

## Validating the AST optimizer on a Linux arch (not just conformance)

The matrix above cross-builds each `<arch>-mcc` **without** `MCC_CONFIG_OPTIMIZER`,
so it exercises codegen conformance but never the AST optimizer (replay, register
promotion, COLOR, VLAT, ...). To validate optimizer work on a Linux arch from the
macOS host, use `../native-optcheck.sh`: on Apple silicon + colima the container
is arm64, so it does a **native** optimizer-enabled build of mcc and runs the exec
suite under chosen `MCC_AST_*` gates — e.g.

```sh
MCC_GATES="MCC_AST_PROMOTE=1" tests/qemu/native-optcheck.sh   # arm64-Linux
PLATFORM=linux/amd64 tests/qemu/native-optcheck.sh            # x86_64-Linux
```
