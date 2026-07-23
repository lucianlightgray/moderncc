# Shared scaffolding for the tools/*-docker.sh differential harnesses. Sourced,
# not executed: it only defines functions. Every harness needs the same skip-77
# gating (missing binary / no docker / unrunnable platform) and the same Git
# Bash path-mangling guard on each `docker` call, so that lives here once.
#
# Usage (from a harness):
#   . "$(dirname "$0")/dockergate.sh"
#   dg_need_bin "$MCC" "riscv64 mcc"
#   dg_need_docker
#   dg_need_platform linux/amd64 "$IMAGE"
#   dg_docker run --rm --platform linux/amd64 -v "$WP":/w -w /w "$IMAGE" sh -c '...'

# Run docker with the guards that stop Git Bash / MSYS from rewriting Unix-style
# arguments (image refs, `--platform linux/386`, in-container paths) into Windows
# paths. Use in place of a bare `docker` for every invocation.
dg_docker() {
	MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' docker "$@"
}

# Print a SKIP line and exit with ctest's SKIP_RETURN_CODE (77).
dg_skip() {
	echo "SKIP: $*"
	exit 77
}

# Gate: the binary under test must exist and be executable. Its mere presence is
# what wires the harness into ctest; the compiler actually used may be rebuilt
# in-container. $2 is a human label for the SKIP message.
dg_need_bin() {
	{ [ -n "${1:-}" ] && [ -x "$1" ]; } || dg_skip "${2:-binary} not found at '${1:-<unset>}'"
}

# Gate: a usable docker daemon (both the client and a reachable engine).
dg_need_docker() {
	command -v docker >/dev/null 2>&1 || dg_skip "docker not available"
	docker info >/dev/null 2>&1 || dg_skip "docker daemon not available"
}

# Gate: the host can actually run containers of the given platform (binfmt/qemu
# present for a foreign arch). An empty platform means "the host's own arch"
# (no --platform). $2 is the probe image.
dg_need_platform() {
	if [ -n "${1:-}" ]; then
		dg_docker run --rm --platform "$1" "$2" true >/dev/null 2>&1 \
			|| dg_skip "cannot run $1 containers ($2)"
	else
		dg_docker run --rm "$2" true >/dev/null 2>&1 \
			|| dg_skip "cannot run Linux containers ($2)"
	fi
}
