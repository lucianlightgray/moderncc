FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates gpg wget \
    && wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main" \
        > /etc/apt/sources.list.d/kitware.list \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        build-essential \
        gcc \
        clang \
        wine \
        wine64 \
        mingw-w64 \
        qemu-user \
        git \
        rsync \
    && if [ "$(dpkg --print-architecture)" = amd64 ]; then \
           dpkg --add-architecture i386 && apt-get update && \
           DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends wine32:i386; \
       fi \
    && rm -rf /var/lib/apt/lists/*

RUN set -e; for cc in gcc clang; do \
        command -v "$cc" >/dev/null || { echo "missing reference compiler $cc"; exit 1; }; \
    done; echo "reference compilers present (gcc + clang)"; \
    for t in wine x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do \
        command -v "$t" >/dev/null || { echo "missing PE toolchain $t"; exit 1; }; \
    done; echo "PE toolchain present (wine + mingw-w64)"; \
    for q in qemu-x86_64 qemu-i386 qemu-arm qemu-aarch64 qemu-riscv64; do \
        command -v "$q" >/dev/null || { echo "missing $q"; exit 1; }; \
    done; echo "qemu-user emulators present"; \
    if [ "$(dpkg --print-architecture)" = amd64 ]; then \
        wine --version; \
    else \
        echo "note: $(dpkg --print-architecture) host -- wine runs arm64/PE only;"; \
        echo "      build with --platform linux/amd64 for the x86 PE cells"; \
    fi; \
    cmake --version | head -1

RUN useradd --create-home ci \
    && mkdir -p /src /build \
    && chown ci:ci /src /build

ENV WINEDEBUG=-all \
    WINEPREFIX=/home/ci/.wine

COPY run-ci.sh /usr/local/bin/run-ci
RUN chmod +x /usr/local/bin/run-ci

USER ci
WORKDIR /work
ENTRYPOINT ["/usr/local/bin/run-ci"]
