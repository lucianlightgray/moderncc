









FROM debian:stable-slim














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
