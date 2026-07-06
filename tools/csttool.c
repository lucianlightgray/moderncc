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

static void suite_store(void) {
    CstArena *a = cst_arena_new(7);
    CstLocal tu = cst_node_open(a, CST_TranslationUnit);
    CstLocal decl = cst_node_open(a, CST_Declaration);
    cst_own_file(a, "x", (const uint8_t *)"int x;", 6);
    CstLocal t0 = cst_leaf(a, 1, 0, 4, NULL, 0);
    CstLocal t1 = cst_leaf(a, 2, 4, 1, NULL, 0);
    CstLocal t2 = cst_leaf(a, 3, 5, 1, NULL, 0);
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

    CstId id = cst_id(7, decl);
    CHECK(cst_id_file(id) == 7, "id file round-trip");
    CHECK(cst_id_local(id) == decl, "id local round-trip");

    cst_arena_free(a);
}

static void suite_hash(void) {
    const char *a_src = "int x ;";
    const char *b_src = "int  x  ;";
    uint32_t a_span[3] = {0, 3, 5};
    uint32_t a_triv[3] = {0, 1, 1};
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

    const char *c_src = "int y ;";
    uint32_t c_span[3] = {0, 3, 5};
    uint32_t c_triv[3] = {0, 1, 1};
    CstArena *C = build_flat(c_src, c_span, 3, c_triv);
    cst_rehash_all(C);
    CHECK(!cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                       cst_struct_hash(C, cst_root(C))),
          "struct hash changes when a token changes");

    CstArena *A2 = build_flat(a_src, a_span, 3, a_triv);
    cst_rehash_all(A2);
    CHECK(cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                      cst_struct_hash(A2, cst_root(A2))),
          "identical trees hash equal");

    CstHash leafA = cst_hash_leaf(1, (const uint8_t *)"a", 1);
    CstHash leafB = cst_hash_leaf(1, (const uint8_t *)"b", 1);
    CstHash kids2[2] = {leafA, leafB};
    CstHash kids3[3] = {leafA, leafB, leafB};
    CHECK(!cst_hash_eq(cst_hash_internal(CST_Binary, kids2, 2),
                       cst_hash_internal(CST_Binary, kids3, 3)),
          "child-count is part of the salt");

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

static void suite_geom(void) {
    const char *src = "int x = 42;";
    uint32_t span[5] = {0, 3, 5, 7, 10};
    uint32_t triv[5] = {0, 1, 1, 1, 0};
    CstArena *a = build_flat(src, span, 5, triv);

    uint32_t sum = 0;
    CstLocal c;
    for (c = cst_first_child(a, cst_root(a)); c != CST_NONE;
         c = cst_next_sib(a, c))
        sum += cst_width(a, c);
    CHECK(sum == cst_width(a, cst_root(a)), "tiling: children sum to parent");
    CHECK(cst_width(a, cst_root(a)) == strlen(src), "root width == source len");

    cst_index_build(a);

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

    uint32_t expect = 0;
    for (c = cst_first_child(a, cst_root(a)); c != CST_NONE;
         c = cst_next_sib(a, c)) {
        CHECK(cst_abs_offset(a, c) == expect, "leaf abs_offset abuts");
        expect += cst_width(a, c);
    }

    cst_arena_free(a);
}

static void suite_serial(void) {
    const char *src = "int main(){return 0;}";
    uint32_t span[6] = {0, 3, 8, 10, 12, 20};
    CstArena *a = build_flat(src, span, 6, NULL);

    uint8_t buf[256];
    size_t need = cst_reflect(a, cst_root(a), NULL, 0);
    CHECK(need == strlen(src), "reflect size == source len");
    size_t wrote = cst_reflect(a, cst_root(a), buf, sizeof buf);
    CHECK(wrote == strlen(src), "reflect wrote whole source");
    CHECK(memcmp(buf, src, strlen(src)) == 0, "reflect byte-identical");

    cst_rehash_all(a);
    CstHash root_h = cst_struct_hash(a, cst_root(a));

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

    FILE *f = fopen(path, "r+b");
    if (f) {
        uint32_t bad = 0xFFFFFFFFu;
        fseek(f, 4, SEEK_SET);
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

static void suite_sym(void) {
    CstArena *a = cst_arena_new(3);
    cst_own_file(a, "s", (const uint8_t *)"int def; use=def;", 17);
    CstLocal tu = cst_node_open(a, CST_TranslationUnit);
    CstLocal d = cst_node_open(a, CST_Declaration);
    CstLocal defname = cst_leaf(a, 1, 0, 8, NULL, 0);
    cst_node_close(a, d);
    CstLocal u = cst_node_open(a, CST_ExprStmt);
    CstLocal usename = cst_leaf(a, 1, 8, 9, NULL, 0);
    cst_node_close(a, u);
    cst_node_close(a, tu);

    cst_set_sym_ref(a, usename, cst_id(3, defname));
    CstId got = cst_sym_ref(a, usename);
    CHECK(cst_id_file(got) == 3, "sym_ref file id round-trips");
    CHECK(cst_id_local(got) == defname, "sym_ref def node-id round-trips");
    CHECK(cst_sym_ref(a, defname) == (CstId)0xffffffffffffffffull,
          "unset sym_ref is NONE");

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

static void suite_comment(void) {
    const char *a_src = "int x;";
    CstArena *A = cst_arena_new(0);
    cst_own_file(A, "a", (const uint8_t *)a_src, 6);
    cst_node_open(A, CST_TranslationUnit);
    cst_leaf(A, 1, 0, 4, NULL, 0);
    cst_leaf(A, 2, 4, 1, NULL, 0);
    cst_leaf(A, 3, 5, 1, NULL, 0);
    cst_node_close(A, cst_root(A));
    cst_rehash_all(A);

    const char *b_src = "int /*c*/x;";
    CstArena *B = cst_arena_new(0);
    cst_own_file(B, "b", (const uint8_t *)b_src, 11);
    cst_node_open(B, CST_TranslationUnit);
    cst_leaf(B, 1, 0, 4, NULL, 0);
    cst_leaf_kinded(B, CST_Comment, 0, 4, 5, NULL, 0);
    cst_leaf(B, 2, 9, 1, NULL, 0);
    cst_leaf(B, 3, 10, 1, NULL, 0);
    cst_node_close(B, cst_root(B));
    cst_rehash_all(B);
    cst_index_build(B);

    CHECK(cst_hash_eq(cst_struct_hash(A, cst_root(A)),
                      cst_struct_hash(B, cst_root(B))),
          "H_s excludes the comment node (comment-invariant)");
    CHECK(!cst_hash_eq(cst_trivia_hash(A, cst_root(A)),
                       cst_trivia_hash(B, cst_root(B))),
          "H_t includes the comment bytes");

    uint8_t buf[32];
    size_t w = cst_reflect(B, cst_root(B), buf, sizeof buf);
    CHECK(w == 11 && memcmp(buf, b_src, 11) == 0, "comment round-trips");

    char msg[64];
    CHECK(cst_validate(B, msg, sizeof msg) == 0, "comment tree validates");

    CstLocal cn = cst_node_at(B, 6);
    CHECK(cn != CST_NONE && cst_kind(B, cn) == CST_Comment,
          "offset inside a comment maps to the Comment node");

    cst_arena_free(A);
    cst_arena_free(B);
}


static CstArena *build_toggle(const char *ifdef_line, const char *a_branch,
                              const char *else_line, const char *b_branch,
                              const char *endif_line, CstLocal *cond_out) {
    char src[256];
    uint32_t o0 = 0, l0 = strlen(ifdef_line);
    uint32_t o1 = l0, l1 = strlen(a_branch);
    uint32_t o2 = o1 + l1, l2 = strlen(else_line);
    uint32_t o3 = o2 + l2, l3 = strlen(b_branch);
    uint32_t o4 = o3 + l3, l4 = strlen(endif_line);
    uint32_t total = o4 + l4;
    memcpy(src + o0, ifdef_line, l0);
    memcpy(src + o1, a_branch, l1);
    memcpy(src + o2, else_line, l2);
    memcpy(src + o3, b_branch, l3);
    memcpy(src + o4, endif_line, l4);

    CstArena *a = cst_arena_new(0);
    cst_own_file(a, "toggle", (const uint8_t *)src, total);
    cst_node_open(a, CST_TranslationUnit);
    CstLocal cond = cst_node_open(a, CST_PPConditional);
    cst_leaf(a, 1, o0, l0, NULL, 0);
    CstLocal ab = cst_leaf(a, 1, o1, l1, NULL, 0);
    cst_mark_branch(a, ab, 0);
    cst_leaf(a, 1, o2, l2, NULL, 0);
    CstLocal bb = cst_leaf(a, 1, o3, l3, NULL, 0);
    cst_mark_branch(a, bb, 1);
    cst_leaf(a, 1, o4, l4, NULL, 0);
    cst_node_close(a, cond);
    cst_node_close(a, cst_root(a));
    cst_rehash_all(a);
    if (cond_out)
        *cond_out = cond;
    return a;
}

static size_t render_to(const CstStore *s, const CstBinding *b, char *out,
                        size_t cap) {
    size_t n = cst_render(s, b, (uint8_t *)out, cap);
    if (n < cap)
        out[n] = '\0';
    return n;
}

static void suite_template(void) {
    CstStore *store = cst_store_new();

    CstLocal cond;
    CstArena *f1 = build_toggle("#ifdef FEATURE_FLAG_TOGGLE\n", "  int chosen = 1;\n",
                                "#else\n", "  int chosen = 0;\n", "#endif\n", &cond);
    CstLocal cond2;
    CstArena *f2 = build_toggle("#ifdef FEATURE_FLAG_TOGGLE\n", "  int chosen = 1;\n",
                                "#else\n", "  int chosen = 0;\n", "#endif\n", &cond2);

    CstLocal ifb = cst_first_child(f1, cond);
    ifb = cst_next_sib(f1, ifb);
    CstLocal elb = cst_next_sib(f1, cst_next_sib(f1, ifb));
    CstHash zero = {0, 0};
    CHECK(!cst_hash_eq(cst_struct_hash(f1, ifb), zero), "#if branch is hashed");
    CHECK(!cst_hash_eq(cst_struct_hash(f1, elb), zero), "#else branch is hashed");

    uint32_t t1 = cst_store_intern(store, f1);
    uint32_t t2 = cst_store_intern(store, f2);

    CHECK(t1 == t2, "both includes reference the same SourceFile template id");
    CHECK(cst_store_count(store) == 1, "the header is stored exactly once");

    CstArena *tmpl = cst_store_get(store, t1);
    CstLocal tcond = cst_first_child(tmpl, cst_root(tmpl));

    CstBinding *inst1 = cst_binding_new(t1);
    cst_binding_select(inst1, tcond, 1);
    CstBinding *inst2 = cst_binding_new(t1);
    cst_binding_select(inst2, tcond, 0);

    char buf[256];
    render_to(store, inst1, buf, sizeof buf);
    CHECK(strcmp(buf, "  int chosen = 0;\n") == 0,
          "instance 1 (undefined) renders the #else branch");
    render_to(store, inst2, buf, sizeof buf);
    CHECK(strcmp(buf, "  int chosen = 1;\n") == 0,
          "instance 2 (defined) renders the #if branch");

    char rt[256];
    size_t rn = cst_render_identity(tmpl, (uint8_t *)rt, sizeof rt);
    rt[rn] = '\0';
    CHECK(strcmp(rt, "#ifdef FEATURE_FLAG_TOGGLE\n  int chosen = 1;\n#else\n"
                     "  int chosen = 0;\n#endif\n") == 0,
          "identity render reproduces the written source");

    CstArena *inner = build_toggle("#ifdef T\n", "IA\n", "#else\n", "IB\n",
                                   "#endif\n", NULL);
    uint32_t tin = cst_store_intern(store, inner);

    const char *osrc = "#include \"inner.h\"\n#ifdef U\nOA\n#else\nOB\n#endif\n";
    CstArena *outer = cst_arena_new(0);
    cst_own_file(outer, "outer", (const uint8_t *)osrc, strlen(osrc));
    cst_node_open(outer, CST_TranslationUnit);
    CstLocal inc = cst_node_open(outer, CST_IncludeDirective);
    cst_leaf(outer, 1, 0, 19, NULL, 0);
    cst_node_close(outer, inc);
    cst_set_include_target(outer, inc, tin);
    CstLocal ocond = cst_node_open(outer, CST_PPConditional);
    cst_leaf(outer, 1, 19, 9, NULL, 0);
    CstLocal oa = cst_leaf(outer, 1, 28, 3, NULL, 0);
    cst_mark_branch(outer, oa, 0);
    cst_leaf(outer, 1, 31, 6, NULL, 0);
    CstLocal ob = cst_leaf(outer, 1, 37, 3, NULL, 0);
    cst_mark_branch(outer, ob, 1);
    cst_leaf(outer, 1, 40, 7, NULL, 0);
    cst_node_close(outer, ocond);
    cst_node_close(outer, cst_root(outer));
    cst_rehash_all(outer);
    uint32_t tout = cst_store_intern(store, outer);

    CHECK(cst_include_target(cst_store_get(store, tout),
                             cst_first_child(cst_store_get(store, tout),
                                             cst_root(cst_store_get(store, tout)))) == tin,
          "the IncludeDirective targets the inner template id");

    CstArena *ot = cst_store_get(store, tout);
    CstLocal otcond = cst_next_sib(ot, cst_first_child(ot, cst_root(ot)));

    CstBinding *ob_a = cst_binding_new(tout);
    cst_binding_select(ob_a, otcond, 0);
    CstBinding *ib_a = cst_binding_new(tin);
    cst_binding_select(ib_a, cst_first_child(cst_store_get(store, tin),
                                             cst_root(cst_store_get(store, tin))), 0);
    cst_binding_add_include(ob_a, ib_a);
    render_to(store, ob_a, buf, sizeof buf);
    CHECK(strcmp(buf, "IA\nOA\n") == 0, "recursive: inner #if + outer #if");

    CstBinding *ob_b = cst_binding_new(tout);
    cst_binding_select(ob_b, otcond, 0);
    CstBinding *ib_b = cst_binding_new(tin);
    cst_binding_select(ib_b, cst_first_child(cst_store_get(store, tin),
                                             cst_root(cst_store_get(store, tin))), 1);
    cst_binding_add_include(ob_b, ib_b);
    render_to(store, ob_b, buf, sizeof buf);
    CHECK(strcmp(buf, "IB\nOA\n") == 0,
          "recursive: toggling the nested header flips only its branch");

    CstBinding *ob_c = cst_binding_new(tout);
    cst_binding_select(ob_c, otcond, 1);
    CstBinding *ib_c = cst_binding_new(tin);
    cst_binding_select(ib_c, cst_first_child(cst_store_get(store, tin),
                                             cst_root(cst_store_get(store, tin))), 0);
    cst_binding_add_include(ob_c, ib_c);
    render_to(store, ob_c, buf, sizeof buf);
    CHECK(strcmp(buf, "IA\nOB\n") == 0,
          "recursive: toggling the outer header flips only its branch");

    cst_binding_free(inst1);
    cst_binding_free(inst2);
    cst_binding_free(ob_a);
    cst_binding_free(ob_b);
    cst_binding_free(ob_c);
    cst_store_free(store);
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
    if (!only || !strcmp(only, "template"))
        suite_template();

    fprintf(stderr, "csttool: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
