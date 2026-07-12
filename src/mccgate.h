#ifndef MCC_GATE_H
#define MCC_GATE_H

/*
 * The optimizer strategy/knob bitset and the M3 vocabulary bridge.
 *
 * `AstGateMask` is sized to the widest host integer (`uint64_t`, up to 64 knobs)
 * rather than a fixed 8/32: the search starts using only the low bits (the four fold
 * gates) and scales up to the max width as opt-in knobs are added, so a new knob is
 * never silently truncated. The low bits are the in-process fold gates + their opt-in
 * knobs; higher bits are reserved for the out-of-process superopt's axes so one
 * unified search can eventually enumerate the union of both vocabularies (roadmap M3).
 *
 * Header-only and dependency-free (only <stdint.h>) so both the compiler
 * (src/mccast.c) and the unit harness (tools/asttool.c) share one definition.
 */

#include <stdint.h>

/* Each including TU gets its own copy; unused copies are harmless (no -Werror). */
#ifndef MCC_GATE_INLINE
#define MCC_GATE_INLINE
#endif

typedef uint64_t AstGateMask;

/* In-process fold gates (the four toggled by the live -O4 search). */
#define AST_SG_TEMPLATES ((AstGateMask)1)
#define AST_SG_NARROW ((AstGateMask)2)
#define AST_SG_BITFLAG ((AstGateMask)4)
#define AST_SG_SETHI ((AstGateMask)8)
/* Opt-in enablement knob (roadmap "widen the search space"): off in every -O
 * baseline, so the subset lattice cannot reach it by dropping bits — the search
 * ADDS it. Modifies the narrow strategy (iterate-to-fixpoint) rather than being its
 * own pass, and only bites when AST_SG_NARROW is also set. */
#define AST_SG_NARROWFIX ((AstGateMask)16)
/* Opt-in enablement knob gated on AST_SG_SETHI (reliably in base in both superopt
 * modes, so it is actually exercised): leaf-aware Sethi-Ullman register-need. Off in
 * every baseline; the search ADDS it. Correct-by-construction — sethi only reorders
 * commutative operands, so any choice is semantics-preserving. */
#define AST_SG_SETHILEAF ((AstGateMask)32)

/*
 * M3 (subsume the out-of-process superopt) — vocabulary bridge, blocker B.
 *
 * The two optimization searches drive OVERLAPPING-BUT-DIFFERENT config axes on two
 * substrates: the in-process AstGateMask search toggles {templates, narrow, bitflag,
 * sethi, narrowfix, sethileaf}; the out-of-process superopt (`so_setenv_cfg`,
 * `SoPfCkpt.best_cfg` in mcc.c) toggles {templates, promote, inline, no_callful} as
 * gate bits plus {cprop_join, cse_join} and integer node/graft/bitflag levels. To
 * eventually run ONE search over the union, both encodings must map losslessly onto
 * one bitset — these functions are that bridge. The superopt-only axes occupy bits
 * 6.. so they never collide with the fold-gate/knob bits above.
 *
 * This is the selftested vocabulary bridge only (exercised by tools/asttool.c),
 * NOT yet wired into the live search — the same way mcccombo.h landed before its
 * call-sites. The superopt gate-bit meanings mirror so_setenv_cfg (mcc.c): bit0=
 * templates, bit1=promote, bit2=inline, bit3=no_callful; perfn best_cfg uses bit0=
 * tmpl, bit1=promo, bit2=inl (the drivers use the values 1/3/7).
 */
#define AST_SG_PROMOTE ((AstGateMask)64)
#define AST_SG_INLINE ((AstGateMask)128)
#define AST_SG_NOCALLFUL ((AstGateMask)256)
#define AST_SG_CPROPJOIN ((AstGateMask)512)
#define AST_SG_CSEJOIN ((AstGateMask)1024)

/* More in-process opt-in knobs (continue above the superopt axes). V-licm(d): the
 * existing env-gated CSE-embedded helpers — LICM temp materialization, induction-var
 * strength reduction, partial redundancy elimination — promoted to search-selectable
 * bits. All default off, run inside ast_cse_run (so only bite when templates/cse are
 * enabled), and are already differential-fuzz-validated (fuzz GATES[]). */
#define AST_SG_LTEMP ((AstGateMask)2048)
#define AST_SG_IVSR ((AstGateMask)4096)
#define AST_SG_PRE ((AstGateMask)8192)
/* Two more opt-in dataflow knobs, both templates-gated (dse/tco run under templates)
 * and validated to the full M8 bar (exec+fuzz+self-host+cross-arch): DSE see-through
 * of bare calls, and TCO of pointer params. */
#define AST_SG_DSECALL ((AstGateMask)16384)
#define AST_SG_TCOPTR ((AstGateMask)32768)
#define AST_SG_CSECOMM ((AstGateMask)65536)
/* Standalone opt-in pass (no base-gate dependency): the V-bf(a) range-predicate fold
 * (`lo<=x && x<=hi` -> unsigned-subtract compare). Off in every -O baseline; the search
 * ADDS it via `searchable`, like narrowfix/sethileaf. */
#define AST_SG_RANGE ((AstGateMask)131072)
/* Standalone opt-in pass: unsigned constant division/remainder strength reduction
 * (`x/C`, `x%C` -> high-multiply + shift). Off in every -O baseline; search ADDS it. */
#define AST_SG_DIVMAGIC ((AstGateMask)262144)
/* Standalone opt-in pass: branchless abs from `x<0?-x:x`. Search ADDS it via `searchable`. */
#define AST_SG_ABS ((AstGateMask)524288)
#define AST_SG_REASSOC ((AstGateMask)1048576)
/* Modifier of the templates-gated sccp pass: fuse cprop+sccp to a fixpoint
 * (`MCC_AST_SCCP_FIX`). Off in every -O baseline; the search ADDS it when
 * templates is in base, like ltemp/ivsr/pre. */
#define AST_SG_SCCPFIX ((AstGateMask)2097152)

enum {
	SO_GATE_TEMPLATES = 1u,
	SO_GATE_PROMOTE = 2u,
	SO_GATE_INLINE = 4u,
	SO_GATE_NOCALLFUL = 8u
};

/* superopt-search gate bits (so_setenv_cfg) <-> unified AstGateMask. Bit-valued axes
 * only; the integer node/graft/bitflag levels carried in `budget` have no on/off gate
 * bit and are not part of this bit bridge. Lossless and invertible for the 4 bits. */
static MCC_GATE_INLINE AstGateMask ast_gate_from_so(unsigned so_gate) {
	return ((so_gate & SO_GATE_TEMPLATES) ? AST_SG_TEMPLATES : 0) |
				 ((so_gate & SO_GATE_PROMOTE) ? AST_SG_PROMOTE : 0) |
				 ((so_gate & SO_GATE_INLINE) ? AST_SG_INLINE : 0) |
				 ((so_gate & SO_GATE_NOCALLFUL) ? AST_SG_NOCALLFUL : 0);
}

static MCC_GATE_INLINE unsigned ast_gate_to_so(AstGateMask g) {
	return ((g & AST_SG_TEMPLATES) ? SO_GATE_TEMPLATES : 0) |
				 ((g & AST_SG_PROMOTE) ? SO_GATE_PROMOTE : 0) |
				 ((g & AST_SG_INLINE) ? SO_GATE_INLINE : 0) |
				 ((g & AST_SG_NOCALLFUL) ? SO_GATE_NOCALLFUL : 0);
}

/* superopt-perfn best_cfg (bit0=tmpl, bit1=promo, bit2=inl; values 1/3/7) <-> unified. */
static MCC_GATE_INLINE AstGateMask ast_gate_from_perfn(unsigned best_cfg) {
	return ((best_cfg & 1u) ? AST_SG_TEMPLATES : 0) |
				 ((best_cfg & 2u) ? AST_SG_PROMOTE : 0) |
				 ((best_cfg & 4u) ? AST_SG_INLINE : 0);
}

static MCC_GATE_INLINE unsigned ast_gate_to_perfn(AstGateMask g) {
	return ((g & AST_SG_TEMPLATES) ? 1u : 0) | ((g & AST_SG_PROMOTE) ? 2u : 0) |
				 ((g & AST_SG_INLINE) ? 4u : 0);
}

#endif /* MCC_GATE_H */
