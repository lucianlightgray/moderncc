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
