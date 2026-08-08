dg_docker() {
	MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' docker "$@"
}

dg_skip() {
	echo "SKIP: $*"
	exit 77
}

dg_need_bin() {
	{ [ -n "${1:-}" ] && [ -x "$1" ]; } || dg_skip "${2:-binary} not found at '${1:-<unset>}'"
}

dg_need_docker() {
	command -v docker >/dev/null 2>&1 || dg_skip "docker not available"
	docker info >/dev/null 2>&1 || dg_skip "docker daemon not available"
}

dg_host_plat() {
	case "$(uname -m)" in
	aarch64 | arm64) echo linux/arm64 ;;
	*) echo linux/amd64 ;;
	esac
}

dg_need_platform() {
	if [ -n "${1:-}" ]; then
		dg_docker run --rm --platform "$1" "$2" true >/dev/null 2>&1 \
			|| dg_skip "cannot run $1 containers ($2)"
	else
		dg_docker run --rm "$2" true >/dev/null 2>&1 \
			|| dg_skip "cannot run Linux containers ($2)"
	fi
}

dg_need_mount() {
	dg_probe_dir=$(cd "$1" && pwd) || dg_skip "work dir '$1' does not exist"
	: > "$dg_probe_dir/.dgmount" 2>/dev/null \
		|| dg_skip "work dir '$dg_probe_dir' is not writable"
	if ! dg_docker run --rm -v "$dg_probe_dir":/dgw alpine:3 \
			test -f /dgw/.dgmount >/dev/null 2>&1; then
		rm -f "$dg_probe_dir/.dgmount"
		dg_skip "docker cannot see '$dg_probe_dir' -- the bind mount is empty inside the container, so the host files this test writes would be invisible. On macOS add the path under Docker Desktop > Settings > Resources > File sharing, or use a build directory under a shared prefix such as \$HOME."
	fi
	rm -f "$dg_probe_dir/.dgmount"
}

dg_reset_work() {
	if [ -e "$1" ] && ! rm -rf "$1" 2>/dev/null; then
		dg_docker run --rm -v "$(cd "$(dirname "$1")" && pwd)":/p \
			alpine:3 sh -c "rm -rf /p/$(basename "$1")" >/dev/null 2>&1 || true
		rm -rf "$1" 2>/dev/null || true
	fi
	mkdir -p "$1"
}
