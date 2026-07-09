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
        wine64 \
        mingw-w64 \
        qemu-user \
        git \
        rsync \
    && rm -rf /var/lib/apt/lists/*

RUN set -e; for cc in gcc clang; do \
        command -v "$cc" >/dev/null || { echo "missing reference compiler $cc"; exit 1; }; \
    done; echo "reference compilers present (gcc + clang)"; \
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
