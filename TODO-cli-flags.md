# TODO — CLI flags: remaining deferred work

All of the planned gcc/clang-style CLI flags have shipped (`-std=`, `-b`/`-bt`,
`-O` levels, `-pie`/`-no-pie`, `--sysroot=`/`-isysroot`, `-fshort-enums`,
`-fwrapv`, `-fvisibility=`, `-fstack-protector*`, the accepted `-m*` set, …).
See git history for those changes. Two items remain **intentionally deferred** —
the CLI surface is in place, but the deep work below was not attempted.

## T1.5 — `-fPIC` / `-fPIE` / `-fpic` / `-fpie` / `-fno-pic` (codegen)

The flags are accepted today, and x86_64 is position-independent already
(`-fPIC -pie` runs as a PIE). The remaining work is converting compile-time PIC
codegen into a runtime decision on **i386/arm**.

Deferred because it's untestable in this environment (no i386 multilib/libc, so
i386 PIC binaries can't be run/verified) and the refactor would risk regressing
the i386 backend with no test coverage.

PIC codegen is hardwired by `#if defined CONFIG_MCC_PIC` across the backends
(`i386-gen.c` ~14 sites, `arm-gen.c:497`, `i386-link.c:17`).

- [ ] Add `MCCState.pic` (0/1/2 = none/pic/PIC) + `-f[no-]pic` / `-f[no-]PIE`
      handling (affects codegen, so not plain `options_f[]` rows).
- [ ] i386 backend: convert each `#if defined CONFIG_MCC_PIC` to test `s->pic`
      at emit time (GOT/PLT sequences in `i386-gen.c`).
- [ ] arm backend: same for `arm-gen.c:497`.
- [ ] Linker: `i386-link.c:17` relocation handling under runtime `pic`.
- [ ] Keep `CONFIG_MCC_PIC` as the compiled-in *default* for `s->pic`.
- [ ] Tests: diff relocs for a GOT-referencing TU `-fPIC` vs `-fno-pic`; run a
      `-fPIC -pie` exe under qemu (ties into the cross harness).
- [ ] Note: x86_64 is largely RIP-relative already, so this is mostly i386/arm.

## N.2 — `-s` (strip symbols)

Currently reported under `-Wunsupported` rather than honored. Real stripping was
deferred: naively truncating `.symtab`/`.strtab` corrupted the image.

- [ ] Implement proper section removal (drop `.symtab`/`.strtab` and fix up the
      section table / offsets) instead of truncation.
