#!/usr/bin/env python3
"""Byte-level Replay_IR (RIR) coverage census and ratchet gate.

Body counts overweight tiny functions, so this measures the honest denominator:
bytes of emitted .text. RIR has two layers and only the second one has a gap:

  capture  the op stream, replayed and byte-checked by rir_verify() under
           MCC_REPLAY_IR=3.
  arena    rir_to_arena() -> rir_prod_take() -> ast_replay_body(), what
           production actually ships, censused by MCC_RIR_PROD=2.

For the arena layer the split that matters is semantic, not byte-exact:

  kept       modelled, replayed, byte-identical to the parser: shipped.
  discarded  modelled, replay ran to completion, a memcmp against the
             parser's second derivation disagreed (len/bytes/rellen/
             relcontent/posterr) so the parser's bytes were restored.
             Coverage exists here; the byte gate threw it away. Byte
             divergence is not by itself evidence of a defect, so this is
             NOT counted as a gap -- but it is not proof of correctness
             either: --nofb-probe keeps one such body at a time and diffs
             the program, and banks the set that miscompiles (empty today;
             structs_unions/union_cast.c::main used to be in it). It also
             costs optimization: ast_run_strat_seq gates every strategy on
             `faithful`.
  gap        never modelled at all (skip: asm, regdangle, mismatch, invalid,
             unsafe, noops, unbal, ovf, bail, revargs, replayok) or modelled
             but the replay did not run to completion (abort). This is the
             only true coverage gap and the only thing the ratchet banks.

Bytes outside any body (prologue/epilogue, inter-function padding) belong to
neither layer and are reported separately so the accounting reconciles against
the object's real .text size.

Beside modelled/gap there is a third, orthogonal census: LOWERABLE. Byte
coverage only proves the arena can regenerate the SAME target's code.
Retargeting a slice to a GPU compute shader is strictly stronger, so the
lowerable census asks a different question of the same arena: what share of it
is a candidate for shader lowering at all? The predicate is one function,
ast_low_node() in src/mccast.c, and it is deliberately conservative -- a node
counts as lowerable only when we are sure. Every node gets exactly one class:

  ok      nothing below applies
  asm     an inline-asm node (AST_OP_ASM / ASMGEN / ASMOPS)
  reg     a Ref whose r names host machine state: a hardware register
          (valmask < VT_CONST) or VT_CMP / VT_JMP / VT_JMPI
  opaque  AST_Poison, or an op with no portable meaning: VLA, VLA_RESTORE,
          VAARG, VASTART, GGOTO, the atomics (AXADD, AXCHG, ACMPXCHG,
          ACASRMW, BITB) and the complex builders (CPLXBUILD, IMAG)
  call    AST_Invoke.  Slices are the code BETWEEN anonymous invokes, so an
          invoke inside a region ends it by definition
  type    not type-complete (see MCC_RIR_STAMP below: the class is computed
          over ast_stype_t, the static type, not over the emitter's type_t):
          ast_bad_type() (struct, bitfield, long double,
          __int128, _Float128), a Convert with no operand, or a dereference
          (Load / Store / MEMBER / MEMBER_ARROW) whose address operand does
          not derive from any node with a recorded pointer/array/struct type.
          That last clause used to catch a cast emitting no machine code and
          so leaving no trace in the arena, which made the replay's -> see an
          integer.  Fixed 2026-08-06: rir_hook_cast_type() records the
          post-cast CType, so what remains here is genuine -- a value whose
          static type has no portable scalar meaning.
  frame   the value's meaning is a host frame offset: AST_OP_ADDR, a VT_LLOCAL
          Ref, or a VT_LOCAL Ref (see the locals level below)
  global  a Ref carrying VT_SYM -- a host link-time address

A node is CLEAN when its whole subtree is ok, i.e. it roots a maximal
lowerable region.  A body is lowerable when every node in its arena is ok.

The VT_LOCAL rule is the one genuinely open question (a local's `ival` is the
parser's own frame offset), so it is a level, and all three are measured in a
single walk:

  0 strict   every VT_LOCAL Ref is frame-dependent.
  1 default  a VT_LOCAL Ref is frame-INdependent iff no node anywhere in the
             same arena takes the address of a local (no AST_OP_ADDR, no
             VT_LLOCAL Ref) and the Ref's own type is a scalar the arena
             models.  Rationale: with no address-of-local in the body no
             local's address is observable, so every slot is a pure value
             cell and its offset is only a name.
  2 loose    a VT_LOCAL Ref never disqualifies.

Pointer VALUES are allowed at every level: a pointer that reaches the region
is a buffer binding to be resolved at the region boundary, not a frame
dependency.

Compiler side: MCC_RIR_PROD=2 (or MCC_RIR_LOW=1) emits `[rir-low]` and
`[rir-low-why]` aggregates, plus `[rir-untyped]` / `[rir-untyped-kind]`: how
many arena nodes carry no static type at all, split into genuinely unknown and
explicitly void, per node kind.  MCC_RIR_LOW_DUMP=<func> prints the per-node
classification of one body, or of every body when it is `*`.  Only percentages
are banked, never totals: the census body total is not stable run to run at
fixed HEAD.

MCC_RIR_STAMP=1 makes rir_to_arena record, for every node, the static CType of
the value that node produces, in a shadow view the emitter never reads
(ast_stype_*); MCC_RIR_STAMP=2 additionally derives the types the vstack never
witnessed.  Default off.  The census reads the shadow view, so the toggle moves
the lowerable numbers without moving a single emitted byte.

The compiler side is `MCC_RIR_PROD=2` (see src/mccrir.c rir_prod_note): one
tab-separated row per body

    verdict  file  func  why  unfaithful  bytes  nrawops

plus, at exit, the aggregate `[rir-prod-*]` lines, appended to that same file
when MCC_RIR_PROD_OUT is set (the capture layer's `[rir-capbytes]` goes to
stderr instead). Both are parsed here.

Usage:
  tools/rir-coverage.py <build-dir> [--levels O0,O1,O2,O3]
                        [--corpus self|wide|exec] [--layers both|arena|capture]
                        [--bank FILE] [--update-bank] [--update-bank-low]
                        [--update-bodies]
                        [--top N] [--json FILE] [--classify] [--check-gap-dir]
                        [--check-low-dir] [--nofb-probe] [--rebank-config]

--update-bodies writes ONLY tests/rir/lowerable-bodies.tsv, the per-body
inventory the ratchet attributes with.  --update-bank-low writes that AND the
percentage floors in the bank, which is a different decision: adding bodies to
the inventory cannot hide a regression (the comparison is over the intersection
of banked and present bodies), while moving a floor can and needs an
attribution first.  They were one switch until 2026-08-13, and this file's own
warning -- that a blind re-bank arrives as a convenience rather than as a
mistake -- is why they are two.

The `self` and `wide` corpora are the compiler's own source, so the banked
percentages are only comparable across builds that compile the same source into
src/mcc.c.  The bank records that configuration under "corpus_config" (see
corpus_config() below); a build that differs exits 77 (skip) rather than
reporting the dilution as a regression.  No build option shapes the corpus
today; what the recorded configuration does carry is LOW_EXCLUDE, the device
layer (src/mccgpu.c and src/mccgpu.h), which the LOWERABLE census alone drops
from numerator and denominator both.  That census was the one measurement whose
subject was also its author -- src/mcc.c amalgamates the SPIR-V emitter, so
every line added to the emitter enlarged the denominator of the ratchet that
guards it, and at 9abbcf5c it failed by 0.0001 points while absolute lowerable
nodes ROSE by 46.  Coverage and byte accounting are not self-referential and are
untouched by the exclusion.  --rebank-config, with --update-bank[-low], is the
deliberate way to move the recorded configuration; do not use it to bank a
non-default build.

THE LOWERABLE RATCHET IS TAKEN ON BODIES, NOT ON THE CORPUS RATIO.  Excluding
the device layer treated one instance of a general disease: every lowerable
percentage is a ratio whose denominator is the compiler's own source, so adding
source that is less lowerable than the source it joins drops the ratio without
anything having become less lowerable.  That is dilution, it is the normal
state of a growing compiler, and between 2026-08-09 and 2026-08-10 it took this
cell red twice -- nodes_pct_strict once and bodies_pct once -- each time costing
a full investigation that ended in a re-bank.  A ratchet that fails on ordinary
merges trains its readers to re-bank without looking, which is how a real
regression gets banked.

So the gate compares like with like.  MCC_RIR_LOW_BODY=1 makes the compiler emit
one [rir-low-body] row per body -- the same arithmetic as the [rir-low]
aggregate, one line per body instead of one per run -- and tests/rir/
lowerable-bodies.tsv banks, beside the percentages, WHICH bodies the census saw
and which of them were lowerable.  Two things are then enforced:

  * no body that was banked lowerable may stop being lowerable.  Named, per
    body, with no tolerance: this is what "a regression" means here.
  * each banked percentage is recomputed over the intersection of the banked
    body set and this run's, and compared against the bank.  Bodies added since
    the bank are excluded from BOTH sides, so adding any amount of unlowerable
    source moves this comparison by exactly zero.

Every run prints BANK DRIFT: the movement of each percentage split into the part
that comes from pre-existing bodies (the gated part) and the part that comes
from the corpus having changed shape (reported, never gated).  The --tol band
therefore now absorbs only edits to bodies the bank already knew, which is what
a tolerance can honestly absorb; the 0.05 band used to hide dilution as well,
and 0.0414pp of it accumulated unnoticed before the 2026-08-10 wave.

A corpus with no banked inventory (wide) and an mcc too old to emit the per-body
rows both fall back to the whole-corpus comparison, and say so in the failure
text rather than reporting an unattributed number.

Exit status is 0 when every level is at or above its banked coverage and the
byte accounting reconciles against the objects' real .text size.

A host the bank was not taken on does NOT skip the whole run.  Three banked
quantities are host-sensitive and only those three are dropped:

  lowerable   per-format by schema (see low_floor); MCC_HOST_* decides which
              source amalgamates into the corpus.
  residual    unattributed .text.  Darwin's mcc_tlv_thunk is 120 bytes of raw
              asm with no C body, so 120 here is correct against an ELF 0.
  kept_coverage  host-sensitive, but by far less than first reported, and both
              figures once blamed on the host were bank staleness.  The original
              96.156-vs-83.219 figure was an artifact: 96.156 was banked at
              879bf988, before 1ad3f1aa moved eleven passes across the -O ladder
              and took elf/x86-64 kept to 82.520.  Comparing a post-ladder
              darwin number against a pre-ladder bank attributed ~12.2 points of
              bank staleness to the host.  This file then claimed wide's
              93.4-on-darwin against 98.4-banked was 5 points BELOW the floor
              and that whether those 5 points were a host axis or a darwin
              modelling gap was UNMEASURED.  It was the same failure mode a
              second time: elf/x86-64 measured wide at 92.92-93.01 against that
              same 98.4 bank, so darwin agreed with Linux to within a tenth and
              the bank -- taken before chain-store was demoted off the ladder --
              was stale.  Root cause was a replay-fidelity bug, not a host axis:
              rir_to_arena() collapsed a childless AST_StoreVal over its
              AST_Store source unconditionally while only tagging the result
              when -fchain-store was on, so with the flag off the arena carried
              a chained store nothing re-expanded and ~110 bodies re-emitted
              different bytes.  Fixed in src/mccrir.c (IR_OP_VSTORE) and both
              banks refreshed; wide is now 92.92 / 96.60 / 96.65 / 96.65 on elf.
              The remaining host spread is: self 84.1-84.5 on darwin/aarch64
              against 82.9-91.9 banked, wide ~93.4 against 92.92-96.65.  Both
              stay in the unbanked-host partial skip, which is keyed on the
              lowerable floors and covers every corpus alike; arming
              kept_coverage per host still needs a per-format schema.

capture coverage, modelled coverage and the census self-reconciliation still
run everywhere, and that is measured, not assumed: on darwin/aarch64 the wide
corpus reports capture 100.000% and modelled 100.000% at -O1, the same values
the bank carries from elf/x86-64.  Both are near-saturated ratios that do carry
across hosts, so keeping them armed off Linux costs nothing and is worth a
great deal -- it is what catches an arena that stopped modelling something.

The run reports 0 when anything was gated and 77 only when a skip is genuinely
all that is left; every skip names what it dropped and why.
"""
import argparse, hashlib, json, os, re, shlex, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_BANK = os.path.join(ROOT, "tests", "rir", "coverage-bank.json")

UNF = ["len", "bytes", "rellen", "relcontent", "abort", "posterr"]
WHY = ["bail", "noops", "capbad", "unbal", "ovf", "mismatch", "invalid",
       "unsafe", "asm", "regdangle", "revargs", "replayok"]

LOWCLS = ["asm", "reg", "opaque", "call", "type", "frame", "global"]
LOWKEYS = (["bodies", "bytes", "nodes"] +
           ["clean%d" % i for i in range(3)] +
           ["ok%d" % i for i in range(3)] +
           ["okbytes%d" % i for i in range(3)] +
           ["regions%d" % i for i in range(3)] +
           ["nmin%d" % i for i in range(3)] +
           ["nbig%d" % i for i in range(3)])
LOWLVL = ["strict", "default", "loose"]
LOW_MIN, LOW_BIG = 3, 16
LOW_BANKED = ["bodies_pct", "bytes_pct", "nodes_pct",
              "nodes_pct_strict", "nodes_pct_loose",
              "region_nodes_pct", "big_region_nodes_pct"]


def find_mcc(bdir):
    mcc = os.path.join(bdir, "mcc")
    if not os.access(mcc, os.X_OK) and os.access(mcc + ".exe", os.X_OK):
        mcc += ".exe"
    if not os.access(mcc, os.X_OK):
        sys.exit("rir-coverage: no mcc at " + mcc)
    return mcc


def self_flags(bdir):
    cdb = os.path.join(bdir, "compile_commands.json")
    if not os.path.exists(cdb):
        return []
    cc = json.load(open(cdb))
    rec = [x for x in cc if os.path.basename(x["file"]) == "mcc.c"]
    if not rec:
        return []
    if "arguments" in rec[0]:
        argv = rec[0]["arguments"][1:]
    else:
        cmd = rec[0]["command"]
        if os.name == "nt":
            cmd = re.sub(r'\\(?!")', "/", cmd)
        argv = shlex.split(cmd)[1:]
    return [a for a in argv
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


# Every -D that changes WHICH source src/mcc.c amalgamates. Getting this list
# wrong is silent: the guard exists to turn corpus-shape changes into a skip
# rather than a fake regression, and a missing entry means it does neither.
# MCC_EMBED_JIT gates two whole translation units (src/libmcc.c includes
# mccjit_intent.c and mccjit_embed.c under it) and is a user-visible CMake
# option defaulting ON, so it was the largest corpus-shaping option the guard
# could not see.
CORPUS_DEFS = ["MCC_DIAG", "MCC_EMBED_JIT"]

LOW_EXCLUDE = "src/mccgpu.c,src/mccgpu.h"


def check_low_exclude(root):
    """LOW_EXCLUDE is matched as a path SUFFIX inside the compiler, so an entry
    that names a file which no longer exists stops matching and excludes
    nothing -- and the lowerable ratchet then compares a full-corpus number
    against a floor banked on an excluded corpus. That is a fake regression
    with no diagnostic, which is sweep row 22's second half. Renaming or
    splitting src/mccgpu.c is exactly the move that triggers it, so check that
    every entry still names something."""
    missing = [e for e in LOW_EXCLUDE.split(",")
               if e and not os.path.exists(os.path.join(root, e))]
    if not missing:
        return None
    return ("LOW_EXCLUDE names %d path(s) that no longer exist: %s. The "
            "exclusion is a suffix match, so these now exclude NOTHING and "
            "every lowerable figure below is over a corpus the banked floor "
            "was not taken on. Rename the entry with the file, or re-bank "
            "deliberately with --rebank-config."
            % (len(missing), ", ".join(missing)))


def corpus_config(flags):
    """The build options that change WHICH source src/mcc.c amalgamates.

    Every banked percentage on the `self` and `wide` corpora is a ratio taken
    over the compiler's own source, so a build option that compiles extra source
    into src/mcc.c moves every one of them without anything having regressed.
    MCC_GPU used to be exactly that; the GPU compute backend is unconditional
    now, so it is in every corpus and no longer a variable.  The list is empty
    until some other option starts shaping the source again.

    The bank records the configuration it was taken on under "corpus_config";
    a build whose configuration differs is not comparable to it and skips
    instead of failing.  A source-shaping option missing from this list makes
    the ratchet FAIL rather than skip, which is the safe direction: it demands
    an explanation instead of quietly accepting a moved number.
    """
    cfg = {}
    for name in CORPUS_DEFS:
        val = "0"
        for f in flags:
            if f == "-D" + name:
                val = "1"
            elif f.startswith("-D" + name + "="):
                val = f.split("=", 1)[1]
        cfg[name] = val
    cfg["LOW_EXCLUDE"] = LOW_EXCLUDE
    return cfg


def fmt_config(cfg):
    return ",".join("%s=%s" % kv for kv in sorted(cfg.items())) or "(none)"


def selfcheck_corpus_guard(mutate):
    global CORPUS_DEFS
    if mutate:
        full = CORPUS_DEFS
        for dropped in full:
            CORPUS_DEFS = [d for d in full if d != dropped]
            try:
                blind = corpus_config([]) == corpus_config(["-D%s=1" % dropped])
            finally:
                CORPUS_DEFS = full
            if not blind:
                print("FAIL rir/corpus-guard-known-positive: dropping %r from "
                      "CORPUS_DEFS left corpus_config still sensitive to its flip, "
                      "so the gate cannot see that option going unguarded" % dropped)
                return 1
        print("rir/corpus-guard-known-positive: OK -- each of %s is load-bearing; "
              "dropping any one makes corpus_config blind to that option's flip and "
              "the gate catches it" % (full,))
        return 0
    if "MCC_EMBED_JIT" not in CORPUS_DEFS:
        print("FAIL rir/corpus-guard: MCC_EMBED_JIT is a source-shaping build option "
              "(it pulls two TUs into src/mcc.c) absent from CORPUS_DEFS, so a build "
              "that flips it would compare across corpus shapes instead of refusing")
        return 1
    base = corpus_config([])
    for opt in CORPUS_DEFS:
        if corpus_config(["-D%s=1" % opt]) == base:
            print("FAIL rir/corpus-guard: flipping %s does not change corpus_config, "
                  "so the bank's corpus_config guard cannot see it" % opt)
            return 1
    print("rir/corpus-guard: OK -- MCC_EMBED_JIT enrolled; flipping any of %s changes "
          "corpus_config, so a bank taken with an option off refuses (skip 77) against "
          "a build with it on rather than comparing across corpus shapes" % (CORPUS_DEFS,))
    return 0


def gap_levels(path):
    """Levels a tests/rir/gap fixture must reproduce its class at.

    A fixture whose class is only reachable under an optimization that does not
    run at every level says so with a `rir-gap-levels: O2,O3` comment; without
    one every level in --levels is required.
    """
    tag = "rir-gap-levels:"
    with open(path) as f:
        head = f.read(4096)
    if tag not in head:
        return None
    rest = head.split(tag, 1)[1].split("*/", 1)[0].split("\n", 1)[0]
    return [x.strip() for x in rest.split(",") if x.strip()]


def text_size(path):
    """.text byte count of an ELF/PE/Mach-O object, or None if unparsed."""
    try:
        d = open(path, "rb").read()
    except OSError:
        return None
    if d[:4] == b"\x7fELF":
        is64 = d[4] == 2
        le = "<" if d[5] == 1 else ">"
        if is64:
            e_shoff, = struct.unpack_from(le + "Q", d, 0x28)
            e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(le + "HHH", d, 0x3a)
        else:
            e_shoff, = struct.unpack_from(le + "I", d, 0x20)
            e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(le + "HHH", d, 0x2e)
        def sh(i):
            o = e_shoff + i * e_shentsize
            if is64:
                name, typ = struct.unpack_from(le + "II", d, o)
                size, = struct.unpack_from(le + "Q", d, o + 0x20)
                off, = struct.unpack_from(le + "Q", d, o + 0x18)
            else:
                name, typ = struct.unpack_from(le + "II", d, o)
                off, size = struct.unpack_from(le + "II", d, o + 0x10)
            return name, typ, off, size
        _, _, stroff, _ = sh(e_shstrndx)
        tot = 0
        for i in range(e_shnum):
            name, typ, off, size = sh(i)
            end = d.index(b"\0", stroff + name)
            nm = d[stroff + name:end].decode()
            if nm == ".text" or nm.startswith(".text."):
                tot += size
        return tot
    if d[:2] == b"MZ" or d[:4] in (b"\x64\x86\x00\x00",):
        pass
    return None


def host_objfmt(mcc):
    """Object format of the host mcc was built for: 'elf', 'pe', 'macho', None.

    The self/wide banks are ratios over src/ compiled for the host, and it is the
    host -- not the emitted object -- that decides them: MCC_HOST_* selects which
    #ifdef branches amalgamate into the corpus.  src/mccrun.c contributes a
    120-byte raw-asm TLS thunk (mcc_tlv_thunk, MCC_RUN_TLS_MACHO) on Darwin and
    nothing on Linux; it lands in .text with no C body behind it, so it moves the
    unattributed residual from 0 to 120 on its own.  A bank taken on a different
    host is not comparable and belongs in the corpus_config skip, not a
    regression.

    Probe the mcc binary itself rather than an object it emits.  mcc -c
    cross-emits ELF on every host, so the emitted format is 'elf' on macOS too
    and says nothing about which branches were compiled -- keying on it left this
    skip permanently disarmed off Linux.
    """
    fmt = None
    try:
        with open(mcc, "rb") as f:
            d = f.read(4)
        if d[:4] == b"\x7fELF":
            fmt = "elf"
        elif d[:2] == b"MZ":
            fmt = "pe"
        elif d[:4] in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe",
                       b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca"):
            fmt = "macho"
    except OSError:
        fmt = None
    if fmt is None:
        fmt = {"linux": "elf", "win32": "pe", "darwin": "macho",
               "cygwin": "pe", "msys": "pe"}.get(sys.platform)
    return fmt


def arena_floor(entry, key, fmt):
    """A per-format arena scalar, with a legacy flat value reading as elf.

    residual and kept_coverage are host-format properties, not portable ones:
    src/mccrun.c's mcc_tlv_thunk is 120 bytes of raw asm with no C body on
    Darwin, so a macho residual is legitimately nonzero where the banked elf
    value is 0.  Banking them flat is what forced the whole lowerable cluster
    to partial-skip on any host that is not the one they were taken on.
    """
    v = entry.get(key)
    if isinstance(v, dict):
        return v.get(fmt)
    return v if fmt == "elf" else None


def low_floor(entry, fmt):
    """The lowerable floor for host format `fmt` in a bank's per-opt entry.

    Floors are recorded per host format because MCC_HOST_* decides which source
    amalgamates into the corpus, so PE and ELF do not share numbers.  A legacy
    bank stored one flat set taken on ELF; treat that as the elf floor.
    """
    if not isinstance(entry, dict):
        return None
    if "bodies_pct" in entry:
        return entry if fmt == "elf" else None
    return entry.get(fmt)


PERFMT_ARENA_KEYS = ("residual", "kept_coverage", "failed")


def merge_bank(prev, out, fmt):
    for opt, layers in out.items():
        dst = prev.setdefault(opt, {})
        for lname, v in layers.items():
            if lname == "lowerable":
                cur = dst.get("lowerable")
                if not isinstance(cur, dict) or "bodies_pct" in cur:
                    cur = {"elf": cur} if cur else {}
                cur.update(v)
                dst["lowerable"] = cur
            else:
                keep = dst.get(lname, {})
                for k in PERFMT_ARENA_KEYS:
                    if k not in v:
                        continue
                    cur = keep.get(k)
                    if not isinstance(cur, dict):
                        cur = {"elf": cur} if cur is not None else {}
                    cur[fmt] = v[k]
                    v[k] = cur
                dst[lname] = v
    return prev


def _bankkeying_probe(mergefn):
    prev = {"O0": {"arena": {"failed": {"elf": 9, "macho": 17},
                             "residual": {"elf": 0, "macho": 120},
                             "kept_coverage": {"elf": 82.5, "macho": 82.7},
                             "coverage": 99.0, "text": 1000}}}
    out = {"O0": {"arena": {"failed": 9, "residual": 0, "kept_coverage": 82.5,
                            "coverage": 99.1, "text": 1010}}}
    res = mergefn(prev, out, "elf")
    a = res["O0"]["arena"]
    for k, other, want, mine in (("failed", "macho", 17, 9),
                                 ("residual", "macho", 120, 0),
                                 ("kept_coverage", "macho", 82.7, 82.5)):
        v = a.get(k)
        if not isinstance(v, dict):
            return False, ("%s collapsed to a flat %r on an elf re-bank; the "
                           "macho floor is gone" % (k, v))
        if v.get(other) != want:
            return False, ("%s[%s] erased by an elf re-bank (got %r, want %r)"
                           % (k, other, v.get(other), want))
        if v.get("elf") != mine:
            return False, "%s[elf] not updated to this host's measurement" % k
    if a.get("coverage") != 99.1 or a.get("text") != 1010:
        return False, "a portable scalar was not updated by this host's run"
    leg = {"O1": {"arena": {"failed": 5}}}
    lr = mergefn(leg, {"O1": {"arena": {"failed": 3}}}, "macho")
    f = lr["O1"]["arena"]["failed"]
    if not isinstance(f, dict) or f.get("elf") != 5 or f.get("macho") != 3:
        return False, "a legacy flat floor was not lifted per-format (got %r)" % (f,)
    return True, ("per-format floors (%s) survive a same-host re-bank"
                  % ", ".join(PERFMT_ARENA_KEYS))


def selfcheck_bankkeying(mutate):
    global PERFMT_ARENA_KEYS
    if mutate:
        full = PERFMT_ARENA_KEYS
        for dropped in full:
            PERFMT_ARENA_KEYS = tuple(k for k in full if k != dropped)
            try:
                ok, _ = _bankkeying_probe(merge_bank)
            finally:
                PERFMT_ARENA_KEYS = full
            if ok:
                print("FAIL rir/bank-keying-known-positive: dropping %r from the "
                      "per-format key set still passed the invariant, so the gate "
                      "cannot see that floor being erased" % dropped)
                return 1
        print("rir/bank-keying-known-positive: OK -- each of %s is load-bearing; "
              "removing any one lets an elf re-bank erase the macho floor and the "
              "gate catches it" % (full,))
        return 0
    ok, msg = _bankkeying_probe(merge_bank)
    if not ok:
        print("FAIL rir/bank-keying: %s" % msg)
        return 1
    print("rir/bank-keying: OK -- %s" % msg)
    return 0


def run_one(mcc, flags, src, opt, out_o, tsv, env0, layer):
    env = dict(env0)
    if layer == "capture":
        env["MCC_REPLAY_IR"] = "3"
    else:
        env["MCC_RIR_PROD"] = "2"
        env["MCC_RIR_PROD_OUT"] = tsv
        if opt == "O0":
            env["MCC_FORCE_REPLAY"] = "1"
    cmd = [mcc] + flags + ["-" + opt, "-c", src, "-o", out_o]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT, env=env)
    return p


def parse_report(stderr, tu=""):
    r = {"used": 0, "fallback": 0, "skip": 0, "lowbody_short": 0,
         "b_used": 0, "b_fallback": 0, "b_skip": 0, "b_body": 0,
         "fn_n": 0, "fn_bytes": 0, "unnoted": 0, "b_unnoted": 0, "b_nonbody": 0,
         "raw_used": 0, "raw_fallback": 0, "raw_skip": 0,
         "b_raw_used": 0, "b_raw_fallback": 0, "b_raw_skip": 0,
         "cap_fn": 0, "cap_faithful": 0,
         "cap_b_faith": 0, "cap_b_unfaith": 0, "cap_b_err": 0, "cap_n_err": 0,
         "cap_raw_fn": 0, "cap_raw_b": 0,
         "reemit": 0, "b_reemit": 0,
         "unf": {}, "why": {}, "low": {}, "lowbody": []}
    for k in LOWKEYS:
        r["low_" + k] = 0
    for line in stderr.splitlines():
        line = line.replace("\r", "")
        if line.startswith("[rir-low-body]\t"):
            g = line.split("\t")
            if len(g) >= 10:
                r["lowbody"].append((g[1].replace("\\", "/"), g[2]) +
                                    tuple(int(x) for x in g[3:10]) + (tu,))
            else:
                r["lowbody_short"] += 1
            continue
        f = line.split()
        if not f:
            continue
        if f[0] == "[rir-total]":
            m = dict(kv.split("=") for kv in f[1:] if "=" in kv)
            r["cap_fn"] += int(m.get("fn", 0))
            r["cap_faithful"] += int(m.get("faithful", 0))
        elif f[0] == "[rir-capbytes]":
            m = dict(kv.split("=") for kv in f[1:] if "=" in kv)
            r["cap_b_faith"] += int(m["faithful"])
            r["cap_b_unfaith"] += int(m["unfaithful"])
            r["cap_b_err"] += int(m["rerror"])
            r["cap_n_err"] += int(m["rerrorfn"])
            r["cap_raw_fn"] += int(m["rawfn"])
            r["cap_raw_b"] += int(m["rawbytes"])
            r["fn_n"] += int(m["fn"])
            r["fn_bytes"] += int(m["fnbytes"])
            r["unnoted"] += int(m["unnoted"])
            r["b_unnoted"] += int(m["unnotedbytes"])
            r["reemit"] += int(m.get("reemit", 0))
            r["b_reemit"] += int(m.get("reemitbytes", 0))
        elif f[0] == "[rir-prod-raw]":
            m = dict(kv.split("=") for kv in f[1:])
            r["raw_used"] += int(m["used"])
            r["raw_fallback"] += int(m["fallback"])
            r["raw_skip"] += int(m["skip"])
            r["b_raw_used"] += int(m["usedbytes"])
            r["b_raw_fallback"] += int(m["fallbackbytes"])
            r["b_raw_skip"] += int(m["skipbytes"])
        elif f[0] == "[rir-prod-total]":
            for kv in f[1:]:
                k, v = kv.split("=")
                r[k] += int(v)
        elif f[0] == "[rir-prod-bytes]":
            for kv in f[1:]:
                k, v = kv.split("=")
                r["b_" + k] += int(v)
        elif f[0] == "[rir-prod-fn]":
            m = dict(kv.split("=") for kv in f[1:])
            r["fn_n"] += int(m["n"])
            r["fn_bytes"] += int(m["bytes"])
            r["unnoted"] += int(m["unnoted"])
            r["b_unnoted"] += int(m["unnotedbytes"])
            r["b_nonbody"] += int(m["nonbody"])
            r["reemit"] += int(m.get("reemit", 0))
            r["b_reemit"] += int(m.get("reemitbytes", 0))
        elif f[0] in ("[rir-low]", "[rir-low-region]"):
            m = dict(kv.split("=") for kv in f[1:] if "=" in kv)
            for k in LOWKEYS:
                r["low_" + k] += int(m.get(k, 0))
        elif f[0] == "[rir-low-why]":
            m = dict(kv.split("=") for kv in f[1:] if "=" in kv)
            name = f[1].split("=")[0]
            cur = r["low"].setdefault(name, [0, 0, 0, 0, 0])
            for i, k in enumerate((name, "bytes", "nodes", "sole", "solebytes")):
                cur[i] += int(m.get(k, 0))
        elif f[0] in ("[rir-prod-unfaithful]", "[rir-prod-why]"):
            key = "unf" if f[0] == "[rir-prod-unfaithful]" else "why"
            name, n = f[1].split("=")
            b = int(f[2].split("=")[1])
            cur = r[key].setdefault(name, [0, 0])
            cur[0] += int(n)
            cur[1] += b
    return r


def merge(a, b):
    for k, v in b.items():
        if isinstance(v, dict):
            for name, pair in v.items():
                cur = a[k].setdefault(name, [0] * len(pair))
                for i in range(len(pair)):
                    cur[i] += pair[i]
        else:
            a[k] += v
    return a


def read_rows(tsv):
    rows = []
    if not os.path.exists(tsv):
        return rows
    for line in open(tsv, errors="replace"):
        f = line.replace("\r", "").rstrip("\n").split("\t")
        if len(f) < 7 or f[0].startswith("["):
            continue
        f = f[:7]
        try:
            f[5] = int(f[5])
            f[6] = int(f[6])
        except ValueError:
            continue
        rows.append(f)
    return rows


def census(mcc, flags, sources, opt, layer="arena", keep_rows=True):
    env0 = dict(os.environ)
    for k in ("MCC_RIR_PROD", "MCC_RIR_PROD_OUT", "MCC_FORCE_REPLAY",
              "MCC_REPLAY_IR", "MCC_REPLAY_IR_OUT", "MCC_RIR_FORCE",
              "MCC_TEST_OPT"):
        env0.pop(k, None)
    env0["MCC_RIR_LOW_EXCLUDE"] = LOW_EXCLUDE
    env0["MCC_RIR_LOW_BODY"] = "1"
    agg = parse_report("")
    rows = []
    text = 0
    fails = []
    with tempfile.TemporaryDirectory() as td:
        tsv = os.path.join(td, "prod.tsv")
        for i, src in enumerate(sources):
            out_o = os.path.join(td, "o%d.o" % i)
            fl = flags if os.path.basename(src) == "mcc.c" else []
            if os.path.exists(tsv):
                os.remove(tsv)
            p = run_one(mcc, fl, src, opt, out_o, tsv, env0, layer)
            if p.returncode != 0:
                fails.append((src, p.stderr.strip().splitlines()[-1:] or [""]))
                continue
            merge(agg, parse_report(p.stderr, low_rel(src)))
            if layer != "capture" and os.path.exists(tsv):
                merge(agg, parse_report(open(tsv, errors="replace").read(),
                                        low_rel(src)))
                if keep_rows:
                    rows.extend(read_rows(tsv))
            t = text_size(out_o)
            if t is not None:
                text += t
    agg["text"] = text
    agg["rows"] = rows
    agg["failed"] = fails
    return agg


def classify(rows):
    """rows -> {class: [count, bytes, (biggest file, func, bytes), rawn, rawb]}"""
    out = {}
    for verdict, f, fn, why, unf, nb, nraw in rows:
        if verdict == "used":
            continue
        cls = unf if verdict == "fallback" else ("skip:" + (why or "?"))
        e = out.setdefault(cls, [0, 0, None, 0, 0])
        e[0] += 1
        e[1] += nb
        if e[2] is None or nb > e[2][2]:
            e[2] = (f, fn, nb)
        if nraw:
            e[3] += 1
            e[4] += nb
    return out


def pct(a, b):
    return 100.0 * a / b if b else 0.0


DISCARD = ("len", "bytes", "rellen", "relcontent", "posterr")


def lowerable(c):
    """Lowerable census -> percentages only (see the module docstring)."""
    nb, nn = c["low_bodies"], c["low_nodes"]
    bb = c["low_bytes"]
    out = {"bodies": nb, "nodes": nn,
           "bodies_pct": round(pct(c["low_ok1"], nb), 4),
           "bytes_pct": round(pct(c["low_okbytes1"], bb), 4),
           "nodes_pct": round(pct(c["low_clean1"], nn), 4),
           "nodes_pct_strict": round(pct(c["low_clean0"], nn), 4),
           "nodes_pct_loose": round(pct(c["low_clean2"], nn), 4),
           "bodies_pct_strict": round(pct(c["low_ok0"], nb), 4),
           "bodies_pct_loose": round(pct(c["low_ok2"], nb), 4),
           "bytes_pct_strict": round(pct(c["low_okbytes0"], bb), 4),
           "bytes_pct_loose": round(pct(c["low_okbytes2"], bb), 4),
           "region_nodes_pct": round(pct(c["low_nmin1"], nn), 4),
           "big_region_nodes_pct": round(pct(c["low_nbig1"], nn), 4),
           "region_nodes_pct_strict": round(pct(c["low_nmin0"], nn), 4),
           "big_region_nodes_pct_strict": round(pct(c["low_nbig0"], nn), 4),
           "region_nodes_pct_loose": round(pct(c["low_nmin2"], nn), 4),
           "big_region_nodes_pct_loose": round(pct(c["low_nbig2"], nn), 4),
           "regions": c["low_regions1"],
           "blockers": {}}
    for name in LOWCLS:
        n, b, nd, sn, sb = c["low"].get(name, [0, 0, 0, 0, 0])
        out["blockers"][name] = {
            "bodies_pct": round(pct(n, nb), 4),
            "bytes_pct": round(pct(b, bb), 4),
            "nodes_pct": round(pct(nd, nn), 4),
            "sole_bodies_pct": round(pct(sn, nb), 4),
            "sole_bytes_pct": round(pct(sb, bb), 4)}
    return out


def print_lowerable(low):
    print("   LOWERABLE census (provisional predicate; locals level 1 = default)")
    print("     denominator: %d arena-modelled bodies, %d arena nodes"
          % (low["bodies"], low["nodes"]))
    print("     WHOLE BODIES lowerable  %.3f%% of bodies  %.3f%% of modelled "
          "body bytes" % (low["bodies_pct"], low["bytes_pct"]))
    print("     REGIONS (nodes in a maximal lowerable subtree)  "
          "strict %.3f%%  default %.3f%%  loose %.3f%%"
          % (low["nodes_pct_strict"], low["nodes_pct"], low["nodes_pct_loose"]))
    print("     whole bodies by locals level: strict %.3f%%  default %.3f%%  "
          "loose %.3f%%  (of body bytes)"
          % (low["bytes_pct_strict"], low["bytes_pct"], low["bytes_pct_loose"]))
    print("     nodes in maximal regions >=%d nodes  strict %.3f%%  "
          "default %.3f%%  loose %.3f%%"
          % (LOW_MIN, low["region_nodes_pct_strict"], low["region_nodes_pct"],
             low["region_nodes_pct_loose"]))
    print("     nodes in maximal regions >=%d nodes  strict %.3f%%  "
          "default %.3f%%  loose %.3f%%   (%d maximal regions)"
          % (LOW_BIG, low["big_region_nodes_pct_strict"],
             low["big_region_nodes_pct"], low["big_region_nodes_pct_loose"],
             low["regions"]))
    print("     blockers (level 1), sorted by the bytes they alone reject:")
    bl = low["blockers"]
    for name in sorted(bl, key=lambda k: (-bl[k]["sole_bytes_pct"],
                                          -bl[k]["bytes_pct"])):
        e = bl[name]
        if not e["bytes_pct"] and not e["nodes_pct"]:
            continue
        print("       %-8s blocks %7.3f%% of body bytes  (%7.3f%% of bodies), "
              "SOLE blocker of %7.3f%%,  %6.3f%% of nodes"
              % (name, e["bytes_pct"], e["bodies_pct"], e["sole_bytes_pct"],
                 e["nodes_pct"]))


LB_NODES, LB_C0, LB_C1, LB_C2, LB_BYTES, LB_NMIN, LB_NBIG = range(2, 9)
LB_TU = 9
LOW_INV_NAME = "lowerable-bodies.tsv"


def low_inventory_path(bank):
    return os.path.join(os.path.dirname(bank) or ".", LOW_INV_NAME)


def low_rel(p):
    p = p.replace("\\", "/")
    r = ROOT.replace("\\", "/") + "/"
    return p[len(r):] if p.startswith(r) else p


def low_body_index(c, why=None):
    """(file, func, tu) -> per-body lowerable row, or None with a reason.

    The rows are the same arithmetic the compiler's own [rir-low] aggregate
    reports, one line per body instead of one line per run, so anything derived
    from them can be recomputed over a SUBSET of the corpus.  That is the whole
    point: a ratio over the whole corpus moves when the corpus grows, and a
    ratio over the bodies the bank already knew about does not.

    The key carries the translation unit because (file, func) is not unique and
    the compiler cannot make it so: [rir-low-body] reports the lexer's current
    file, so a header body is reported once per TU that includes it.  Keying on
    (file, func) alone collapsed 4550 rows to 4538 and disarmed the whole
    per-body ratchet for the whole corpus -- silently, because the corpus-wide
    fallback below it still passed.  Five keys did that: `<command line>`
    helpers emitted once per TU, and bitfields_ms.c #including bitfields.c.

    `why` collects the reason a None is returned, so the caller can say which
    of the three causes fired instead of naming all three.
    """
    def no(reason):
        if why is not None:
            why.append(reason)
        return None

    rows = c.get("lowbody") or []
    if not rows:
        return no("this mcc emitted no [rir-low-body] rows (needs "
                  "MCC_RIR_LOW_BODY=1 and a build that honours it)")
    if c.get("lowbody_short"):
        return no("%d [rir-low-body] row(s) had fewer than 10 fields, so the "
                  "row format moved under this tool" % c["lowbody_short"])
    idx = {}
    for t in rows:
        idx[(low_rel(t[0]), t[1], t[LB_TU])] = t
    if len(idx) != len(rows):
        dup = {}
        for t in rows:
            dup.setdefault((low_rel(t[0]), t[1], t[LB_TU]), []).append(t)
        names = sorted(k for k, v in dup.items() if len(v) > 1)
        return no("%d of %d rows share a (file, func, tu) key, so no per-body "
                  "attribution is possible: %s"
                  % (len(rows) - len(idx), len(rows),
                     ", ".join("%s::%s in %s" % k for k in names[:5])))
    tot = low_body_totals(rows)
    for k, agg in (("bodies", "low_bodies"), ("nodes", "low_nodes"),
                   ("clean1", "low_clean1"), ("ok1", "low_ok1"),
                   ("bytes", "low_bytes")):
        if tot[k] != c[agg]:
            return no("per-body rows do not reconcile with the [rir-low] "
                      "aggregate: %s is %d by row and %d in aggregate"
                      % (k, tot[k], c[agg]))
    return idx


def low_body_name(key):
    """A (file, func, tu) key, printed so the tu is visible only when it adds.

    The tu disambiguates a body reported once per TU that includes it; naming
    it on every body would bury the ones where it is the whole point.
    """
    return ("%s:%s" % (key[0], key[1]) if not key[2] or key[2] == key[0]
            else "%s:%s [in %s]" % (key[0], key[1], key[2]))


def low_body_totals(rows):
    t = {"bodies": len(rows), "nodes": 0, "bytes": 0, "nmin1": 0, "nbig1": 0,
         "clean0": 0, "clean1": 0, "clean2": 0,
         "ok0": 0, "ok1": 0, "ok2": 0, "okbytes1": 0}
    for r in rows:
        t["nodes"] += r[LB_NODES]
        t["bytes"] += r[LB_BYTES]
        t["nmin1"] += r[LB_NMIN]
        t["nbig1"] += r[LB_NBIG]
        for i in range(3):
            t["clean%d" % i] += r[LB_C0 + i]
            if r[LB_C0 + i] == r[LB_NODES]:
                t["ok%d" % i] += 1
                if i == 1:
                    t["okbytes1"] += r[LB_BYTES]
    return t


def low_body_pcts(rows):
    t = low_body_totals(rows)
    nb, nn, bb = t["bodies"], t["nodes"], t["bytes"]
    return {"bodies_pct": round(pct(t["ok1"], nb), 4),
            "bytes_pct": round(pct(t["okbytes1"], bb), 4),
            "nodes_pct": round(pct(t["clean1"], nn), 4),
            "nodes_pct_strict": round(pct(t["clean0"], nn), 4),
            "nodes_pct_loose": round(pct(t["clean2"], nn), 4),
            "region_nodes_pct": round(pct(t["nmin1"], nn), 4),
            "big_region_nodes_pct": round(pct(t["nbig1"], nn), 4)}


def low_body_mask(r):
    m = 0
    for i in range(3):
        if r[LB_C0 + i] == r[LB_NODES]:
            m |= 1 << i
    return m


def read_low_inventory(path):
    """-> (levels, {(corpus, fmt): {(file, func, tu): mask-per-level}}, dropped)

    Rows banked before 2026-08-13 have no tu column.  They are read with tu ""
    and matched by low_inventory_match's legacy arm rather than dropped, so a
    schema change does not silently retire every body the bank already knew.
    A row that is neither width IS a finding, so it is counted and reported.
    """
    levels, table, dropped = [], {}, 0
    if not os.path.exists(path):
        return levels, table, dropped
    for line in open(path, errors="replace"):
        line = line.replace("\r", "").rstrip("\n")
        if line.startswith("#levels\t"):
            levels = line.split("\t")[1].split(",")
            continue
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        if len(f) == 5:
            table.setdefault((f[0], f[1]), {})[(f[2], f[3], "")] = f[4]
        elif len(f) == 6:
            table.setdefault((f[0], f[1]), {})[(f[2], f[3], f[4])] = f[5]
        else:
            dropped += 1
    return levels, table, dropped


def write_low_inventory(path, levels, table):
    out = ["# rir-coverage lowerable body inventory; see tools/rir-coverage.py",
           "# corpus\tformat\tfile\tfunc\ttu\tone hex ok-bitmask per level, "
           "'-' where the body is absent",
           "# tu is the translation unit the body was compiled in: file+func is "
           "not unique,",
           "# because a header body is reported once per TU that includes it",
           "#levels\t" + ",".join(levels)]
    for corpus, fmt in sorted(table):
        for key in sorted(table[(corpus, fmt)]):
            out.append("%s\t%s\t%s\t%s\t%s\t%s"
                       % (corpus, fmt, key[0], key[1], key[2],
                          table[(corpus, fmt)][key]))
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    open(path, "w").write("\n".join(out) + "\n")


def update_low_inventory(a, fmt, low_index):
    """Re-take the banked body inventory for the corpus and format just run."""
    levels = a.levels.split(",")
    have = {opt: idx for opt, idx in low_index.items() if idx is not None}
    if len(have) != len(levels):
        print("rir-coverage: NOT writing %s: this mcc emitted per-body census "
              "rows for %d of %d levels" % (low_inventory_path(a.bank),
                                            len(have), len(levels)))
        return
    path = low_inventory_path(a.bank)
    old_levels, table, dropped = read_low_inventory(path)
    if dropped:
        print("rir-coverage: NOT writing %s: %d row(s) parse as neither the "
              "5-column nor the 6-column schema, and rewriting would erase them"
              % (path, dropped))
        return
    if old_levels and old_levels != levels:
        print("rir-coverage: %s was banked on levels %s and is being rewritten "
              "for %s; every other corpus in it is dropped and must be re-taken"
              % (path, ",".join(old_levels), ",".join(levels)))
        table = {}
    table[(a.corpus, fmt)] = {}
    keys = set()
    for opt in levels:
        keys |= set(have[opt])
    for key in keys:
        table[(a.corpus, fmt)][key] = "".join(
            "%x" % low_body_mask(have[opt][key]) if key in have[opt] else "-"
            for opt in levels)
    write_low_inventory(path, levels, table)
    print("banked %d lowerable-census bodies for %s/%s -> %s"
          % (len(keys), a.corpus, fmt, path))


def low_inventory_match(want, idx):
    """Line the banked bodies up with this run's.

    Exact (file, func, tu) first.  Then the legacy arm: a row banked before the
    tu column existed carries tu "", and pairs with this run's row for the same
    (file, func) when there is exactly one -- which is every body except the
    handful the tu column was added to disambiguate.  Then a body whose func
    name is unique on both sides is followed across a file move rather than
    counted as a deletion, because a header split is not a regression and must
    not read as one.
    """
    pairs, gone = [], []
    taken = set()
    byfunc, byfilefunc = {}, {}
    for key in idx:
        byfunc.setdefault(key[1], []).append(key)
        byfilefunc.setdefault((key[0], key[1]), []).append(key)
    for key in sorted(want):
        if key in idx:
            pairs.append((key, key))
            taken.add(key)
    for key in sorted(want):
        if key in idx:
            continue
        cand = []
        if not key[2]:
            cand = [k for k in byfilefunc.get((key[0], key[1]), [])
                    if k not in taken]
        if len(cand) != 1:
            cand = [k for k in byfunc.get(key[1], []) if k not in taken]
        if len(cand) == 1:
            pairs.append((key, cand[0]))
            taken.add(cand[0])
        else:
            gone.append(key)
    new = [k for k in sorted(idx) if k not in taken]
    return pairs, gone, new


def is_self_source(src):
    return src.replace("\\", "/").endswith("src/mcc.c")


def nofb_link_libs(bdir):
    p = os.path.join(bdir, "selfhost-link-libs.txt")
    if not os.path.exists(p):
        return ["-lm", "-ldl"]
    return [ln.strip() for ln in open(p) if ln.strip()]


def nofb_stage1(mcc, bdir, flags, opt, obj, exe, env, env0):
    """Compile src/mcc.c under `env` and link the result into a usable mcc."""
    r = subprocess.run([mcc] + list(flags) +
                       ["-" + opt, "-c", os.path.join(ROOT, "src", "mcc.c"),
                        "-o", obj],
                       cwd=ROOT, env=env, capture_output=True, text=True)
    if r.returncode != 0:
        return None
    blob = os.path.join(bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
    link_objs, link_flags = [], []
    if os.path.exists(blob):
        link_objs.append(blob)
    else:
        for p in (os.path.join(bdir, "libmccrt.a"),
                  os.path.join(bdir, "lib", "libmccrt.a")):
            if os.path.exists(p):
                link_flags += ["-B", os.path.dirname(p)]
                break
        else:
            return None
    if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in flags):
        jb = os.path.join(bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
        if not os.path.exists(jb):
            return None
        link_objs.append(jb)
    r = subprocess.run([mcc] + link_flags + [obj] + link_objs + ["-o", exe] +
                       nofb_link_libs(bdir),
                       cwd=ROOT, env=env0, capture_output=True, text=True)
    return exe if r.returncode == 0 else None


NOFB_WORK_N = 24


def nofb_work_sources():
    d = os.path.join(ROOT, "tests", "exec")
    out = []
    for dp, _, fns in os.walk(d):
        for fn in sorted(fns):
            if fn.endswith(".c") and fn != "runner.c":
                out.append(os.path.join(dp, fn))
    out.sort()
    if not out:
        return out
    return out[::max(1, len(out) // NOFB_WORK_N)]


def nofb_work(exe, flags, td, env0, work):
    """What the compiler under test DOES, as a comparable signature."""
    sig = []
    obj = os.path.join(td, "w.o")
    if os.path.exists(obj):
        os.remove(obj)
    try:
        r = subprocess.run([exe] + list(flags) +
                           ["-O2", "-c", os.path.join(ROOT, "src", "mcc.c"),
                            "-o", obj],
                           cwd=ROOT, env=env0, capture_output=True, timeout=900)
        h = ("-" if r.returncode or not os.path.exists(obj) else
             hashlib.sha256(open(obj, "rb").read()).hexdigest())
        sig.append(("compile src/mcc.c", r.returncode, h))
    except subprocess.TimeoutExpired:
        sig.append(("compile src/mcc.c", -1, "timeout"))
    for s in work:
        try:
            q = subprocess.run([exe, "-O1", "-run", s], cwd=ROOT, env=env0,
                               capture_output=True, timeout=120)
            sig.append((os.path.relpath(s, ROOT), q.returncode,
                        hashlib.sha256(q.stdout).hexdigest()))
        except subprocess.TimeoutExpired:
            sig.append((os.path.relpath(s, ROOT), -1, "timeout"))
    return sig


def nofb_probe_self(mcc, bdir, flags, opt, td, env0, verbose):
    """Benignity of the compiler's OWN byte-divergent bodies.

    src/mcc.c cannot be `-run`, so the behaviour compared is what the compiler
    built from it PRODUCES: a stage-1 mcc is linked with one divergent body's
    replay bytes kept and every other one still falling back, and it must agree
    with the baseline stage-1 on a fixed workload -- the object bytes of a
    stage-2 self-compile, plus the exit status and stdout of a stride sample of
    tests/exec run through it.

    The reference is the CONTROL, which runs the same -fno-replay-fallback but
    keeps nothing (skip = every divergent name), so a probe differs from it in
    exactly one body.  Whether the control also reproduces a plain build is
    reported, not assumed: MCC_FORCE_REPLAY is a no-op from -O1 up but turns
    replay on at -O0, so at -O0 the two are not the same compiler.  A probe that
    reproduces the control's object never kept its body at all and its verdict
    would be vacuous.
    """
    tsv = os.path.join(td, "self.tsv")
    if os.path.exists(tsv):
        os.remove(tsv)
    p = run_one(mcc, flags, os.path.join(ROOT, "src", "mcc.c"), opt,
                os.path.join(td, "self.o"), tsv, env0, "arena")
    if p.returncode != 0:
        return None, "src/mcc.c does not compile under MCC_RIR_PROD=2"
    names = [r[2] for r in read_rows(tsv)
             if r[0] == "fallback" and r[4] in DISCARD]
    if not names:
        return {"benign": 0, "miscompile": 0, "unrunnable": 0, "inert": 0,
                "bodies": 0, "miscompiles": []}, None
    work = nofb_work_sources()
    base_obj = os.path.join(td, "base.o")
    base_exe = os.path.join(bdir, "mcc-nofb-base")
    if not nofb_stage1(mcc, bdir, flags, opt, base_obj, base_exe, env0, env0):
        return None, "no baseline stage-1 (no runtime blob to link?)"
    plain_sig = nofb_work(base_exe, flags, td, env0, work)
    plain_bytes = open(base_obj, "rb").read()

    def probe_env(keep):
        env = dict(env0)
        env["MCC_FORCE_REPLAY"] = "1"
        skip = [x for x in names if x != keep]
        if skip:
            env["MCC_RIR_NOFB_SKIP"] = ",".join(skip)
        return env

    ctl_obj = os.path.join(td, "ctl.o")
    ctl_exe = os.path.join(bdir, "mcc-nofb-ctl")
    if not nofb_stage1(mcc, bdir, flags + ["-fno-replay-fallback"], opt, ctl_obj,
                       ctl_exe, probe_env(None), env0):
        return None, "the control stage-1 does not build or link"
    ref_bytes = open(ctl_obj, "rb").read()
    ref_sig = nofb_work(ctl_exe, flags, td, env0, work)
    ctl_drift = [a[0] for a, b in zip(ref_sig, plain_sig) if a != b]
    ctl_neutral = ref_bytes == plain_bytes and not ctl_drift

    benign, bad, inert = [], [], []
    obj = os.path.join(td, "probe.o")
    exe = os.path.join(bdir, "mcc-nofb-probe")
    for nm in names:
        if not nofb_stage1(mcc, bdir, flags + ["-fno-replay-fallback"], opt,
                           obj, exe, probe_env(nm), env0):
            bad.append((nm, "stage-1 does not build or link"))
            continue
        if open(obj, "rb").read() == ref_bytes:
            inert.append(nm)
            continue
        sig = nofb_work(exe, flags, td, env0, work)
        diff = [a[0] for a, b in zip(sig, ref_sig) if a != b]
        if diff:
            bad.append((nm, "%d of %d workload items differ, first %s"
                        % (len(diff), len(sig), diff[0])))
        else:
            benign.append(nm)
    for f in (base_exe, ctl_exe, exe):
        if os.path.exists(f):
            os.remove(f)
    if verbose:
        print("   self-host arm: %d divergent bodies in src/mcc.c; the control "
              "keeps none of them and %s a plain build%s"
              % (len(names), "reproduces" if ctl_neutral else "DOES NOT reproduce",
                 "" if ctl_neutral else
                 " (object %s, %d of %d workload items differ)"
                 % ("differs" if ref_bytes != plain_bytes else "matches",
                    len(ctl_drift), len(ref_sig))))
        print("   workload: stage-2 object bytes of src/mcc.c + %d tests/exec "
              "programs run through the stage-1" % len(work))
        if inert:
            print("   %d body(ies) left the stage-1 object unchanged, so their "
                  "verdict would be vacuous: %s"
                  % (len(inert), ", ".join(sorted(inert)[:8])))
    return {"benign": len(benign), "miscompile": len(bad), "unrunnable": 0,
            "inert": len(inert), "bodies": len(names),
            "control_neutral": ctl_neutral,
            "miscompiles": [("src/mcc.c", n) for n, _ in bad],
            "why": [(n, w) for n, w in bad]}, None


def nofb_probe(mcc, sources, opt, flags=(), bdir=None, verbose=True):
    """Per-body benignity split of category 2 (modelled, byte-divergent).

    For every body the byte compare discarded, keep THAT body's replay output
    (-fno-replay-fallback, with every other divergent body in the same TU still
    falling back via MCC_RIR_NOFB_SKIP) and compare the program's behaviour to
    the shipped compiler's. Same behaviour => that body's divergence is benign
    on this program; different => a real miscompile, e.g. union_cast::main.

    src/mcc.c is the compiler, not a program to run, and it is where the whole
    divergent population lives; it goes to nofb_probe_self above.
    """
    env0 = dict(os.environ)
    for k in ("MCC_RIR_PROD", "MCC_RIR_PROD_OUT", "MCC_FORCE_REPLAY",
              "MCC_REPLAY_IR", "MCC_REPLAY_IR_OUT", "MCC_RIR_NOFB_SKIP",
              "MCC_TEST_OPT"):
        env0.pop(k, None)
    benign, bad, unrunnable, nbodies, inert = [], [], [], 0, 0
    ctl = None
    if verbose:
        print("== -%s  category-2 benignity probe (-fno-replay-fallback, "
              "per body)" % opt)
    with tempfile.TemporaryDirectory() as td:
        for src in sources:
            if is_self_source(src):
                if bdir is None:
                    continue
                r, why = nofb_probe_self(mcc, bdir, list(flags), opt, td, env0,
                                         verbose)
                if r is None:
                    if verbose:
                        print("   self-host arm SKIPPED: %s" % why)
                    continue
                nbodies += r["bodies"]
                inert += r["inert"]
                ctl = r.get("control_neutral", ctl)
                benign += [(src, "?")] * r["benign"]
                bad += [(src, n, w) for n, w in r.get("why", [])]
                continue
            tsv = os.path.join(td, "p.tsv")
            if os.path.exists(tsv):
                os.remove(tsv)
            p = run_one(mcc, [], src, opt, os.path.join(td, "p.o"), tsv, env0,
                        "arena")
            if p.returncode != 0:
                continue
            rows = [r for r in read_rows(tsv)
                    if r[0] == "fallback" and r[4] in DISCARD]
            if not rows:
                continue
            names = [r[2] for r in rows]
            nbodies += len(names)
            base = subprocess.run([mcc, "-" + opt, "-run", src],
                                  capture_output=True, text=True, cwd=ROOT,
                                  env=env0, timeout=60)
            if base.returncode != 0:
                unrunnable.append((src, names))
                continue
            for nm in names:
                env = dict(env0)
                env["MCC_FORCE_REPLAY"] = "1"
                skip = [x for x in names if x != nm]
                if skip:
                    env["MCC_RIR_NOFB_SKIP"] = ",".join(skip)
                try:
                    q = subprocess.run(
                        [mcc, "-" + opt, "-fno-replay-fallback", "-run", src],
                        capture_output=True, text=True, cwd=ROOT, env=env,
                        timeout=60)
                except subprocess.TimeoutExpired:
                    bad.append((src, nm, "timeout"))
                    continue
                if q.returncode == base.returncode and q.stdout == base.stdout:
                    benign.append((src, nm))
                else:
                    bad.append((src, nm, "rc %d vs %d" % (q.returncode,
                                                          base.returncode)))
    if verbose:
        print("   %d divergent bodies in %d programs: %d benign, "
              "%d MISCOMPILE, %d vacuous, %d in programs that do not run "
              "standalone"
              % (nbodies, len(set(s for s, _ in benign)) +
                 len(set(s for s, _, _ in bad)), len(benign), len(bad), inert,
                 sum(len(n) for _, n in unrunnable)))
        for src, nm, why in bad:
            print("   MISCOMPILE %s::%s (%s)" % (os.path.relpath(src, ROOT), nm,
                                                 why))
    return {"benign": len(benign), "miscompile": len(bad),
            "unrunnable": sum(len(n) for _, n in unrunnable),
            "inert": inert, "bodies": nbodies, "control_neutral": ctl,
            "miscompiles": [(os.path.relpath(s, ROOT), n) for s, n, _ in bad]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir", nargs="?")
    ap.add_argument("--levels", default="O0,O1,O2,O3")
    ap.add_argument("--corpus", default="self")
    ap.add_argument("--layers", default="both",
                    choices=["both", "arena", "capture"])
    ap.add_argument("--bank", default=DEFAULT_BANK)
    ap.add_argument("--update-bank", action="store_true")
    ap.add_argument("--update-bank-low", action="store_true")
    ap.add_argument("--update-bodies", action="store_true")
    ap.add_argument("--check-low-dir", action="store_true")
    ap.add_argument("--no-check", action="store_true")
    ap.add_argument("--top", type=int, default=0)
    ap.add_argument("--tol", type=float, default=0.05)
    ap.add_argument("--json", default=None)
    ap.add_argument("--sources", nargs="*", default=None)
    ap.add_argument("--nofb-probe", action="store_true")
    ap.add_argument("--classify", action="store_true")
    ap.add_argument("--check-gap-dir", action="store_true")
    ap.add_argument("--opt-in", action="store_true")
    ap.add_argument("--rebank-config", action="store_true")
    ap.add_argument("--selfcheck-bankkeying", action="store_true")
    ap.add_argument("--selfcheck-corpus-guard", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    a = ap.parse_args()

    if a.selfcheck_bankkeying:
        return selfcheck_bankkeying(a.mutate)

    if a.selfcheck_corpus_guard:
        return selfcheck_corpus_guard(a.mutate)

    if not a.build_dir:
        ap.error("build_dir is required")

    if a.opt_in and not os.environ.get("MCC_RIR_CENSUS"):
        print("rir-coverage: set MCC_RIR_CENSUS=1 to run this census "
              "(ctest -L census)")
        return 77

    bdir = a.build_dir if os.path.isabs(a.build_dir) else os.path.join(ROOT, a.build_dir)
    mcc = find_mcc(bdir)
    flags = self_flags(bdir)

    if a.sources:
        sources = a.sources
    elif a.corpus == "self":
        sources = [os.path.join(ROOT, "src", "mcc.c")]
    elif a.corpus in ("wide", "exec"):
        sources = [] if a.corpus == "exec" else [os.path.join(ROOT, "src", "mcc.c")]
        for d in (("tests/exec",) if a.corpus == "exec" else
                  ("tests/exec", "tests/behavior", "tests/ast", "tests/asm",
                   "tests/runtime", "tests/static", "examples")):
            p = os.path.join(ROOT, d)
            if not os.path.isdir(p):
                continue
            for dp, _, fns in os.walk(p):
                for fn in sorted(fns):
                    if fn.endswith(".c"):
                        sources.append(os.path.join(dp, fn))
    else:
        sys.exit("rir-coverage: unknown corpus " + a.corpus)

    if a.sources:
        corpus_seen = "explicit --sources"
    elif a.corpus == "self":
        corpus_seen = "self = src/mcc.c"
    elif a.corpus == "exec":
        corpus_seen = "exec = tests/exec/**.c"
    else:
        corpus_seen = ("wide[rir-coverage] = src/mcc.c + "
                       "tests/{exec,behavior,ast,asm,runtime,static} + examples, "
                       "unfiltered")

    bank = {}
    if os.path.exists(a.bank):
        bank = json.load(open(a.bank))
    banked = bank.get(a.corpus, {})

    result = {}
    bad = []
    if a.check_gap_dir:
        env0 = dict(os.environ)
        for k in ("MCC_RIR_PROD", "MCC_RIR_PROD_OUT", "MCC_FORCE_REPLAY",
                  "MCC_REPLAY_IR", "MCC_TEST_OPT"):
            env0.pop(k, None)
        gdir = os.path.join(ROOT, "tests", "rir", "gap")
        pairs_run = 0
        pairs_expected = 0
        covered = []
        with tempfile.TemporaryDirectory() as td:
            for fn in sorted(os.listdir(gdir)):
                if not fn.endswith(".c"):
                    continue
                want = fn[:-2]
                if want not in UNF:
                    want = "skip:" + want
                only = gap_levels(os.path.join(gdir, fn))
                for opt in a.levels.split(","):
                    if only and opt not in only:
                        continue
                    pairs_expected += 1
                    tsv = os.path.join(td, "g.tsv")
                    if os.path.exists(tsv):
                        os.remove(tsv)
                    p = run_one(mcc, [], os.path.join(gdir, fn), opt,
                                os.path.join(td, "g.o"), tsv, env0, "arena")
                    if p.returncode != 0:
                        bad.append("%s -%s: compile failed" % (fn, opt))
                        continue
                    got = set()
                    for r in read_rows(tsv):
                        got.add("used" if r[0] == "used" else
                                r[4] if r[0] == "fallback" else "skip:" + r[3])
                    ok = want in got
                    pairs_run += 1
                    if ok:
                        covered.append(want)
                    print("%-14s -%-3s %-16s %s"
                          % (fn, opt, want, "ok" if ok else
                             "MISSING, got " + ",".join(sorted(got))))
                    if not ok:
                        bad.append("%s -%s: class %s no longer reproduces (got %s)"
                                   % (fn, opt, want, ",".join(sorted(got))))
        if not pairs_run:
            bad.append("check-gap-dir ran no fixture/level pair at all; an empty "
                       "tests/rir/gap or a level filter that excludes every "
                       "--levels entry makes this mode pass by measuring nothing")
        elif pairs_run != pairs_expected:
            bad.append("check-gap-dir ran %d fixture/level pair(s), expected %d "
                       "from the directory listing crossed with --levels"
                       % (pairs_run, pairs_expected))
        else:
            print("gap floor: %d fixture/level pair(s) run, %d class(es) of %d "
                  "covered" % (pairs_run, len(set(covered)), len(UNF) + len(WHY)))
        for m in bad:
            print("FAIL " + m)
        return 1 if bad else 0
    if a.check_low_dir:
        ldir = os.path.join(ROOT, "tests", "rir", "low")
        low_run = 0
        low_expected = 0
        low_seen = set()
        for fn in sorted(os.listdir(ldir)):
            if not fn.endswith(".c"):
                continue
            want = fn[:-2]
            for opt in a.levels.split(","):
                low_expected += 1
                c = census(mcc, [], [os.path.join(ldir, fn)], opt,
                           keep_rows=False)
                if c["failed"]:
                    bad.append("%s -%s: compile failed" % (fn, opt))
                    continue
                got = sorted(k for k, v in c["low"].items() if v[0])
                if want == "ok":
                    ok = c["low_bodies"] and c["low_ok1"] == c["low_bodies"]
                    print("%-12s -%-3s %-8s %s (ok1=%d/%d)"
                          % (fn, opt, want, "ok" if ok else "REGRESSED",
                             c["low_ok1"], c["low_bodies"]))
                else:
                    ok = want in got
                    print("%-12s -%-3s %-8s %s   [blockers %s]"
                          % (fn, opt, want, "ok" if ok else "MISSING",
                             ",".join(got) or "-"))
                low_run += 1
                if want != "ok":
                    low_seen.add(want)
                if not ok:
                    bad.append("%s -%s: lowerable class %s no longer "
                               "reproduces" % (fn, opt, want))
        if not low_run:
            bad.append("check-low-dir ran no fixture/level pair at all; an empty "
                       "tests/rir/low makes this mode pass by measuring nothing")
        elif low_run != low_expected:
            bad.append("check-low-dir ran %d fixture/level pair(s), expected %d"
                       % (low_run, low_expected))
        else:
            missing = sorted(set(LOWCLS) - low_seen)
            print("low floor: %d fixture/level pair(s) run, %d blocker class(es) "
                  "of %d covered%s"
                  % (low_run, len(low_seen), len(LOWCLS),
                     "; no fixture for " + ",".join(missing) if missing else ""))
        for m in bad:
            print("FAIL " + m)
        return 1 if bad else 0
    if a.classify:
        env0 = dict(os.environ)
        for k in ("MCC_RIR_PROD", "MCC_RIR_PROD_OUT", "MCC_FORCE_REPLAY",
                  "MCC_REPLAY_IR", "MCC_TEST_OPT"):
            env0.pop(k, None)
        with tempfile.TemporaryDirectory() as td:
            for opt in a.levels.split(","):
                for src in sources:
                    tsv = os.path.join(td, "c.tsv")
                    if os.path.exists(tsv):
                        os.remove(tsv)
                    p = run_one(mcc, [], src, opt, os.path.join(td, "c.o"), tsv,
                                env0, "arena")
                    if p.returncode != 0:
                        print("-%s %-40s COMPILE FAILED" % (opt, os.path.basename(src)))
                        continue
                    for r in read_rows(tsv):
                        cls = ("used" if r[0] == "used" else
                               r[4] if r[0] == "fallback" else "skip:" + r[3])
                        print("-%s %-28s %-24s %-14s %6d B raw=%d"
                              % (opt, os.path.basename(src), r[2], cls, r[5], r[6]))
        return 0
    if a.nofb_probe:
        known = bank.get("nofb_miscompiles", {})
        for opt in a.levels.split(","):
            r = nofb_probe(mcc, sources, opt, flags=flags, bdir=bdir)
            result.setdefault(opt, {})["nofb_probe"] = r
            got = sorted("%s::%s" % (f, n) for f, n in r["miscompiles"])
            was = sorted(known.get(opt, []))
            if a.update_bank:
                mine = set(os.path.relpath(s, ROOT) for s in sources)
                known[opt] = sorted(set(got) |
                                    set(m for m in was
                                        if m.rsplit("::", 1)[0] not in mine))
            elif not a.no_check:
                for m in got:
                    if m not in was:
                        bad.append("-%s: new byte-divergent body miscompiles "
                                   "under -fno-replay-fallback: %s" % (opt, m))
        if a.update_bank:
            bank["nofb_miscompiles"] = known
            json.dump(bank, open(a.bank, "w"), indent=1, sort_keys=True)
            print("banked nofb_miscompiles -> %s" % a.bank)
        if a.json:
            json.dump(result, open(a.json, "w"), indent=1, sort_keys=True)
        for m in bad:
            print("FAIL " + m)
        return 1 if bad else 0
    src_rel = sorted(os.path.relpath(x, ROOT).replace("\\", "/") for x in sources)
    src_manifest = {"n": len(src_rel),
                    "sha": hashlib.sha256("\n".join(src_rel).encode()).hexdigest()[:16]}
    _lx = check_low_exclude(ROOT)
    if _lx and not a.no_check:
        bad.append(_lx)
    src_key = "sources_" + a.corpus
    if a.update_bank:
        bank[src_key] = src_manifest
    elif not a.no_check and src_key in bank:
        wantm = bank[src_key]
        if wantm.get("sha") != src_manifest["sha"]:
            bad.append("corpus %s drifted: banked %d file(s) sha %s, this run walked "
                       "%d sha %s -- the denominator every percentage below is over "
                       "is not the one that was banked; re-bank deliberately or find "
                       "what moved"
                       % (a.corpus, wantm.get("n", -1), wantm.get("sha", "?"),
                          src_manifest["n"], src_manifest["sha"]))
    self_corpus = any(s.replace("\\", "/").endswith("src/mcc.c") for s in sources)
    want_cfg = bank.get("corpus_config")
    have_cfg = corpus_config(flags)
    if self_corpus and want_cfg is not None and have_cfg != want_cfg:
        if a.rebank_config and (a.update_bank or a.update_bank_low):
            bank["corpus_config"] = have_cfg
        else:
            print("rir-coverage: SKIP: the %s corpus is the compiler's own source "
                  "and this build is not the configuration the bank was taken on "
                  "(bank %s, this build %s)." % (a.corpus, fmt_config(want_cfg),
                                                 fmt_config(have_cfg)))
            print("rir-coverage: every banked percentage is a ratio over src/, so "
                  "a configuration that compiles extra source into src/mcc.c "
                  "dilutes all of them; the movement is configuration, not "
                  "regression. Run the ratchet on a default build, and do NOT "
                  "re-bank here -- see docs/TODO.md, 'The lowerable ratchet is "
                  "self-referential'.")
            return 77

    fmt = host_objfmt(mcc) if self_corpus else None
    unbanked_host = False
    if (self_corpus and fmt and not a.update_bank and not a.update_bank_low
            and not a.no_check):
        levels = a.levels.split(",")
        anylow = any(banked.get(o, {}).get("lowerable") for o in levels)
        havefmt = any(low_floor(banked.get(o, {}).get("lowerable"), fmt)
                      for o in levels)
        unbanked_host = bool(anylow and not havefmt)
    if unbanked_host:
        print("rir-coverage: PARTIAL SKIP: the %s corpus has no banked lowerable "
              "floors for the %s host, which is this tool's only marker that the "
              "bank was taken somewhere else.  The host-sensitive comparisons "
              "are therefore not made.  Skipped, and why:" % (a.corpus, fmt))
        print("  lowerable ratchet -- MCC_HOST_* decides which source "
              "amalgamates into the corpus, so the elf/pe floors are ratios over "
              "different source than this host compiles.")
        print("  arena .text residual -- host-specific and NOT per-format in the "
              "schema: src/mccrun.c's mcc_tlv_thunk is 120 bytes of raw asm with "
              "no C body, so Darwin's residual is legitimately 120 against an "
              "ELF-banked 0.  Comparing it would fail a correct number.")
        print("  arena kept_coverage -- host-sensitive, but by much less than "
              "this tool once claimed, and twice what was blamed on the host "
              "turned out to be a stale bank.  The old 96.156-vs-83.219 figure "
              "was an artifact: 96.156 was banked at 879bf988, BEFORE 1ad3f1aa "
              "moved eleven passes across the -O ladder and took elf/x86-64 "
              "kept to 82.520, so ~12.2 points of bank staleness were being "
              "attributed to the host.  wide's 93.4-vs-98.4 was the same thing "
              "again: elf/x86-64 measured 92.92-93.01 against that bank, so "
              "darwin agreed with Linux and the bank predated the chain-store "
              "demotion.  The arena builder's unconditional chained-store "
              "collapse is fixed and both banks are refreshed (wide 92.92 / "
              "96.60 / 96.65 / 96.65 on elf), so the residual host spread is "
              "small on both corpora.  They stay skipped here only because this "
              "partial skip is keyed on the lowerable floors; arming "
              "kept_coverage per host needs a per-format schema.")
        print("  Still enforced here: capture byte coverage, arena modelled "
              "coverage, and the census self-reconciliation.  Those are "
              "near-saturated ratios that do carry across hosts -- measured: "
              "the wide corpus reports capture 100.000% and modelled 100.000% "
              "on darwin/aarch64, the same values banked from elf/x86-64.")
        print("  To gate the rest on this host, its floors have to be banked for "
              "it (--update-bank-low for lowerable; residual and kept_coverage "
              "need a per-format schema first); or run the whole ratchet on a "
              "banked host (elf, pe).")

    inv_levels, inv_table, inv_dropped = read_low_inventory(
        low_inventory_path(a.bank))
    if inv_dropped:
        print("rir-coverage: %s has %d unparsable row(s); they are NOT bodies "
              "this run will gate" % (low_inventory_path(a.bank), inv_dropped))
    low_index = {}
    checked, skipped = [], []
    for opt in a.levels.split(","):
        if a.layers != "arena":
            cc2 = census(mcc, flags, sources, opt, layer="capture",
                         keep_rows=False)
            ctext = cc2["text"]
            cfaith = cc2["cap_b_faith"]
            cfn = cc2["fn_bytes"] + cc2["b_reemit"]
            cbody = cfaith + cc2["cap_b_unfaith"] + cc2["cap_b_err"]
            ccov = pct(cfaith, cbody)
            if (not a.update_bank and not a.no_check and not cc2["failed"]
                    and cc2["cap_fn"] > 0 and cbody == 0):
                print("SKIP: target emits no per-body byte accounting (e.g. PE); "
                      "rir byte-coverage is unmeasurable here")
                return 77
            result.setdefault(opt, {})["capture"] = {
                "fn": cc2["cap_fn"], "faithful_bodies": cc2["cap_faithful"],
                "text": ctext, "fn_bytes": cfn, "body_bytes": cbody,
                "faithful_bytes": cfaith, "unfaithful_bytes": cc2["cap_b_unfaith"],
                "rerror_bytes": cc2["cap_b_err"], "rerror_bodies": cc2["cap_n_err"],
                "raw_fn": cc2["cap_raw_fn"], "raw_bytes": cc2["cap_raw_b"],
                "unnoted": cc2["unnoted"], "unnoted_bytes": cc2["b_unnoted"],
                "residual": ctext - cfn, "coverage": round(ccov, 4),
            }
            print("== -%s  CAPTURE layer (op stream, MCC_REPLAY_IR=3)  "
                  "corpus=%s files=%d" % (opt, corpus_seen, len(sources)))
            print("   bodies %d, rfaithful %d, rerror %d, unnoted fns %d"
                  % (cc2["cap_fn"], cc2["cap_faithful"], cc2["cap_n_err"],
                     cc2["unnoted"]))
            print("   .text %d = fn %d (incl reemit %d) + residual %d;  "
                  "body %d + pro/epilogue %d"
                  % (ctext, cfn, cc2["b_reemit"], ctext - cfn, cbody,
                     cfn - cbody - cc2["b_reemit"]))
            print("   BYTE COVERAGE %.3f%% of body bytes  (%.3f%% of .text); "
                  "unfaithful %d B, rerror %d B"
                  % (ccov, pct(cfaith, ctext), cc2["cap_b_unfaith"],
                     cc2["cap_b_err"]))
            print("   IR_OP_RAW escape hatch used by %d bodies / %d bytes "
                  "(%.3f%% of .text)"
                  % (cc2["cap_raw_fn"], cc2["cap_raw_b"],
                     pct(cc2["cap_raw_b"], ctext)))
            if cc2["failed"]:
                print("   %d source(s) failed to compile" % len(cc2["failed"]))
            b = banked.get(opt, {}).get("capture")
            if b and not a.no_check:
                checked.append("-%s capture coverage" % opt)
                if ccov + a.tol < b["coverage"]:
                    bad.append("-%s capture: byte coverage regressed: %.4f%% < "
                               "banked %.4f%%" % (opt, ccov, b["coverage"]))
        if a.layers == "capture":
            continue
        c = census(mcc, flags, sources, opt)
        text = c["text"]
        body = c["b_body"]
        used = c["b_used"]
        cov = pct(used, text)
        resid = text - c["fn_bytes"] - c["b_reemit"]
        recon_bodies = c["b_used"] + c["b_fallback"] + c["b_skip"]
        unf_sum = sum(v[0] for v in c["unf"].values())
        why_sum = sum(v[0] for v in c["why"].values())
        abort_n, abort_b = c["unf"].get("abort", [0, 0])
        disc_b = c["b_fallback"] - abort_b
        disc_n = c["fallback"] - abort_n
        gap_b = c["b_skip"] + abort_b
        gap_n = c["skip"] + abort_n
        modelled = used + disc_b
        result.setdefault(opt, {})["arena"] = {
            "bodies": c["used"] + c["fallback"] + c["skip"],
            "used": c["used"], "fallback": c["fallback"], "skip": c["skip"],
            "text": text, "fn_bytes": c["fn_bytes"], "body_bytes": body,
            "used_bytes": used, "fallback_bytes": c["b_fallback"],
            "skip_bytes": c["b_skip"], "nonbody_bytes": c["b_nonbody"],
            "unnoted": c["unnoted"], "unnoted_bytes": c["b_unnoted"],
            "residual": resid, "coverage": round(cov, 4),
            "discarded_bodies": disc_n, "discarded_bytes": disc_b,
            "gap_bodies": gap_n, "gap_bytes": gap_b,
            "modelled_bytes": modelled,
            "modelled_coverage": round(pct(modelled, body), 4),
            "kept_coverage": round(pct(used, body), 4),
            "unf": c["unf"], "why": c["why"],
            "classes": classify(c["rows"]) if c["rows"] else {},
            "failed": len(c["failed"]),
        }
        low = lowerable(c)
        result[opt]["lowerable"] = low
        print("== -%s  ARENA layer (production, MCC_RIR_PROD=2)  corpus=%s "
              "files=%d" % (opt, corpus_seen, len(sources)))
        print("   bodies %d = used %d + fallback %d + skip %d"
              % (result[opt]["arena"]["bodies"], c["used"], c["fallback"],
                 c["skip"]))
        print("   .text %d = fn %d + reemit %d (%d fns) + residual %d"
              % (text, c["fn_bytes"], c["b_reemit"], c["reemit"], resid))
        print("   fn    %d = body %d + prologue/epilogue %d"
              % (c["fn_bytes"], body, c["b_nonbody"]))
        print("   body  %d = used %d + fallback %d + skip %d  (delta %d)"
              % (body, used, c["b_fallback"], c["b_skip"], body - recon_bodies))
        print("   MODELLED %.3f%% of body bytes  (kept %.3f%% + discarded by "
              "byte compare %.3f%%)"
              % (pct(modelled, body), pct(used, body), pct(disc_b, body)))
        print("   GAP      %.3f%% of body bytes = %d B  "
              "(never modelled %d B + replay aborted %d B, %d bodies)"
              % (pct(gap_b, body), gap_b, c["b_skip"], abort_b, gap_n))
        print("   as %% of .text: modelled %.3f%%  kept %.3f%%  gap %.3f%%  "
              "prologue/epilogue %.3f%%"
              % (pct(modelled, text), cov, pct(gap_b, text),
                 pct(c["b_nonbody"], text)))
        print("   unnoted %d bodies/%d bytes" % (c["unnoted"], c["b_unnoted"]))
        for name in UNF:
            if name in c["unf"]:
                n, b = c["unf"][name]
                tag = "GAP " if name == "abort" else "disc"
                print("     %s fallback/%-11s %5d bodies %9d bytes  %6.3f%% of body"
                      % (tag, name, n, b, pct(b, body)))
        for name in WHY:
            if name in c["why"]:
                n, b = c["why"][name]
                print("     GAP  skip/%-15s %5d bodies %9d bytes  %6.3f%% of body"
                      % (name, n, b, pct(b, body)))
        if unf_sum != c["fallback"] or why_sum != c["skip"]:
            bad.append("-%s: census does not reconcile: unfaithful sum %d vs "
                       "fallback %d, why sum %d vs skip %d"
                       % (opt, unf_sum, c["fallback"], why_sum, c["skip"]))
        if body != recon_bodies:
            bad.append("-%s: body bytes %d != used+fallback+skip %d"
                       % (opt, body, recon_bodies))
        print("   IR_OP_RAW bodies: used %d/%dB  fallback %d/%dB  skip %d/%dB"
              % (c["raw_used"], c["b_raw_used"], c["raw_fallback"],
                 c["b_raw_fallback"], c["raw_skip"], c["b_raw_skip"]))
        print_lowerable(low)
        lost = c["b_fallback"] + c["b_skip"]
        rawlost = c["b_raw_fallback"] + c["b_raw_skip"]
        print("   of the %d lost bytes, %d (%.2f%%) are in bodies that used "
              "IR_OP_RAW" % (lost, rawlost, pct(rawlost, lost)))
        if c["failed"]:
            print("   %d source(s) failed to compile" % len(c["failed"]))
        if a.top:
            cls = result[opt]["arena"]["classes"]
            for cname in sorted(cls, key=lambda k: -cls[k][1]):
                n, b, big, rawn, rawb = cls[cname]
                print("   class %-18s %5d bodies %9d bytes  raw %d/%dB  "
                      "biggest %s:%s (%d)"
                      % (cname, n, b, rawn, rawb, os.path.basename(big[0]),
                         big[1], big[2]))
                rr = [r for r in c["rows"]
                      if (r[4] if r[0] == "fallback" else "skip:" + r[3]) == cname]
                rr.sort(key=lambda r: -r[5])
                for r in rr[:a.top]:
                    print("       %8d raw=%d %s:%s"
                          % (r[5], r[6], os.path.basename(r[1]), r[2]))
        b = banked.get(opt, {}).get("arena")
        if b and not a.no_check:
            mc = pct(modelled, body)
            checked.append("-%s modelled_coverage" % opt)
            if mc + a.tol < b["modelled_coverage"]:
                bad.append("-%s: modelled coverage regressed: %.4f%% < banked "
                           "%.4f%% (the gap grew)"
                           % (opt, mc, b["modelled_coverage"]))
            kc = pct(used, body)
            kc_floor = arena_floor(b, "kept_coverage", fmt)
            if kc_floor is None:
                skipped.append("-%s arena kept_coverage (this host %.4f%%, no "
                               "%s floor banked)" % (opt, kc, fmt))
            else:
                checked.append("-%s kept_coverage" % opt)
                if kc + a.tol < kc_floor:
                    bad.append("-%s: kept coverage regressed: %.4f%% < banked "
                               "%.4f%% (fewer body bytes ship optimized)"
                               % (opt, kc, kc_floor))
            # Sweep row 22's other half. Every percentage above is computed
            # over the sources that HAPPENED TO COMPILE, and the count of the
            # ones that did not was printed and never compared -- so a source
            # dropping out shrank the denominator silently and the ratchet went
            # on passing over a smaller corpus. Bank it and fail when it rises.
            # Read per-format, for the same reason residual and kept_coverage
            # are: which sources compile is a property of the host, not of the
            # compiler.  The arch-gated files inverted between the two hosts --
            # arch/arm64_*.c and winarm64_interlocked.c fail on x86_64-linux and
            # compile here, while the four inline_asm/asm_*_x86 files and
            # fastcall.c do the reverse -- so elf's 9 and macho's 17 are both
            # correct and neither is a regression against the other.  A flat
            # legacy value reads as the elf floor and leaves any other host
            # skipping with a reason rather than failing on a number that was
            # never about it.
            nfail = len(c["failed"])
            fail_floor = arena_floor(b, "failed", fmt)
            if fail_floor is None:
                if "failed" in b:
                    skipped.append("-%s compile failures (this host %d, no %s "
                                   "floor banked -- which sources compile is a "
                                   "host property, so the banked count is not "
                                   "this host's)" % (opt, nfail, fmt))
            else:
                checked.append("-%s compile failures" % opt)
                if nfail > fail_floor:
                    names = ", ".join(
                        os.path.basename(f if isinstance(f, str) else f[0])
                        for f in c["failed"][:6])
                    bad.append("-%s: %d source(s) now fail to compile against "
                               "%d banked, so every percentage above is over a "
                               "denominator that just shrank: %s%s"
                               % (opt, nfail, fail_floor, names,
                                  " ..." if nfail > 6 else ""))
            resid_floor = arena_floor(b, "residual", fmt)
            if resid_floor is None:
                skipped.append("-%s arena residual (this host %d, no %s floor "
                               "banked)" % (opt, resid, fmt))
            else:
                checked.append("-%s residual" % opt)
                if abs(resid) > abs(resid_floor):
                    bad.append("-%s: unattributed .text grew: residual %d, "
                               "banked %d" % (opt, resid, resid_floor))
        elif not b and not a.update_bank and not a.no_check:
            bad.append("-%s: no banked coverage for corpus %s" % (opt, a.corpus))
        lb = low_floor(banked.get(opt, {}).get("lowerable"), fmt)
        idx_why = []
        idx = low_body_index(c, idx_why)
        low_index[opt] = idx
        want = {}
        if idx is not None and opt in inv_levels:
            col = inv_levels.index(opt)
            for key, mask in inv_table.get((a.corpus, fmt), {}).items():
                if col < len(mask) and mask[col] != "-":
                    want[key] = int(mask[col], 16)
        if lb and want:
            pairs, gone, fresh = low_inventory_match(want, idx)
            kept = [idx[cur] for _, cur in pairs]
            cur_old = low_body_pcts(kept)
            newrows = [idx[k] for k in fresh]
            print("     BANK DRIFT: %d banked bodies, %d present, %d gone, "
                  "%d new (%d nodes, %d lowerable = %.2f%% against %.2f%% "
                  "banked corpus-wide)"
                  % (len(want), len(pairs), len(gone), len(fresh),
                     sum(r[LB_NODES] for r in newrows),
                     sum(1 for r in newrows if low_body_mask(r) & 2),
                     pct(sum(1 for r in newrows if low_body_mask(r) & 2),
                         len(newrows)), lb.get("bodies_pct", 0.0)))
            for k in LOW_BANKED:
                print("       %-21s banked %8.4f  now %8.4f  = pre-existing "
                      "%+7.4fpp + corpus mix %+7.4fpp"
                      % (k, lb.get(k, 0.0), low[k], cur_old[k] - lb.get(k, 0.0),
                         low[k] - cur_old[k]))
            if gone:
                print("       %d banked body(ies) no longer in the corpus: %s"
                      % (len(gone), ", ".join(low_body_name(g)
                                              for g in gone[:8])))
        if lb and not a.no_check:
            checked.append("-%s lowerable[%s]%s"
                           % (opt, fmt, "" if want else " (corpus-wide only)"))
            if want:
                lost = ["%s -> %s" % (low_body_name(b), low_body_name(cur))
                        if b != cur else low_body_name(b)
                        for b, cur in pairs
                        if want[b] & 2 and not (low_body_mask(idx[cur]) & 2)]
                if lost:
                    bad.append("-%s lowerable[%s]: %d body(ies) banked lowerable "
                               "are NOT lowerable now: %s"
                               % (opt, fmt, len(lost), ", ".join(lost[:8])))
                for k in LOW_BANKED:
                    if cur_old[k] + a.tol >= lb.get(k, 0.0):
                        continue
                    bad.append(
                        "-%s lowerable[%s]: %s regressed ON PRE-EXISTING BODIES: "
                        "%.4f%% < banked %.4f%% over the %d banked bodies still "
                        "in the corpus (%d gone).  This is NOT dilution: the %d "
                        "bodies added since the bank are excluded from both "
                        "sides of this comparison.  Corpus-wide the figure is "
                        "%.4f%%."
                        % (opt, fmt, k, cur_old[k], lb[k], len(pairs), len(gone),
                           len(fresh), low[k]))
            else:
                why = (idx_why[0] if idx is None else
                       "no banked body inventory for %s/%s at -%s; take one "
                       "with --update-bodies, which moves no floor"
                       % (a.corpus, fmt, opt))
                for k in LOW_BANKED:
                    if low[k] + a.tol < lb.get(k, 0.0):
                        bad.append(
                            "-%s lowerable[%s]: %s regressed: %.4f%% < banked "
                            "%.4f%% over the WHOLE corpus, which also falls when "
                            "new source is less lowerable than the source it "
                            "joined.  Attribution is unavailable (%s), so this "
                            "number cannot tell dilution from a real loss; see "
                            "%s" % (opt, fmt, k, low[k], lb[k], why,
                                    low_inventory_path(a.bank)))
        elif unbanked_host:
            skipped.append("-%s lowerable (no %s floor banked)" % (opt, fmt))
        elif (not lb and not a.update_bank and not a.update_bank_low
              and not a.no_check and low["bodies"]):
            bad.append("-%s: no banked lowerable census for corpus %s (%s host)"
                       % (opt, a.corpus, fmt))

    if a.json:
        json.dump(result, open(a.json, "w"), indent=1, sort_keys=True)

    if a.update_bodies:
        update_low_inventory(a, fmt, low_index)
    if a.update_bank_low:
        prev = bank.get(a.corpus, {})
        for opt, layers in result.items():
            if "lowerable" not in layers:
                continue
            v = layers["lowerable"]
            e = {k: v[k] for k in LOW_BANKED}
            e["blockers"] = {n: v["blockers"][n]["sole_bytes_pct"]
                             for n in LOWCLS}
            slot = prev.setdefault(opt, {}).get("lowerable")
            if slot is None or "bodies_pct" in slot:
                slot = {"elf": slot} if slot else {}
                prev[opt]["lowerable"] = slot
            slot[fmt] = e
        bank[a.corpus] = prev
        bank.setdefault("corpus_config", have_cfg)
        os.makedirs(os.path.dirname(a.bank), exist_ok=True)
        json.dump(bank, open(a.bank, "w"), indent=1, sort_keys=True)
        print("banked lowerable/%s -> %s" % (a.corpus, a.bank))
        update_low_inventory(a, fmt, low_index)
    if a.update_bank:
        out = {}
        for opt, layers in result.items():
            out[opt] = {}
            for lname, v in layers.items():
                if lname == "lowerable":
                    e = {k: v[k] for k in LOW_BANKED}
                    e["blockers"] = {
                        n: v["blockers"][n]["sole_bytes_pct"] for n in LOWCLS}
                    out[opt][lname] = {fmt: e}
                    continue
                out[opt][lname] = {"coverage": v["coverage"],
                                   "residual": v["residual"],
                                   "text": v["text"]}
                if lname == "arena":
                    out[opt][lname].update(
                        {"bodies": v["bodies"], "used_bytes": v["used_bytes"],
                         "body_bytes": v["body_bytes"],
                         "modelled_coverage": v["modelled_coverage"],
                         "kept_coverage": v["kept_coverage"],
                         "modelled_bytes": v["modelled_bytes"],
                         "gap_bytes": v["gap_bytes"],
                         "gap_bodies": v["gap_bodies"],
                         "failed": v["failed"],
                         "discarded_bytes": v["discarded_bytes"]})
                else:
                    out[opt][lname].update(
                        {"bodies": v["fn"], "faithful_bytes": v["faithful_bytes"]})
        bank[a.corpus] = merge_bank(bank.get(a.corpus, {}), out, fmt)
        bank.setdefault("corpus_config", have_cfg)
        os.makedirs(os.path.dirname(a.bank), exist_ok=True)
        json.dump(bank, open(a.bank, "w"), indent=1, sort_keys=True)
        print("banked %s -> %s" % (a.corpus, a.bank))
        update_low_inventory(a, fmt, low_index)

    for m in bad:
        print("FAIL " + m)
    if skipped:
        print("rir-coverage: %d comparison(s) skipped on this %s host:"
              % (len(skipped), fmt))
        for m in skipped:
            print("  SKIP " + m)
    if bad:
        return 1
    if skipped and not checked:
        print("rir-coverage: SKIP: every comparison this run could make is "
              "host-format-derived and unbanked for %s; nothing was gated."
              % fmt)
        return 77
    if checked:
        print("rir-coverage: PASS: %d comparison(s) enforced%s."
              % (len(checked),
                 ", %d skipped as host-specific" % len(skipped) if skipped
                 else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
