#ifndef MCC_GATE_H
#define MCC_GATE_H

#include <stdint.h>

#ifndef MCC_GATE_INLINE
#define MCC_GATE_INLINE
#endif

typedef uint64_t AstGateMask;

#define AST_SG_TEMPLATES ((AstGateMask)1)
#define AST_SG_NARROW ((AstGateMask)2)
#define AST_SG_BITFLAG ((AstGateMask)4)
#define AST_SG_SETHI ((AstGateMask)8)
#define AST_SG_NARROWFIX ((AstGateMask)16)
#define AST_SG_SETHILEAF ((AstGateMask)32)

#define AST_SG_PROMOTE ((AstGateMask)64)
#define AST_SG_INLINE ((AstGateMask)128)
#define AST_SG_NOCALLFUL ((AstGateMask)256)
#define AST_SG_CPROPJOIN ((AstGateMask)512)
#define AST_SG_CSEJOIN ((AstGateMask)1024)

#define AST_SG_LTEMP ((AstGateMask)2048)
#define AST_SG_IVSR ((AstGateMask)4096)
#define AST_SG_PRE ((AstGateMask)8192)
#define AST_SG_DSECALL ((AstGateMask)16384)
#define AST_SG_TCOPTR ((AstGateMask)32768)
#define AST_SG_CSECOMM ((AstGateMask)65536)
#define AST_SG_RANGE ((AstGateMask)131072)
#define AST_SG_DIVMAGIC ((AstGateMask)262144)
#define AST_SG_ABS ((AstGateMask)524288)
#define AST_SG_REASSOC ((AstGateMask)1048576)
#define AST_SG_SCCPFIX ((AstGateMask)2097152)
#define AST_SG_IDENT_CONV ((AstGateMask)4194304)
#define AST_SG_IDENT_SHIFT ((AstGateMask)8388608)
#define AST_SG_IDENT_ARITH ((AstGateMask)16777216)
#define AST_SG_IDENT_BIT ((AstGateMask)33554432)
#define AST_SG_IDENT_REL ((AstGateMask)67108864)
#define AST_SG_IDENT_URANGE ((AstGateMask)134217728)
#define AST_SG_REASSOC_ASSOC ((AstGateMask)268435456)
#define AST_SG_REASSOC_SHLSHR ((AstGateMask)536870912)
#define AST_SG_REASSOC_SHRSHL ((AstGateMask)1073741824)
#define AST_SG_REASSOC_MULDIST ((AstGateMask)2147483648)
#define AST_SG_BFOLD_SQRT ((AstGateMask)4294967296)
#define AST_SG_BFOLD_SIGN ((AstGateMask)8589934592)
#define AST_SG_BFOLD_ROUND ((AstGateMask)17179869184)
#define AST_SG_BFOLD_MINMAX ((AstGateMask)34359738368)
#define AST_SG_NARROW_C0 ((AstGateMask)68719476736)
#define AST_SG_NARROW_C1 ((AstGateMask)137438953472)
#define AST_SG_NARROW_C2 ((AstGateMask)274877906944)
#define AST_SG_NARROW_C3 ((AstGateMask)549755813888)
#define AST_SG_JIT_DISPATCH ((AstGateMask)1 << 40)
#define AST_SG_JIT_GUARD ((AstGateMask)1 << 41)
#define AST_SG_VLAT ((AstGateMask)1 << 42)
#define AST_SG_MATHPRE ((AstGateMask)1 << 43)
#define AST_SG_INTERCHANGE ((AstGateMask)1 << 44)
#define AST_SG_FUSION ((AstGateMask)1 << 45)
#define AST_SG_TILE ((AstGateMask)1 << 46)

enum {
	SO_GATE_TEMPLATES = 1u,
	SO_GATE_PROMOTE = 2u,
	SO_GATE_INLINE = 4u,
	SO_GATE_NOCALLFUL = 8u
};

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

static MCC_GATE_INLINE AstGateMask ast_gate_from_perfn(unsigned best_cfg) {
	return ((best_cfg & 1u) ? AST_SG_TEMPLATES : 0) |
				 ((best_cfg & 2u) ? AST_SG_PROMOTE : 0) |
				 ((best_cfg & 4u) ? AST_SG_INLINE : 0);
}

static MCC_GATE_INLINE unsigned ast_gate_to_perfn(AstGateMask g) {
	return ((g & AST_SG_TEMPLATES) ? 1u : 0) | ((g & AST_SG_PROMOTE) ? 2u : 0) |
				 ((g & AST_SG_INLINE) ? 4u : 0);
}

#endif
