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

From the repo root. Mount the repo at `/work` and a named volume at
`/qemu-roots` so the (large) Gentoo downloads are cached across runs:

```sh
docker run --rm \
  -v "$PWD":/work \
  -v mcc-qemu-roots:/qemu-roots \
  mcc-qemu
```

The repo mount is staged to an internal `/src` copy, so no Linux build
artifacts are written back into your macOS tree.

### Narrow the matrix

`ARCHS` / `LIBCS` select the grid; trailing args pass through to `ctest`.

```sh
# one cell, fast smoke test
docker run --rm -v "$PWD":/work -v mcc-qemu-roots:/qemu-roots \
  -e ARCHS=x86_64 -e LIBCS=glibc mcc-qemu

# everything, but only the musl rows
docker run --rm -v "$PWD":/work -v mcc-qemu-roots:/qemu-roots \
  -e LIBCS=musl mcc-qemu

# pass ctest flags (e.g. a single test by name)
docker run --rm -v "$PWD":/work -v mcc-qemu-roots:/qemu-roots \
  mcc-qemu -R qemu-arm64-glibc
```

| Env     | Default                          | Meaning                          |
|---------|----------------------------------|----------------------------------|
| `ARCHS` | `x86_64;i386;arm;arm64;riscv64`  | architectures to exercise        |
| `LIBCS` | `glibc;musl`                     | C libraries to exercise          |
| `JOBS`  | `$(nproc)`                       | build parallelism                |

First run downloads ~250 MB per cell; subsequent runs reuse `/qemu-roots`.
This is the containerized form of the CI job in `TODO.md` ("10.6.2 CI workflow").
