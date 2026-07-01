# mcc Linux CI runner -- a container-equivalent of the GitHub Actions `linux`
# job (gcc/clang x native/cross; the portable + wine + macho test labels).
#
# It bundles the exact toolchain the `linux` job apt-installs (cmake, ninja,
# gcc, clang, wine64) so `docker build && docker run` reproduces a CI cell with
# no GitHub runner in the loop. macOS and Windows cells cannot be containerized
# on a Linux host, so they stay in Actions; this image plus the qemu-user image
# (tests/qemu/docker) together cover everything that CAN run under Linux Docker.
#
# Build:  docker build -t mcc-ci tests/ci/docker
# Run:    docker run --rm -e CC=gcc -e TARGET=cross -v "$PWD:/work:ro" mcc-ci
#         (CC=gcc|clang, TARGET=native|cross; see run-ci.sh for all knobs)
#
# Pinned to 24.04 to track ubuntu-latest (the `linux` job's runner image) so the
# apt package set and versions match what CI actually exercises.
FROM ubuntu:24.04

# Ubuntu 24.04's apt cmake is 3.28, but the macho-native test driver uses
# cmake_language(EXIT) (CMake >= 3.29) and GitHub's ubuntu-latest ships a newer
# cmake. Add Kitware's official APT repo so the image's cmake matches CI rather
# than lagging it.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates gpg wget \
    && wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main" \
        > /etc/apt/sources.list.d/kitware.list \
    && rm -rf /var/lib/apt/lists/*

# Mirror the `linux` job's toolchain: the two diff3/preprocess reference
# compilers (gcc + clang), the build tools, and wine64 for the PE-runtime (wine)
# label on the cross rows. git/ca-certificates/rsync stage a clean source copy
# off the read-only mount (as the qemu runner does).
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        build-essential \
        gcc \
        clang \
        wine64 \
        git \
        rsync \
    && rm -rf /var/lib/apt/lists/*

# Fail the build early if the image ever loses a diff3/preprocess reference
# compiler (gcc + clang), or if cmake is too old for cmake_language(EXIT).
RUN set -e; for cc in gcc clang; do \
        command -v "$cc" >/dev/null || { echo "missing reference compiler $cc"; exit 1; }; \
    done; echo "reference compilers present (gcc + clang)"; \
    cmake --version | head -1

# Run as a non-root user to match GitHub's `runner` account: several tests are
# permission-sensitive (root bypasses access checks and would mask real
# behaviour), and wine dislikes running as root. The read-only source mount at
# /work is readable by any uid; /src (staging) and /build (out-of-source) are
# owned by this user.
# (Ubuntu 24.04 already occupies uid 1000 with its stock `ubuntu` user, so let
# useradd pick the next free uid rather than pinning one.)
RUN useradd --create-home ci \
    && mkdir -p /src /build \
    && chown ci:ci /src /build

# Wine runs headless here (console PE conformance binaries -- no X server needed);
# keep its prefix in the user's home and silence its debug chatter.
ENV WINEDEBUG=-all \
    WINEPREFIX=/home/ci/.wine

COPY run-ci.sh /usr/local/bin/run-ci
RUN chmod +x /usr/local/bin/run-ci

USER ci
WORKDIR /work
ENTRYPOINT ["/usr/local/bin/run-ci"]
