/*
 * csttool.c — pure-library test harness for the CST database.
 *
 * See docs/IMPLEMENTATION.md (§0, §6): the pure slices (B node store, C hashing,
 * D geometry, E serialization) are proven in isolation against SYNTHETIC trees
 * built directly through the public API — no compiler, no real source flows here.
 * This is the substrate that lets B–E be trusted before Weave 1.
 *
 * Compiled with CONFIG_MCC_CST=1 and #includes mcccst.c directly, so it links
 * nothing from the compiler (invariant PLAN §0.3).
 *
 * Usage: csttool [suite]      (default: run all; exit nonzero on first failure)
 */
#include "mcccst.c"

#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_checks;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        g_checks++;                                                             \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, msg);       \
            g_failures++;                                                       \
        }                                                                       \
    } while (0)

/* ------------------------------------------------------------------ *
 * A synthetic-tree builder: parse a tiny bracketed DSL into a CST so the
 * pure slices can be exercised without the compiler. The DSL leaf is just a
 * run of source bytes; whitespace between tokens becomes a leaf's leading
 * trivia so tiling/round-trip stay exact.
 *
 * We build directly instead: a helper that appends leaves whose owned spans
 * tile a given source string, grouped under structural nodes.
 * ------------------------------------------------------------------ */

/* Build a flat tree: TranslationUnit over `ntok` leaves that together tile
 * `src`. Each token is [offs[i], offs[i+1]); optional leading-trivia length
 * triv[i] carves the whitespace prefix out of the structural hash. */
static CstArena *build_flat(const char *src, const uint32_t *offs, uint32_t ntok,
                            const uint32_t *triv) {
    CstArena *a = cst_arena_new(0);
    uint32_t len = (uint32_t)strlen(src);
    cst_own_file(a, "<synthetic>", (const uint8_t *)src, len);
    cst_node_open(a, CST_TranslationUnit);
    uint32_t i;
    for (i = 0; i < ntok; i++) {
        uint32_t start = offs[i];
        uint32_t end = (i + 1 < ntok) ? offs[i + 1] : len;
        CstTrivia tv;
        const CstTrivia *tvp = NULL;
        uint32_t tvn = 0;
        if (triv && triv[i]) {
            tv.kind = CST_TRIV_WS;
            tv.offset = 0;
            tv.length = triv[i];
            tvp = &tv;
            tvn = 1;
        }
        cst_leaf(a, (uint16_t)('a' + (i % 5)), start, end - start, tvp, tvn);
    }
    cst_node_close(a, cst_root(a));
    return a;
}

/* ================================================================== *
 * Slice B — node store topology + ids
 * ================================================================== */
static void suite_store(void) {
    CstArena *a = cst_arena_new(7);
    CstLocal tu = cst_node_open(a, CST_TranslationUnit);
    CstLocal decl = cst_node_open(a, CST_Declaration);
    cst_own_file(a, "x", (const uint8_t *)"int x;", 6);
    CstLocal t0 = cst_leaf(a, 1, 0, 4, NULL, 0); /* "int " */
    CstLocal t1 = cst_leaf(a, 2, 4, 1, NULL, 0); /* "x"    */
    CstLocal t2 = cst_leaf(a, 3, 5, 1, NULL, 0); /* ";"    */
    cst_node_close(a, decl);
    cst_node_close(a, tu);

    CHECK(cst_root(a) == tu, "root is first node");
    CHECK(cst_parent(a, decl) == tu, "decl parent is TU");
    CHECK(cst_parent(a, t0) == decl, "leaf parent is decl");
    CHECK(cst_first_child(a, decl) == t0, "first child");
    CHECK(cst_next_sib(a, t0) == t1, "sibling order 0->1");
    CHECK(cst_next_sib(a, t1) == t2, "sibling order 1->2");
    CHECK(cst_next_sib(a, t2) == CST_NONE, "last sibling");
    CHECK(cst_width(a, decl) == 6, "decl width == sum of leaves");
    CHECK(cst_width(a, tu) == 6, "tu width == 6");
    CHECK(cst_node_count(a) == 5, "5 nodes");

    /* id encode/decode round-trip */
    CstId id = cst_id(7, decl);
    CHECK(cst_id_file(id) == 7, "id file round-trip");
    CHECK(cst_id_local(id) == decl, "id local round-trip");

    cst_arena_free(a);
}

/* ================================================================== *
 * Slice C — hashing invariance (PLAN §8.4)
 * ================================================================== */
static void suite_hash(void) {
    /* Two token streams identical except for whitespace/trivia. Structural
     * hash must match; trivia hash must differ. */
    const char *a_src = "int x ;";       /* tokens: "int","x",";" */
    const char *b_src = "int  x  ;";     /* same tokens, more ws  */
    /* Each leaf span includes its leading whitespace: the token owns the prefix.
     * Layout A: "int"|" x"|" ;"  -> spans [0,3),[3,5),[5,7); triv 0,1,1 */
    uint32_t a_span[3] = {0, 3, 5};
    uint32_t a_triv[3] = {0, 1, 1};
    /* Layout B: "int"|"  x"|"  ;" -> spans [0,3),[3,6),[6,9); triv 0,2,2 */
    uint32_t b_span[3] = {0, 3, 6};
    uint32_t b_triv[3] = {0, 2, 2};

    CstArena *A = build_flat(a_src, a_span, 3, a_triv);
    CstArena *B = build_flat(b_src, b_span, 3, b_triv);
    cst_rehash_all(A);
    cst_rehash_all(B);

    CHECK(cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                      cst_struct_hash(B, cst_root(B))),
          "struct hash invariant to whitespace-only change");
    CHECK(!cst_hash_eq(cst_trivia_hash(A, cst_root(A)),
                       cst_trivia_hash(B, cst_root(B))),
          "trivia hash detects whitespace change");

    /* Changing a token's bytes MUST change the structural hash. */
    const char *c_src = "int y ;";
    uint32_t c_span[3] = {0, 3, 5};
    uint32_t c_triv[3] = {0, 1, 1};
    CstArena *C = build_flat(c_src, c_span, 3, c_triv);
    cst_rehash_all(C);
    CHECK(!cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                       cst_struct_hash(C, cst_root(C))),
          "struct hash changes when a token changes");

    /* Identical subtrees hash equal. */
    CstArena *A2 = build_flat(a_src, a_span, 3, a_triv);
    cst_rehash_all(A2);
    CHECK(cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                      cst_struct_hash(A2, cst_root(A2))),
          "identical trees hash equal");

    /* child-count salt: a+b vs a+(b) — different structure, different hash. */
    CstHash leafA = cst_hash_leaf(1, (const uint8_t *)"a", 1);
    CstHash leafB = cst_hash_leaf(1, (const uint8_t *)"b", 1);
    CstHash kids2[2] = {leafA, leafB};
    CstHash kids3[3] = {leafA, leafB, leafB};
    CHECK(!cst_hash_eq(cst_hash_internal(CST_Binary, kids2, 2),
                       cst_hash_internal(CST_Binary, kids3, 3)),
          "child-count is part of the salt");

    /* frontier rehash matches full rehash after a hypothetical touch. */
    CstLocal touched[1] = {cst_first_child(A, cst_root(A))};
    cst_rehash_frontier(A, touched, 1);
    CHECK(cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                      cst_struct_hash(A2, cst_root(A2))),
          "frontier rehash consistent with full rehash");

    cst_arena_free(A);
    cst_arena_free(A2);
    cst_arena_free(B);
    cst_arena_free(C);
}

/* ================================================================== *
 * Slice D — geometry: tiling + offset round-trip (PLAN §8.2/§8.3)
 * ================================================================== */
static void suite_geom(void) {
    const char *src = "int x = 42;";
    uint32_t span[5] = {0, 3, 5, 7, 10}; /* "int"," x"," ="," 42",";" */
    uint32_t triv[5] = {0, 1, 1, 1, 0};
    CstArena *a = build_flat(src, span, 5, triv);

    /* Tiling: sum of child widths == parent width; leaves cover every byte. */
    uint32_t sum = 0;
    CstLocal c;
    for (c = cst_first_child(a, cst_root(a)); c != CST_NONE;
         c = cst_next_sib(a, c))
        sum += cst_width(a, c);
    CHECK(sum == cst_width(a, cst_root(a)), "tiling: children sum to parent");
    CHECK(cst_width(a, cst_root(a)) == strlen(src), "root width == source len");

    cst_index_build(a);

    /* Every byte offset maps to a leaf whose span contains it, and the leaf's
     * absolute span round-trips the offset. */
    uint32_t off;
    for (off = 0; off < strlen(src); off++) {
        CstLocal n = cst_node_at(a, off);
        CHECK(n != CST_NONE, "offset maps to a node");
        if (n == CST_NONE)
            continue;
        uint32_t start = cst_abs_offset(a, n);
        CHECK(off >= start && off < start + cst_width(a, n),
              "offset within node span");
    }

    /* abs_offset of consecutive leaves abut (no gaps/overlaps). */
    uint32_t expect = 0;
    for (c = cst_first_child(a, cst_root(a)); c != CST_NONE;
         c = cst_next_sib(a, c)) {
        CHECK(cst_abs_offset(a, c) == expect, "leaf abs_offset abuts");
        expect += cst_width(a, c);
    }

    cst_arena_free(a);
}

/* ================================================================== *
 * Slice E — reflection round-trip + snapshot round-trip (PLAN §8.1/§8.6)
 * ================================================================== */
static void suite_serial(void) {
    const char *src = "int main(){return 0;}";
    uint32_t span[6] = {0, 3, 8, 10, 12, 20};
    CstArena *a = build_flat(src, span, 6, NULL);

    /* Round-trip: reflect(tree) == source bytes. */
    uint8_t buf[256];
    size_t need = cst_reflect(a, cst_root(a), NULL, 0);
    CHECK(need == strlen(src), "reflect size == source len");
    size_t wrote = cst_reflect(a, cst_root(a), buf, sizeof buf);
    CHECK(wrote == strlen(src), "reflect wrote whole source");
    CHECK(memcmp(buf, src, strlen(src)) == 0, "reflect byte-identical");

    cst_rehash_all(a);
    CstHash root_h = cst_struct_hash(a, cst_root(a));

    /* Snapshot round-trip: save -> load -> identical tree + hashes. */
    const char *path = "csttool_snapshot.tmp";
    CHECK(cst_snapshot_save(a, path) == 0, "snapshot save ok");
    CstArena *b = cst_snapshot_load(path);
    CHECK(b != NULL, "snapshot load ok");
    if (b) {
        CHECK(cst_node_count(b) == cst_node_count(a), "loaded node count");
        CHECK(cst_hash_eq(cst_struct_hash(b, cst_root(b)), root_h),
              "loaded root hash matches");
        uint8_t buf2[256];
        size_t w2 = cst_reflect(b, cst_root(b), buf2, sizeof buf2);
        CHECK(w2 == strlen(src) && memcmp(buf2, src, w2) == 0,
              "reflect after reload byte-identical");
        cst_arena_free(b);
    }

    /* Version/endian skew is rejected cleanly, not misread. */
    FILE *f = fopen(path, "r+b");
    if (f) {
        uint32_t bad = 0xFFFFFFFFu;
        fseek(f, 4, SEEK_SET); /* clobber version field */
        fwrite(&bad, 1, 4, f);
        fclose(f);
        CstArena *c = cst_snapshot_load(path);
        CHECK(c == NULL, "version skew rejected");
        if (c)
            cst_arena_free(c);
    }
    remove(path);
    cst_arena_free(a);
}

/* ================================================================== *
 * Slice I — symbol refs stored as node-ids (PLAN §1 Symbols).
 * Proves def<->use node-ids round-trip (the CST-side storage contract);
 * the parser-side def-site tracking is Weave-3 work over this mechanism.
 * ================================================================== */
static void suite_sym(void) {
    /* Model: `int def; ... def;` — a use node referencing a def node by id. */
    CstArena *a = cst_arena_new(3);
    cst_own_file(a, "s", (const uint8_t *)"int def; use=def;", 17);
    CstLocal tu = cst_node_open(a, CST_TranslationUnit);
    CstLocal d = cst_node_open(a, CST_Declaration);
    CstLocal defname = cst_leaf(a, 1, 0, 8, NULL, 0); /* "int def;" */
    cst_node_close(a, d);
    CstLocal u = cst_node_open(a, CST_ExprStmt);
    CstLocal usename = cst_leaf(a, 1, 8, 9, NULL, 0);
    cst_node_close(a, u);
    cst_node_close(a, tu);

    /* Record the use->def cross-reference as a tagged (file,local) id. */
    cst_set_sym_ref(a, usename, cst_id(3, defname));
    CstId got = cst_sym_ref(a, usename);
    CHECK(cst_id_file(got) == 3, "sym_ref file id round-trips");
    CHECK(cst_id_local(got) == defname, "sym_ref def node-id round-trips");
    CHECK(cst_sym_ref(a, defname) == (CstId)0xffffffffffffffffull,
          "unset sym_ref is NONE");

    /* Survives a snapshot round-trip (sym_ref is a serialized column). */
    const char *path = "csttool_sym.tmp";
    CHECK(cst_snapshot_save(a, path) == 0, "sym snapshot save");
    CstArena *b = cst_snapshot_load(path);
    CHECK(b != NULL, "sym snapshot load");
    if (b) {
        CstId g2 = cst_sym_ref(b, usename);
        CHECK(cst_id_local(g2) == defname, "sym_ref survives snapshot");
        cst_arena_free(b);
    }
    remove(path);
    cst_arena_free(a);
}

/* ================================================================== *
 * D1d — Comment promotion. A CST_Comment leaf tiles the source and is found
 * by offset lookup, but is excluded from H_s (so a comment-only edit keeps the
 * structural hash) while its bytes feed H_t (PLAN §8.4 / docs/CST.md §D1d).
 * ================================================================== */
static void suite_comment(void) {
    /* Tree A is `int x;`; tree B inserts a block comment between int and x,
     * with the same three tokens. Their H_s must match; their H_t must not. */
    const char *a_src = "int x;";
    CstArena *A = cst_arena_new(0);
    cst_own_file(A, "a", (const uint8_t *)a_src, 6);
    cst_node_open(A, CST_TranslationUnit);
    cst_leaf(A, 1, 0, 4, NULL, 0); /* "int " */
    cst_leaf(A, 2, 4, 1, NULL, 0); /* "x"    */
    cst_leaf(A, 3, 5, 1, NULL, 0); /* ";"    */
    cst_node_close(A, cst_root(A));
    cst_rehash_all(A);

    const char *b_src = "int /*c*/x;";
    CstArena *B = cst_arena_new(0);
    cst_own_file(B, "b", (const uint8_t *)b_src, 11);
    cst_node_open(B, CST_TranslationUnit);
    cst_leaf(B, 1, 0, 4, NULL, 0);                     /* the "int " token   */
    cst_leaf_kinded(B, CST_Comment, 0, 4, 5, NULL, 0); /* the 5-byte comment */
    cst_leaf(B, 2, 9, 1, NULL, 0);                     /* the "x" token      */
    cst_leaf(B, 3, 10, 1, NULL, 0);                    /* the ";" token      */
    cst_node_close(B, cst_root(B));
    cst_rehash_all(B);
    cst_index_build(B);

    CHECK(cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                      cst_struct_hash(B, cst_root(B))),
          "H_s excludes the comment node (comment-invariant)");
    CHECK(!cst_hash_eq(cst_trivia_hash(A, cst_root(A)),
                       cst_trivia_hash(B, cst_root(B))),
          "H_t includes the comment bytes");

    /* Round-trip: the comment bytes are reflected verbatim. */
    uint8_t buf[32];
    size_t w = cst_reflect(B, cst_root(B), buf, sizeof buf);
    CHECK(w == 11 && memcmp(buf, b_src, 11) == 0, "comment round-trips");

    /* Tiling holds with the comment as a leaf. */
    char msg[64];
    CHECK(cst_validate(B, msg, sizeof msg) == 0, "comment tree validates");

    /* Offset lookup lands on the comment node for a byte inside it. */
    CstLocal cn = cst_node_at(B, 6); /* a byte inside the comment span */
    CHECK(cn != CST_NONE && cst_kind(B, cn) == CST_Comment,
          "offset inside a comment maps to the Comment node");

    cst_arena_free(A);
    cst_arena_free(B);
}

int main(int argc, char **argv) {
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "store"))
        suite_store();
    if (!only || !strcmp(only, "hash"))
        suite_hash();
    if (!only || !strcmp(only, "geom"))
        suite_geom();
    if (!only || !strcmp(only, "serial"))
        suite_serial();
    if (!only || !strcmp(only, "sym"))
        suite_sym();
    if (!only || !strcmp(only, "comment"))
        suite_comment();

    fprintf(stderr, "csttool: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
