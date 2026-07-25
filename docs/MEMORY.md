# Memory index

Persistent project notes, version-controlled under `docs/`. One file per note.

- [roi-scheduler-determinism](roi-scheduler-determinism.md) — MCC_AST_ROI clock()→epoch-delta fix; ROI+emitsize fixed; phase-2 (b)/(d) advance() root-caused to recorder gap
- [opassign-recorder-fix](opassign-recorder-fix.md) — MCC_AST_OPASSIGN gate models compound-assign-through-pointer so advance()/`p->field op=` become replayable (default off)
- [math-inline-prepass](math-inline-prepass.md) — MCC_AST_MATH_INLINE_PREPASS gate fixes -O4 dropping sqrt/fabs inline + traces local nonneg so advance()'s sqrt(d2) inlines (default off)
- [promo-arrow-and-hotloop-gaps](promo-arrow-and-hotloop-gaps.md) — MCC_AST_PROMO_ARROW promotes pointers used via `->`; remaining advance() gaps = addressing mode, reg pool, LSR (default off)
- [crossarch-gate-validation](crossarch-gate-validation.md) — recipe + results: opassign/promo-arrow validated correct on x86_64/arm64/riscv64/i386 via cross-mcc + qemu; arm32 blocked on EABI mismatch
- [search-resume-continue](search-resume-continue.md) — the -O>=4 per-function gate search is resumable/continuable and on by default (MCC_AST_SEARCH)
- [slice-cache-refactor](slice-cache-refactor.md) — ongoing refactor: optimize over normalized AST slices with a JIT↔AOT content-addressed cache
- [embed-jit-arm64osx-gap](embed-jit-arm64osx-gap.md) — embedded-JIT baked binaries + in-process -run JIT don't work on arm64-osx in current builds
- [docker-optimizer-mcc-validation](docker-optimizer-mcc-validation.md) — fast recipe to build+test an optimizer-enabled mcc in Docker on this win32 host
- [moderncc-build-test-loop](moderncc-build-test-loop.md) — how to build+test moderncc incrementally (mcc target, asttool selftests, ctest)
- [push-to-main-needs-authorization](push-to-main-needs-authorization.md) — TODO says push to main, but the harness gates it; commit locally + surface a note
