# mcc qemu-user cross-conformance runner.
#
# Provides the one thing macOS can't: Linux user-mode QEMU (qemu-x86_64,
# qemu-aarch64, qemu-arm, qemu-i386, qemu-riscv64) plus a host toolchain to
# build mcc and all <arch>-mcc cross compilers. The per-arch glibc/musl
# sysroots are fetched by the CMake harness at run time into /qemu-roots
# (mount a named volume there to cache them across runs).
#
# Build:  docker build -t mcc-qemu tests/qemu/docker
# Run:    see tests/qemu/docker/run-matrix.sh
FROM debian:stable-slim

# - build-essential/cmake/make/git : build mcc + the cross compilers; its gcc is
#                                    also the Linux diff3/preprocess gcc reference
# - qemu-user                      : BARE-named emulators (qemu-x86_64 ...) the
#                                    CMakeLists harness invokes by name
# - ca-certificates                : TLS for the Gentoo stage3 downloads
# - tar/xz-utils/bzip2             : stage3 tarballs are .tar.xz / .tar.bz2
# - rsync                          : stage a clean source copy off the mount
# - clang/lld                      : the clang half of the diff3/preprocess
#                                    reference pair (gcc + clang), and the
#                                    aarch64 cross-link for the darwin/macho row
#                                    (qemu-arm64-osx): link mcc's arm64-osx -c
#                                    object against the arm64 glibc sysroot and
#                                    run it under qemu-aarch64
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        make \
        git \
        ca-certificates \
        qemu-user \
        tar \
        xz-utils \
        bzip2 \
        rsync \
        clang \
        lld \
    && rm -rf /var/lib/apt/lists/*

# Fail the build early if the distro ever stops shipping a needed emulator
# under its bare name (the harness looks these up with find_program), or either
# diff3/preprocess reference compiler (gcc + clang, discovered on PATH).
RUN set -e; for q in qemu-x86_64 qemu-i386 qemu-arm qemu-aarch64 qemu-riscv64; do \
        command -v "$q" >/dev/null || { echo "missing $q"; exit 1; }; \
    done; echo "qemu-user emulators present"; \
    for cc in gcc clang; do \
        command -v "$cc" >/dev/null || { echo "missing diff3 reference $cc"; exit 1; }; \
    done; echo "diff3 reference compilers present (gcc + clang)"

VOLUME /qemu-roots
WORKDIR /work
COPY run-matrix.sh /usr/local/bin/run-matrix
RUN chmod +x /usr/local/bin/run-matrix
ENTRYPOINT ["/usr/local/bin/run-matrix"]
