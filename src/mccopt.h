#ifndef MCC_OPT_H
#define MCC_OPT_H

#define MCC_OPTD_OFF 0
#define MCC_OPTD_ALWAYS 1
#define MCC_OPTD_SPECIAL 2
#define MCC_OPTD_LEVEL(n) (0x100 | (n))
#define MCC_OPTD_IS_LEVEL(d) (((d) & 0x100) != 0)
#define MCC_OPTD_LEVEL_OF(d) ((d) & 0xff)
#define MCC_OPTD_DEV(d) (0x200 | (d))
#define MCC_OPTD_IS_DEV(d) (((d) & 0x200) != 0)
#define MCC_OPTD_BASE(d) ((d) & ~0x200)

#define MCC_OPT_SEARCH_LEVEL 13
#define MCC_OPT_SEARCH_TICKS 1
#define MCC_OPT_SEARCH_TICK_LIMITS 5u
#define MCC_OPT_SEARCH_TICK_BUDGETS 2u
#define MCC_OPT_SEARCH_TICK_GATES 2u
#define MCC_OPT_SEARCH_TU_EVALS 20000u

#define MCC_OPT_LIST(MCC_OPT_ROW) \
	MCC_OPT_ROW(REPLAY_FALLBACK,               "replay-fallback",              MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REPLAY_MATERIALIZE,            "replay-materialize",           MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REPLAY_LANDOR_INVERT,          "replay-landor-invert",         MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(DUMP_REPLAY,                   "dump-replay",                  MCC_OPTD_OFF) \
	MCC_OPT_ROW(REEMIT_TEMPLATES,              "reemit-templates",             MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(TREE_CONST_LOAD,               "tree-const-load",              MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(OPT_SEARCH,                    "opt-search",                   MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(DUMP_OPT_SEARCH,               "dump-opt-search",              MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_SLICE,                     "opt-slice",                    MCC_OPTD_DEV(MCC_OPTD_LEVEL(9))) \
	MCC_OPT_ROW(OPT_SEARCH_EMIT_SIZE,          "opt-search-emit-size",         MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_SEARCH_EMIT_ISO,           "opt-search-emit-iso",          MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_SEARCH_INLINE,             "opt-search-inline",            MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_SEARCH_THREADS,            "opt-search-threads",           MCC_OPTD_DEV(MCC_OPTD_OFF)) \
	MCC_OPT_ROW(OPT_SEARCH_PTHREADS,           "opt-search-pthreads",          MCC_OPTD_DEV(MCC_OPTD_OFF)) \
	MCC_OPT_ROW(OPT_SEARCH_ORDERED,            "opt-search-ordered",           MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_SEARCH_ORDER,              "opt-search-order",             MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_SEARCH_FULLSET,            "opt-search-fullset",           MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(OPT_SEARCH_PREDICT,            "opt-search-predict",           MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_ROI,                       "opt-roi",                      MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(DUMP_OPT_ROI,                  "dump-opt-roi",                 MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_CYCLE,                     "opt-cycle",                    MCC_OPTD_DEV(MCC_OPTD_LEVEL(11))) \
	MCC_OPT_ROW(PROMOTE_LOCALS,                "promote-locals",               MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(PROMOTE_ARROW,                 "promote-arrow",                MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(PROMOTE_INCDEC,                "promote-incdec",               MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(CHAIN_STORE,                   "chain-store",                  MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(STOREVAL_CONSTL,               "storeval-constl",              MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(STOREVAL_CALLSTORE,            "storeval-callstore",           MCC_OPTD_LEVEL(2)) \
	MCC_OPT_ROW(STOREVAL_ROT,                  "storeval-rot",                 MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(STOREVAL_CALLLAST,             "storeval-calllast",            MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(CHAIN_STORE_LIVE,              "chain-store-live",             MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(CHAIN_STORE_MEMBER,            "chain-store-member",           MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(STOREVAL_CALL,                 "storeval-call",                MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(STOREVAL_CALLUP,               "storeval-callup",              MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REPLAY_CMP_MATERIALIZE,        "replay-cmp-materialize",       MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REPLAY_WHILE_COMMA,            "replay-while-comma",           MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REPLAY_LOOPCOND_STORE,         "replay-loopcond-store",        MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REPLAY_INDIRECT_CALL,          "replay-indirect-call",         MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(PROMOTE_LEAF_XMM,              "promote-leaf-xmm",             MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(DUMP_COST_OPS,                 "dump-cost-ops",                MCC_OPTD_OFF) \
	MCC_OPT_ROW(DUMP_COST_SPILL,               "dump-cost-spill",              MCC_OPTD_OFF) \
	MCC_OPT_ROW(RELOC_EQUIV,                   "reloc-equiv",                  MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(FMOV_IMM,                      "fmov-imm",                     MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(REG_DISP,                      "reg-disp",                     MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(XMM_HI,                        "xmm-hi",                       MCC_OPTD_DEV(MCC_OPTD_LEVEL(5))) \
	MCC_OPT_ROW(PROMOTE_LEAF_CALLEE,           "promote-leaf-callee",          MCC_OPTD_DEV(MCC_OPTD_LEVEL(10))) \
	MCC_OPT_ROW(PROMOTE_ACROSS_CALLS,          "promote-across-calls",         MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(INLINE,                        "inline",                       MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(DUMP_COST,                     "dump-cost",                    MCC_OPTD_OFF) \
	MCC_OPT_ROW(SETHI_ULLMAN,                  "sethi-ullman",                 MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(SETHI_ULLMAN_LEAF,             "sethi-ullman-leaf",            MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(SETHI_ULLMAN_NARY,             "sethi-ullman-nary",            MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TREE_SWITCH_CONVERSION,        "tree-switch-conversion",       MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(DUMP_BITFLAG,                  "dump-bitflag",                 MCC_OPTD_OFF) \
	MCC_OPT_ROW(TREE_COPY_PROP,                "tree-copy-prop",               MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(NARROW,                        "narrow",                       MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TRUNC32,                       "trunc32",                      MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(SWITCH_EXPR,                   "switch-expr",                  MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(NARROW_FIX,                    "narrow-fix",                   MCC_OPTD_DEV(MCC_OPTD_LEVEL(11))) \
	MCC_OPT_ROW(NARROW_CLASS0,                 "narrow-class0",                MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(NARROW_CLASS1,                 "narrow-class1",                MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(NARROW_CLASS2,                 "narrow-class2",                MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(NARROW_CLASS3,                 "narrow-class3",                MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(NARROW_ELIM,                   "narrow-elim",                  MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TREE_CCP_ITERATE,              "tree-ccp-iterate",             MCC_OPTD_LEVEL(3)) \
	MCC_OPT_ROW(IDENT_CONV,                    "ident-conv",                   MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(IDENT_SHIFT,                   "ident-shift",                  MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(IDENT_ARITH,                   "ident-arith",                  MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(IDENT_BIT,                     "ident-bit",                    MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(IDENT_REL,                     "ident-rel",                    MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(IDENT_URANGE,                  "ident-urange",                 MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(TREE_DSE,                      "tree-dse",                     MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(OPTIMIZE_SIBLING_CALLS,        "optimize-sibling-calls",       MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(GCSE,                          "gcse",                         MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TREE_VRP,                      "tree-vrp",                     MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(DIVMAGIC,                      "divmagic",                     MCC_OPTD_LEVEL(2)) \
	MCC_OPT_ROW(IF_CONVERSION_ABS,             "if-conversion-abs",            MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(IF_CONVERSION,                 "if-conversion",                MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TREE_SRA,                      "tree-sra",                     MCC_OPTD_OFF) \
	MCC_OPT_ROW(TREE_SROA,                     "tree-sroa",                    MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TREE_SROA_PARAMS,              "tree-sroa-params",             MCC_OPTD_OFF) \
	MCC_OPT_ROW(TREE_REASSOC,                  "tree-reassoc",                 MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(REASSOC_ASSOC,                 "reassoc-assoc",                MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REASSOC_SHLSHR,                "reassoc-shlshr",               MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REASSOC_SHRSHL,                "reassoc-shrshl",               MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(REASSOC_MULDIST,               "reassoc-muldist",              MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(BFOLD_SQRT,                    "bfold-sqrt",                   MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(BFOLD_SIGN,                    "bfold-sign",                   MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(BFOLD_ROUND,                   "bfold-round",                  MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(BFOLD_MINMAX,                  "bfold-minmax",                 MCC_OPTD_ALWAYS) \
	MCC_OPT_ROW(BUILTIN_MATH,                  "builtin-math",                 MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(BUILTIN_MATH_FABS,             "builtin-math-fabs",            MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(BUILTIN_MATH_PREPASS,          "builtin-math-prepass",         MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(BUILTIN_ROUND,                 "builtin-round",                MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(BUILTIN_COPYSIGN,              "builtin-copysign",             MCC_OPTD_LEVEL(1)) \
	MCC_OPT_ROW(BUILTIN_MINMAX,                "builtin-minmax",               MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(BUILTIN_FMA,                   "builtin-fma",                  MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(BUILTIN_MATH_ERRNO,            "builtin-math-errno",           MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(INLINE_FUNCTIONS,              "inline-functions",             MCC_OPTD_LEVEL(2)) \
	MCC_OPT_ROW(LOOP_INTERCHANGE,              "loop-interchange",             MCC_OPTD_DEV(MCC_OPTD_LEVEL(12))) \
	MCC_OPT_ROW(LOOP_FUSION,                   "loop-fusion",                  MCC_OPTD_DEV(MCC_OPTD_LEVEL(12))) \
	MCC_OPT_ROW(LOOP_BLOCK,                    "loop-block",                   MCC_OPTD_DEV(MCC_OPTD_LEVEL(12))) \
	MCC_OPT_ROW(LOOP_VLAT,                     "loop-vlat",                    MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(JIT_SPLICE,                    "jit-splice",                   MCC_OPTD_OFF) \
	MCC_OPT_ROW(ZERO_INITIALIZED_IN_BSS,       "zero-initialized-in-bss",      MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(MERGE_CONSTANTS,               "merge-constants",              MCC_OPTD_LEVEL(2)) \
	MCC_OPT_ROW(GCSE_JOIN,                     "gcse-join",                    MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(CALL_WINDOW,                   "call-window",                  MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(TREE_LOOP_IM,                  "tree-loop-im",                 MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(IVOPTS,                        "ivopts",                       MCC_OPTD_SPECIAL) \
	MCC_OPT_ROW(IVOPTS_PTR,                    "ivopts-ptr",                   MCC_OPTD_LEVEL(3)) \
	MCC_OPT_ROW(TREE_PRE,                      "tree-pre",                     MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(DUMP_LOOPNEST,                 "dump-loopnest",                MCC_OPTD_OFF) \
	MCC_OPT_ROW(DUMP_LOOPDEP,                  "dump-loopdep",                 MCC_OPTD_OFF) \
	MCC_OPT_ROW(DEP_ALIAS_ORACLE,              "dep-alias-oracle",             MCC_OPTD_OFF) \
	MCC_OPT_ROW(OPT_PERFN_INPROC,              "opt-perfn-inproc",             MCC_OPTD_DEV(MCC_OPTD_LEVEL(8))) \
	MCC_OPT_ROW(ARG_FORWARD,                   "arg-forward",                  MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(REG_COLOR,                     "reg-color",                    MCC_OPTD_LEVEL(2)) \
	MCC_OPT_ROW(SPILL_SHARE,                   "spill-share",                  MCC_OPTD_LEVEL(4)) \
	MCC_OPT_ROW(LOOP_UNROLL,                   "unroll-loops",                 MCC_OPTD_OFF)

enum {
#define MCC_OPT_ROW(id, name, dflt) MCC_OPT_##id,
	MCC_OPT_LIST(MCC_OPT_ROW)
#undef MCC_OPT_ROW
	MCC_OPT_COUNT
};

#endif
