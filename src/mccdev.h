/* mccdev.h -- in-development self-checking, one switch.
 *
 * MCC_DEV=1 compiles in the AST side-car's coherence oracle: wherever the
 * optimizer consults a fast index, the slow authoritative answer is recomputed
 * alongside it and the two are compared. On a mismatch the compiler prints what
 * diverged and abort()s. Three families today -- the def/use table, the
 * property memos (pure / cpropsafe / haslabel / hascase) and the structural
 * hash, plus the VLat value lattice, whose soundness check also widens
 * evaluation to every node so latent unsoundness becomes observable rather than
 * staying dormant until some pass happens to ask.
 *
 * SEPARATE FROM MCC_DIAG on purpose. MCC_DIAG is instrumentation of the
 * compiler as it runs -- allocator leak tracking, one-allocation-per-Sym, the
 * Replay_IR op trace -- and it changes allocation layout. This is a
 * differential check of whether a cache is telling the truth. The shadow-iv
 * sweep wants exactly this and none of that: folding them together made those
 * five cross cells measure a binary with a different Sym layout than the one
 * they exist to check.
 *
 *   cmake -DMCC_DEV=ON        (tools/shadow-iv-sweep.sh sets it directly)
 *
 * It abort()s rather than reports, so it is a CI instrument, not something to
 * ship. Nothing here has an -f spelling: a runtime flag cannot express "also
 * build the slow answer", which is a code-shape decision.
 */
#ifndef MCC_DEV_H
#define MCC_DEV_H

#ifndef MCC_DEV
#define MCC_DEV 0
#endif

/* Reading a knob that only exists for development. Off, these fold to a
   constant and the compiler drops the branch, so a shipped mcc cannot be told
   to inject a fault by anything in its environment -- which is the point:
   MCC_JIT_SPEC_WRONG deliberately mis-specialises, MCC_JIT_POISON_PCT
   deliberately corrupts, and neither should be one setenv away in production. */
#if MCC_DEV
#define MCC_DEV_ENV_ON(name) mcc_env_on(name)
#define MCC_DEV_ENV(name) getenv(name)
#else
#define MCC_DEV_ENV_ON(name) 0
#define MCC_DEV_ENV(name) ((const char *)0)
#endif

#endif /* MCC_DEV_H */
