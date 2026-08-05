/* mccdiag.h -- the compiler's compile-time debug instrumentation: one
 * switch, on or off.
 *
 * MCC_DIAG instruments the compiler as it RUNS. The differential
 * self-checking that asks whether a cache is telling the truth is MCC_DEV in
 * mccdev.h -- kept apart because MCC_DIAG changes allocation layout, and the
 * shadow-iv sweep needs the oracle without that.
 *
 * MCC_DIAG=1 turns on the internal instruments: allocator and tiny-allocator
 * leak tracking with file/line on every chunk, one-allocation-per-Sym instead
 * of slab slots, and the Replay_IR operation trace. Off, none of them compile.
 *
 * MCC_TRACE is deliberately NOT one of them. It keeps its own switch,
 * MCC_CONFIG_TRACE, because it is in a different weight class: ~12,600 call
 * sites, one in essentially every basic block, so wanting a leak report should
 * not drag in a tracer that changes how the whole compiler runs.
 *
 * These are the facilities that CANNOT be runtime flags: the allocator and
 * symbol-table instruments change struct layout, so they are ABI decisions
 * rather than options. Everything that CAN be a runtime flag lives in
 * mccopt.h and is spelled -f<name>; nothing here has an -f spelling.
 *
 *   cmake -DMCC_DIAG=ON
 *
 * The build side of the same switch (what was MCC_ALL_DIAGNOSTICS) also turns
 * on the verbose warning/debug-info flag set and builds the mcc_s and mcc_c
 * targets, so one switch means "scrutinise everything" rather than two that
 * had to be remembered together.
 *
 * One switch replaces five macros with three spelling conventions between them
 * -- #ifdef for some, #if for others, and MCC_MEM_DEBUG/MCC_TAL_DEBUG carrying
 * an undocumented 1/2/3 level you had to reverse-engineer from comparisons like
 * `MCC_TAL_DEBUG != 3`. Those levels are gone with the mask: a leak is tracked
 * and reported, full stop. The two variants that could not survive a single
 * switch were exit(2)-on-leak and track-but-stay-silent, which contradict each
 * other and, with no mask, could not be selected apart anyway.
 *
 * MCC_PROFILE is not here because it no longer exists. It was a linkage hack
 * (#define static) for the mcc_p -pg target, which no test ever exercised; both
 * are deleted. It could not have joined this switch anyway -- applying it to an
 * ordinary build produces a compiler that cannot resolve its own include paths,
 * measured rather than assumed.
 */
#ifndef MCC_DIAG_H
#define MCC_DIAG_H

#ifndef MCC_DIAG
#define MCC_DIAG 0
#endif

#endif /* MCC_DIAG_H */
