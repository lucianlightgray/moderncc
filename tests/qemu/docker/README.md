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

From the repo root. Mount the repo at `/work`, your `vendor/` tree at `/vendor`,
and a named volume at `/qemu-roots` so the (large) Gentoo downloads are cached
across runs:

```sh
docker run --rm \
  -v "$PWD":/work \
  -v "$PWD/vendor:/vendor" \
  -v mcc-qemu-roots:/qemu-roots \
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
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v mcc-qemu-roots:/qemu-roots \
  -e PRESET=qemu-x86_64 mcc-qemu

# one cell, fast smoke test (override the default preset's grid)
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v mcc-qemu-roots:/qemu-roots \
  -e ARCHS=x86_64 -e LIBCS=glibc mcc-qemu

# everything, but only the musl rows
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v mcc-qemu-roots:/qemu-roots \
  -e LIBCS=musl mcc-qemu

# pass ctest flags (e.g. a single test by name)
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" -v mcc-qemu-roots:/qemu-roots \
  mcc-qemu -R qemu-arm64-glibc
```

| Env      | Default                          | Meaning                          |
|----------|----------------------------------|----------------------------------|
| `PRESET` | `qemu`                           | CMake preset (`qemu`, `qemu-<arch>`) |
| `ARCHS`  | *(preset)*                       | override architectures to exercise |
| `LIBCS`  | *(preset: `glibc;musl`)*         | override C libraries to exercise |
| `JOBS`   | `$(nproc)`                       | build parallelism                |

First run downloads ~250 MB per cell; subsequent runs reuse `/qemu-roots`.
This is the containerized form of the CI job in `TODO.md` ("10.6.2 CI workflow").
