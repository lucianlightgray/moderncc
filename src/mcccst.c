/*
 * mcccst.c — Concrete Syntax Tree (CST) database implementation.
 *
 * See docs/PLAN.md, docs/IMPLEMENTATION.md, and src/mcccst.h.
 *
 * Self-contained (invariant PLAN §0.3): uses only the C library (malloc/free),
 * never mcc's allocators or globals, so the pure-library test harness
 * (tools/csttool.c) can link this file alone with no compiler.
 *
 * Everything is wrapped in the CONFIG_MCC_CST guard so that when the feature is
 * off this translation unit is empty — provable zero-cost-off (PLAN §0.2).
 *
 * Slices implemented here (IMPLEMENTATION.md §1):
 *   B  node store core        C  hashing        D  geometry / offset index
 *   E  serialization          G  owned source   I  symbol refs
 *   H  recording-hook skeleton (current-arena + build stack)
 */
#if defined(CONFIG_MCC_CST) && CONFIG_MCC_CST

#include "mcccst.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CST_ASSERT
#include <assert.h>
#define CST_ASSERT(x) assert(x)
#endif

/* The CST owns its memory via the raw C library (invariant PLAN §0.3), never
 * mcc's arena. In the amalgamated build mcc.h poisons malloc/realloc/free to
 * force use of its own allocators; un-poison them for this self-contained unit
 * (harmless no-op in the standalone csttool build where the poison is absent). */
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef realloc
#undef free

#define CST_ID_NONE ((CstId)0xffffffffffffffffull)

/* ================================================================== *
 * Arena / SoA node store (slice B)
 * ================================================================== */

struct CstArena {
    uint32_t file_id;

    /* SoA columns, parallel arrays indexed by CstLocal (PLAN §2). */
    uint16_t *kind;
    CstLocal *parent;
    CstLocal *first_child;
    CstLocal *last_child; /* build-time O(1) append; also serialized */
    CstLocal *next_sib;
    uint32_t *width;      /* relative bytes this node spans */
    CstHash  *struct_hash;
    CstHash  *trivia_hash;
    uint64_t *sym_ref;    /* CstId of def-site, or CST_ID_NONE */
    uint32_t *slot_key;   /* reserved for 5B epoch hash (§3.1); 0 in v1 */

    /* Leaf side-columns, valid where kind == CST_Token. */
    uint16_t *tok_kind;
    uint32_t *leaf_off;   /* start of owned span (leading trivia + token) */
    uint32_t *leaf_len;   /* full span length; == width for a leaf */
    uint32_t *tok_rel;    /* token start within the span (leading trivia len) */
    uint32_t *triv_start; /* index into trivia pool */
    uint32_t *triv_count;

    CstLocal count;
    CstLocal cap;

    /* Trivia pool (relative offsets within the owning leaf). */
    CstTrivia *trivia;
    uint32_t trivia_count, trivia_cap;

    /* Owned source bytes (slice G). */
    uint8_t *src;
    uint32_t src_len, src_cap;

    /* Build stack (slice H). */
    CstLocal *stack;
    uint32_t stack_top, stack_cap;

    /* offset->node index (slice D): leaf starts in source order. */
    struct {
        uint32_t start;
        CstLocal node;
    } *index;
    uint32_t index_count;
    int index_valid;
};

static void *cst_xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fprintf(stderr, "mcccst: out of memory\n");
        abort();
    }
    return q;
}

static void cst_grow(CstArena *a, CstLocal need) {
    CstLocal cap = a->cap;
    if (need <= cap)
        return;
    if (cap == 0)
        cap = 64;
    while (cap < need)
        cap *= 2;
#define G(field) a->field = cst_xrealloc(a->field, (size_t)cap * sizeof *a->field)
    G(kind);
    G(parent);
    G(first_child);
    G(last_child);
    G(next_sib);
    G(width);
    G(struct_hash);
    G(trivia_hash);
    G(sym_ref);
    G(slot_key);
    G(tok_kind);
    G(leaf_off);
    G(leaf_len);
    G(tok_rel);
    G(triv_start);
    G(triv_count);
#undef G
    a->cap = cap;
}

CstArena *cst_arena_new(uint32_t file_id) {
    CstArena *a = cst_xrealloc(NULL, sizeof *a);
    memset(a, 0, sizeof *a);
    a->file_id = file_id;
    return a;
}

void cst_arena_reset(CstArena *a) {
    a->count = 0;
    a->trivia_count = 0;
    a->src_len = 0;
    a->stack_top = 0;
    a->index_count = 0;
    a->index_valid = 0;
}

void cst_arena_free(CstArena *a) {
    if (!a)
        return;
    free(a->kind);
    free(a->parent);
    free(a->first_child);
    free(a->last_child);
    free(a->next_sib);
    free(a->width);
    free(a->struct_hash);
    free(a->trivia_hash);
    free(a->sym_ref);
    free(a->slot_key);
    free(a->tok_kind);
    free(a->leaf_off);
    free(a->leaf_len);
    free(a->tok_rel);
    free(a->triv_start);
    free(a->triv_count);
    free(a->trivia);
    free(a->src);
    free(a->stack);
    free(a->index);
    free(a);
}

/* Allocate a fresh node, default-initialize every column. */
static CstLocal cst_alloc_node(CstArena *a, uint16_t kind) {
    CstLocal n = a->count;
    cst_grow(a, n + 1);
    a->kind[n] = kind;
    a->parent[n] = CST_NONE;
    a->first_child[n] = CST_NONE;
    a->last_child[n] = CST_NONE;
    a->next_sib[n] = CST_NONE;
    a->width[n] = 0;
    a->struct_hash[n].lo = a->struct_hash[n].hi = 0;
    a->trivia_hash[n].lo = a->trivia_hash[n].hi = 0;
    a->sym_ref[n] = CST_ID_NONE;
    a->slot_key[n] = 0;
    a->tok_kind[n] = 0;
    a->leaf_off[n] = 0;
    a->leaf_len[n] = 0;
    a->tok_rel[n] = 0;
    a->triv_start[n] = 0;
    a->triv_count[n] = 0;
    a->count = n + 1;
    return n;
}

/* Append `child` as the last child of `parent`, preserving order. */
static void cst_link_child(CstArena *a, CstLocal parent, CstLocal child) {
    a->parent[child] = parent;
    if (parent == CST_NONE)
        return;
    if (a->first_child[parent] == CST_NONE) {
        a->first_child[parent] = child;
    } else {
        a->next_sib[a->last_child[parent]] = child;
    }
    a->last_child[parent] = child;
}

CstLocal cst_node_open(CstArena *a, uint16_t kind) {
    CstLocal parent = a->stack_top ? a->stack[a->stack_top - 1] : CST_NONE;
    CstLocal n = cst_alloc_node(a, kind);
    cst_link_child(a, parent, n);
    if (a->stack_top >= a->stack_cap) {
        a->stack_cap = a->stack_cap ? a->stack_cap * 2 : 64;
        a->stack = cst_xrealloc(a->stack, a->stack_cap * sizeof *a->stack);
    }
    a->stack[a->stack_top++] = n;
    a->index_valid = 0;
    return n;
}

void cst_node_close(CstArena *a, CstLocal node) {
    CST_ASSERT(a->stack_top > 0 && a->stack[a->stack_top - 1] == node);
    a->stack_top--;
    /* Bubble finalized width into the parent (bottom-up accumulation). */
    CstLocal parent = a->parent[node];
    if (parent != CST_NONE)
        a->width[parent] += a->width[node];
}

CstLocal cst_leaf(CstArena *a, uint16_t tok_kind, uint32_t off, uint32_t len,
                  const CstTrivia *trivia, uint32_t ntrivia) {
    CstLocal parent = a->stack_top ? a->stack[a->stack_top - 1] : CST_NONE;
    CstLocal n = cst_alloc_node(a, CST_Token);
    cst_link_child(a, parent, n);

    uint32_t tok_rel = 0, i;
    uint32_t ts = a->trivia_count;
    for (i = 0; i < ntrivia; i++) {
        if (a->trivia_count >= a->trivia_cap) {
            a->trivia_cap = a->trivia_cap ? a->trivia_cap * 2 : 32;
            a->trivia = cst_xrealloc(a->trivia, a->trivia_cap * sizeof *a->trivia);
        }
        a->trivia[a->trivia_count++] = trivia[i];
        tok_rel += trivia[i].length; /* leading trivia is a contiguous prefix */
    }
    a->tok_kind[n] = tok_kind;
    a->leaf_off[n] = off;
    a->leaf_len[n] = len;
    a->tok_rel[n] = tok_rel;
    a->triv_start[n] = ts;
    a->triv_count[n] = ntrivia;
    a->width[n] = len;

    if (parent != CST_NONE)
        a->width[parent] += len;
    a->index_valid = 0;
    return n;
}

uint16_t cst_kind(const CstArena *a, CstLocal n) { return a->kind[n]; }
CstLocal cst_parent(const CstArena *a, CstLocal n) { return a->parent[n]; }
CstLocal cst_first_child(const CstArena *a, CstLocal n) { return a->first_child[n]; }
CstLocal cst_next_sib(const CstArena *a, CstLocal n) { return a->next_sib[n]; }
uint32_t cst_width(const CstArena *a, CstLocal n) { return a->width[n]; }
CstLocal cst_node_count(const CstArena *a) { return a->count; }
CstLocal cst_root(const CstArena *a) { return a->count ? 0 : CST_NONE; }

/* ================================================================== *
 * Owned source (slice G)
 * ================================================================== */

uint32_t cst_own_file(CstArena *a, const char *name, const uint8_t *bytes,
                      size_t n) {
    (void)name; /* v1: one file per arena; name reserved for multi-file stitch */
    if (a->src_len + n > a->src_cap) {
        a->src_cap = a->src_cap ? a->src_cap : 1024;
        while (a->src_len + n > a->src_cap)
            a->src_cap *= 2;
        a->src = cst_xrealloc(a->src, a->src_cap);
    }
    memcpy(a->src + a->src_len, bytes, n);
    a->src_len += (uint32_t)n;
    return a->file_id;
}

const uint8_t *cst_source(const CstArena *a, uint32_t *len_out) {
    if (len_out)
        *len_out = a->src_len;
    return a->src;
}

/* ================================================================== *
 * Hashing (slice C, PLAN §3). 128-bit non-crypto, two 64-bit lanes.
 * ================================================================== */

static uint64_t cst_mix64(uint64_t h, uint64_t x) {
    h ^= x;
    h *= 0x9E3779B97F4A7C15ull;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    return h;
}

static uint64_t cst_hash_bytes(uint64_t seed, const uint8_t *p, uint32_t len) {
    uint64_t h = cst_mix64(seed, 0x1000193u + len);
    uint32_t i;
    for (i = 0; i < len; i++)
        h = cst_mix64(h, p[i]);
    return h;
}

CstHash cst_hash_leaf(uint16_t tok_kind, const uint8_t *bytes, uint32_t len) {
    CstHash h;
    /* Two independently-seeded lanes; salt with the token kind (PLAN §3). */
    h.lo = cst_hash_bytes(0xC0FFEEull ^ ((uint64_t)tok_kind << 1), bytes, len);
    h.hi = cst_hash_bytes(0x5EED1234ull ^ ((uint64_t)tok_kind << 7 | 1), bytes, len);
    return h;
}

CstHash cst_hash_internal(uint16_t kind, const CstHash *child, uint32_t n) {
    CstHash h;
    /* salt(kind, child_count) disambiguates a+b vs a+(b) (PLAN §3). */
    h.lo = cst_mix64(0xA5A5A5A5ull ^ ((uint64_t)kind << 1), n);
    h.hi = cst_mix64(0x3C3C3C3Cull ^ ((uint64_t)kind << 3 | 1), n);
    uint32_t i;
    for (i = 0; i < n; i++) {
        h.lo = cst_mix64(h.lo, child[i].lo);
        h.lo = cst_mix64(h.lo, child[i].hi);
        h.hi = cst_mix64(h.hi, child[i].hi);
        h.hi = cst_mix64(h.hi, child[i].lo);
    }
    return h;
}

int cst_hash_eq(CstHash x, CstHash y) { return x.lo == y.lo && x.hi == y.hi; }

/* Token bytes of a leaf = its owned span minus the leading trivia prefix. */
static const uint8_t *cst_leaf_tok_bytes(const CstArena *a, CstLocal n,
                                         uint32_t *len_out) {
    uint32_t off = a->leaf_off[n] + a->tok_rel[n];
    *len_out = a->leaf_len[n] - a->tok_rel[n];
    return a->src + off;
}

static CstHash cst_compute_struct(CstArena *a, CstLocal n);

static CstHash cst_compute_struct(CstArena *a, CstLocal n) {
    if (a->kind[n] == CST_Token) {
        uint32_t tl;
        const uint8_t *tb = cst_leaf_tok_bytes(a, n, &tl);
        CstHash h = cst_hash_leaf(a->tok_kind[n], tb, tl);
        a->struct_hash[n] = h;
        return h;
    }
    /* Gather children hashes (post-order: recurse first). */
    CstHash stackbuf[16];
    CstHash *ch = stackbuf;
    uint32_t cap = 16, cnt = 0;
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
        if (cnt >= cap) {
            cap *= 2;
            CstHash *nw = cst_xrealloc(ch == stackbuf ? NULL : ch,
                                       cap * sizeof *nw);
            if (ch == stackbuf)
                memcpy(nw, stackbuf, cnt * sizeof *nw);
            ch = nw;
        }
        ch[cnt++] = cst_compute_struct(a, c);
    }
    CstHash h = cst_hash_internal(a->kind[n], ch, cnt);
    if (ch != stackbuf)
        free(ch);
    a->struct_hash[n] = h;
    return h;
}

/* Trivia hash: folds trivia bytes + kinds + widths (PLAN §3). */
static CstHash cst_compute_trivia(CstArena *a, CstLocal n) {
    CstHash h;
    h.lo = 0xDEADBEEFull;
    h.hi = 0xFEEDFACEull;
    if (a->kind[n] == CST_Token) {
        uint32_t i;
        for (i = 0; i < a->triv_count[n]; i++) {
            CstTrivia *t = &a->trivia[a->triv_start[n] + i];
            h.lo = cst_mix64(h.lo, ((uint64_t)t->kind << 32) | t->length);
            const uint8_t *tb = a->src + a->leaf_off[n] + t->offset;
            h.hi = cst_hash_bytes(h.hi, tb, t->length);
        }
    }
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
        CstHash cc = cst_compute_trivia(a, c);
        h.lo = cst_mix64(h.lo, cc.lo);
        h.hi = cst_mix64(h.hi, cc.hi);
    }
    a->trivia_hash[n] = h;
    return h;
}

void cst_rehash_all(CstArena *a) {
    if (a->count == 0)
        return;
    cst_compute_struct(a, cst_root(a));
    cst_compute_trivia(a, cst_root(a));
}

CstHash cst_struct_hash(const CstArena *a, CstLocal n) { return a->struct_hash[n]; }
CstHash cst_trivia_hash(const CstArena *a, CstLocal n) { return a->trivia_hash[n]; }

/* Frontier-scoped rehash (PLAN §3.1). v1: mark touched + ancestors dirty, then a
 * single post-order pass recomputes only dirty nodes from children's current
 * hashes. Correct; the O(frontier) cost model is a 5B concern. */
static void cst_rehash_dirty(CstArena *a, CstLocal n, const uint8_t *dirty) {
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c])
        cst_rehash_dirty(a, c, dirty);
    if (!dirty[n])
        return;
    if (a->kind[n] == CST_Token) {
        uint32_t tl;
        const uint8_t *tb = cst_leaf_tok_bytes(a, n, &tl);
        a->struct_hash[n] = cst_hash_leaf(a->tok_kind[n], tb, tl);
        return;
    }
    CstHash stackbuf[16];
    CstHash *ch = stackbuf;
    uint32_t cap = 16, cnt = 0;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
        if (cnt >= cap) {
            cap *= 2;
            CstHash *nw = cst_xrealloc(ch == stackbuf ? NULL : ch, cap * sizeof *nw);
            if (ch == stackbuf)
                memcpy(nw, stackbuf, cnt * sizeof *nw);
            ch = nw;
        }
        ch[cnt++] = a->struct_hash[c];
    }
    a->struct_hash[n] = cst_hash_internal(a->kind[n], ch, cnt);
    if (ch != stackbuf)
        free(ch);
}

void cst_rehash_frontier(CstArena *a, const CstLocal *touched, uint32_t n) {
    if (a->count == 0)
        return;
    uint8_t *dirty = cst_xrealloc(NULL, a->count);
    memset(dirty, 0, a->count);
    uint32_t i;
    for (i = 0; i < n; i++) {
        CstLocal m = touched[i];
        while (m != CST_NONE && !dirty[m]) {
            dirty[m] = 1;
            m = a->parent[m];
        }
    }
    cst_rehash_dirty(a, cst_root(a), dirty);
    free(dirty);
}

/* ================================================================== *
 * Geometry & offset->node index (slice D, PLAN §1/§2/§5)
 * ================================================================== */

uint32_t cst_abs_offset(const CstArena *a, CstLocal n) {
    uint32_t off = 0;
    while (n != CST_NONE) {
        CstLocal parent = a->parent[n];
        if (parent != CST_NONE) {
            CstLocal c;
            for (c = a->first_child[parent]; c != n; c = a->next_sib[c])
                off += a->width[c];
        }
        n = parent;
    }
    return off;
}

static void cst_index_walk(CstArena *a, CstLocal n, uint32_t base,
                           uint32_t *pos) {
    if (a->kind[n] == CST_Token) {
        a->index[*pos].start = base;
        a->index[*pos].node = n;
        (*pos)++;
        return;
    }
    uint32_t off = base;
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
        cst_index_walk(a, c, off, pos);
        off += a->width[c];
    }
}

void cst_index_build(CstArena *a) {
    /* Count leaves. */
    uint32_t leaves = 0, i;
    for (i = 0; i < a->count; i++)
        if (a->kind[i] == CST_Token)
            leaves++;
    a->index = cst_xrealloc(a->index, (leaves ? leaves : 1) * sizeof *a->index);
    uint32_t pos = 0;
    if (a->count)
        cst_index_walk(a, cst_root(a), 0, &pos);
    a->index_count = pos;
    a->index_valid = 1;
}

CstLocal cst_node_at(const CstArena *a, uint32_t abs_off) {
    if (!a->index_valid || a->index_count == 0)
        return CST_NONE;
    /* Binary search for the leaf whose [start, start+width) contains abs_off. */
    uint32_t lo = 0, hi = a->index_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        uint32_t start = a->index[mid].start;
        uint32_t end = start + a->width[a->index[mid].node];
        if (abs_off < start)
            hi = mid;
        else if (abs_off >= end)
            lo = mid + 1;
        else
            return a->index[mid].node;
    }
    return CST_NONE;
}

/* ================================================================== *
 * Symbol refs (slice I, PLAN §1 Symbols)
 * ================================================================== */

void cst_set_sym_ref(CstArena *a, CstLocal use, CstId def) {
    a->sym_ref[use] = def;
}
CstId cst_sym_ref(const CstArena *a, CstLocal use) { return a->sym_ref[use]; }

/* ================================================================== *
 * Reflection + serialization (slice E, PLAN §8.1/§8.6)
 * ================================================================== */

static size_t cst_reflect_walk(const CstArena *a, CstLocal n, uint8_t *out,
                               size_t cap, size_t pos) {
    if (a->kind[n] == CST_Token) {
        uint32_t len = a->leaf_len[n];
        if (out && pos + len <= cap)
            memcpy(out + pos, a->src + a->leaf_off[n], len);
        return pos + len;
    }
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c])
        pos = cst_reflect_walk(a, c, out, cap, pos);
    return pos;
}

size_t cst_reflect(const CstArena *a, CstLocal root, uint8_t *out, size_t cap) {
    if (a->count == 0)
        return 0;
    return cst_reflect_walk(a, root, out, cap, 0);
}

/* Snapshot format (PLAN §1 Persistence): versioned header + section columns.
 * Never a raw dump — magic + version + endianness guard cross-version reads. */
#define CST_MAGIC 0x5453434Du /* 'MCST' little-endian */
#define CST_VERSION 1u
#define CST_ENDIAN_TAG 0x01020304u

typedef struct CstHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t endian;
    uint32_t file_id;
    uint32_t node_count;
    uint32_t trivia_count;
    uint32_t src_len;
    uint32_t reserved;
} CstHeader;

static int cst_wr(FILE *f, const void *p, size_t n) {
    return fwrite(p, 1, n, f) == n;
}

int cst_snapshot_save(const CstArena *a, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    CstHeader h;
    h.magic = CST_MAGIC;
    h.version = CST_VERSION;
    h.endian = CST_ENDIAN_TAG;
    h.file_id = a->file_id;
    h.node_count = a->count;
    h.trivia_count = a->trivia_count;
    h.src_len = a->src_len;
    h.reserved = 0;
    int ok = cst_wr(f, &h, sizeof h);
#define W(field) ok = ok && cst_wr(f, a->field, (size_t)a->count * sizeof *a->field)
    W(kind);
    W(parent);
    W(first_child);
    W(last_child);
    W(next_sib);
    W(width);
    W(struct_hash);
    W(trivia_hash);
    W(sym_ref);
    W(slot_key);
    W(tok_kind);
    W(leaf_off);
    W(leaf_len);
    W(tok_rel);
    W(triv_start);
    W(triv_count);
#undef W
    ok = ok && cst_wr(f, a->trivia, (size_t)a->trivia_count * sizeof *a->trivia);
    ok = ok && cst_wr(f, a->src, a->src_len);
    fclose(f);
    return ok ? 0 : -1;
}

static int cst_rd(FILE *f, void *p, size_t n) {
    return fread(p, 1, n, f) == n;
}

CstArena *cst_snapshot_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    CstHeader h;
    if (!cst_rd(f, &h, sizeof h) || h.magic != CST_MAGIC ||
        h.version != CST_VERSION || h.endian != CST_ENDIAN_TAG) {
        fclose(f);
        return NULL; /* version/endian skew rejected cleanly (PLAN §8.6) */
    }
    CstArena *a = cst_arena_new(h.file_id);
    cst_grow(a, h.node_count ? h.node_count : 1);
    a->count = h.node_count;
    int ok = 1;
#define R(field) ok = ok && cst_rd(f, a->field, (size_t)a->count * sizeof *a->field)
    R(kind);
    R(parent);
    R(first_child);
    R(last_child);
    R(next_sib);
    R(width);
    R(struct_hash);
    R(trivia_hash);
    R(sym_ref);
    R(slot_key);
    R(tok_kind);
    R(leaf_off);
    R(leaf_len);
    R(tok_rel);
    R(triv_start);
    R(triv_count);
#undef R
    if (h.trivia_count) {
        a->trivia = cst_xrealloc(NULL, (size_t)h.trivia_count * sizeof *a->trivia);
        a->trivia_cap = h.trivia_count;
        a->trivia_count = h.trivia_count;
        ok = ok && cst_rd(f, a->trivia, (size_t)h.trivia_count * sizeof *a->trivia);
    }
    if (h.src_len) {
        a->src = cst_xrealloc(NULL, h.src_len);
        a->src_cap = h.src_len;
        a->src_len = h.src_len;
        ok = ok && cst_rd(f, a->src, h.src_len);
    }
    fclose(f);
    if (!ok) {
        cst_arena_free(a);
        return NULL;
    }
    cst_index_build(a);
    return a;
}

/* ================================================================== *
 * Recording-hook skeleton (slice H, PLAN §6)
 * A single current arena + build stack; the compiler never sees the arena.
 * Fleshed out during Weave 1. Provided here so mccgen.c/mccpp.c can link.
 * ================================================================== */

/* ================================================================== *
 * Whole-tree validation (PLAN §8.1/§8.2/§8.3) — used by the corpus gate.
 * Checks round-trip reflection, width tiling, and offset->node lookup.
 * Returns 0 on success; else writes a short reason into msg.
 * ================================================================== */
static int cst_check_tiling(const CstArena *a, CstLocal n) {
    if (a->kind[n] == CST_Token)
        return a->width[n] == a->leaf_len[n];
    uint32_t sum = 0;
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
        if (!cst_check_tiling(a, c))
            return 0;
        sum += a->width[c];
    }
    return sum == a->width[n];
}

int cst_validate(const CstArena *a, char *msg, size_t msgcap) {
    if (!a || a->count == 0) {
        snprintf(msg, msgcap, "empty tree");
        return 1;
    }
    CstLocal root = cst_root(a);
    /* 1. Round-trip (§8.1). */
    size_t need = cst_reflect(a, root, NULL, 0);
    if (need != a->src_len) {
        snprintf(msg, msgcap, "reflect size %zu != src %u", need, a->src_len);
        return 1;
    }
    uint8_t *buf = cst_xrealloc(NULL, need ? need : 1);
    cst_reflect(a, root, buf, need);
    int bad = (need && memcmp(buf, a->src, need) != 0);
    free(buf);
    if (bad) {
        snprintf(msg, msgcap, "reflect bytes differ");
        return 1;
    }
    /* 2. Tiling / span coverage (§8.2). */
    if (a->width[root] != a->src_len) {
        snprintf(msg, msgcap, "root width %u != src %u", a->width[root],
                 a->src_len);
        return 1;
    }
    if (!cst_check_tiling(a, root)) {
        snprintf(msg, msgcap, "width tiling violated");
        return 1;
    }
    /* 3. Bidirectional offset->node lookup (§8.3), sampled. */
    uint32_t step = a->src_len / 256 + 1, off;
    for (off = 0; off < a->src_len; off += step) {
        CstLocal n = cst_node_at(a, off);
        if (n == CST_NONE) {
            snprintf(msg, msgcap, "offset %u -> no node", off);
            return 1;
        }
        uint32_t start = cst_abs_offset(a, n);
        if (off < start || off >= start + a->width[n]) {
            snprintf(msg, msgcap, "offset %u outside its node span", off);
            return 1;
        }
    }
    snprintf(msg, msgcap, "ok");
    return 0;
}

/* --- deferred capture state (slice H) -----------------------------------
 * mcc is a single-pass parser with one token of lookahead, so when a grammar
 * function brackets a construct with cst_hook_open/close its first token has
 * ALREADY been lexed and captured. We therefore DON'T build the tree live.
 * Instead we record a flat, source-ordered leaf list (which alone gives the
 * round-trip, PLAN §8.1) plus structural specs as leaf-index ranges, and
 * materialize the nested tree in cst_hook_end — resolving the lookahead by the
 * rule: a node opened while `tok` is its first token spans leaves
 * [open_leaf_count - 1, close_leaf_count - 1). */
static CstArena *cst_current;

typedef struct CstLeafSpec {
    uint16_t kind;
    uint32_t off, len;
} CstLeafSpec;

typedef struct CstNodeSpec {
    uint16_t kind;
    uint32_t first_leaf, last_leaf;
    int32_t parent;
    int32_t child_head, child_tail, sib; /* materialization links */
} CstNodeSpec;

static CstLeafSpec *cst_lbuf;
static uint32_t cst_lcount, cst_lcap;
static CstNodeSpec *cst_sbuf;
static uint32_t cst_scount, cst_scap;
static int32_t *cst_sstack;
static uint32_t cst_sstop, cst_sscap;

/* Symbol refs (slice I): last-seen def offset per identifier token value, plus a
 * buffer of (use,def) source-offset pairs resolved to node-ids at end. v1 is
 * last-declaration-wins (no scope stack); shadowing is a documented limitation. */
static uint32_t *cst_defoff;
static uint32_t cst_defoff_cap;
static struct CstUse {
    uint32_t use_off, def_off;
} *cst_uses;
static uint32_t cst_uses_count, cst_uses_cap;
#define CST_OFF_NONE 0xffffffffu

uint32_t cst_cur_tok_off(void) {
    return cst_lcount ? cst_lbuf[cst_lcount - 1].off : 0;
}

void cst_hook_def(int v, uint32_t off) {
    if (!cst_current || v < 0)
        return;
    if ((uint32_t)v >= cst_defoff_cap) {
        uint32_t nc = cst_defoff_cap ? cst_defoff_cap : 1024;
        while ((uint32_t)v >= nc)
            nc *= 2;
        cst_defoff = cst_xrealloc(cst_defoff, nc * sizeof *cst_defoff);
        for (uint32_t i = cst_defoff_cap; i < nc; i++)
            cst_defoff[i] = CST_OFF_NONE;
        cst_defoff_cap = nc;
    }
    cst_defoff[v] = off;
}

void cst_hook_use(int v, uint32_t off) {
    if (!cst_current || v < 0 || (uint32_t)v >= cst_defoff_cap)
        return;
    uint32_t d = cst_defoff[v];
    if (d == CST_OFF_NONE)
        return;
    if (cst_uses_count >= cst_uses_cap) {
        cst_uses_cap = cst_uses_cap ? cst_uses_cap * 2 : 512;
        cst_uses = cst_xrealloc(cst_uses, cst_uses_cap * sizeof *cst_uses);
    }
    cst_uses[cst_uses_count].use_off = off;
    cst_uses[cst_uses_count].def_off = d;
    cst_uses_count++;
}

static int cst_slurp(CstArena *a, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f)
        return -1;
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        cst_own_file(a, filename, buf, n);
    fclose(f);
    return 0;
}

static int32_t cst_spec_push(uint16_t kind, uint32_t first_leaf) {
    if (cst_scount >= cst_scap) {
        cst_scap = cst_scap ? cst_scap * 2 : 256;
        cst_sbuf = cst_xrealloc(cst_sbuf, cst_scap * sizeof *cst_sbuf);
    }
    int32_t si = (int32_t)cst_scount++;
    CstNodeSpec *s = &cst_sbuf[si];
    s->kind = kind;
    s->first_leaf = first_leaf;
    s->last_leaf = first_leaf;
    s->parent = cst_sstop ? cst_sstack[cst_sstop - 1] : -1;
    s->child_head = s->child_tail = s->sib = -1;
    if (cst_sstop >= cst_sscap) {
        cst_sscap = cst_sscap ? cst_sscap * 2 : 256;
        cst_sstack = cst_xrealloc(cst_sstack, cst_sscap * sizeof *cst_sstack);
    }
    cst_sstack[cst_sstop++] = si;
    return si;
}

void cst_hook_begin(const char *filename) {
    if (cst_current)
        cst_arena_free(cst_current);
    cst_current = cst_arena_new(0);
    cst_slurp(cst_current, filename);
    cst_lcount = cst_scount = cst_sstop = 0;
    cst_uses_count = 0;
    for (uint32_t i = 0; i < cst_defoff_cap; i++)
        cst_defoff[i] = CST_OFF_NONE;
    cst_spec_push(CST_TranslationUnit, 0); /* root */
}

void cst_hook_token(uint32_t start, uint32_t end) {
    if (!cst_current || end <= start)
        return;
    if (cst_lcount >= cst_lcap) {
        cst_lcap = cst_lcap ? cst_lcap * 2 : 1024;
        cst_lbuf = cst_xrealloc(cst_lbuf, cst_lcap * sizeof *cst_lbuf);
    }
    cst_lbuf[cst_lcount].kind = CST_Token;
    cst_lbuf[cst_lcount].off = start;
    cst_lbuf[cst_lcount].len = end - start;
    cst_lcount++;
}

void cst_hook_open(uint16_t kind) {
    if (!cst_current)
        return;
    /* The current lookahead token is this construct's first token. */
    uint32_t first = cst_lcount ? cst_lcount - 1 : 0;
    cst_spec_push(kind, first);
}

/* Mark the current leaf position, for a node whose start is only known to be a
 * node later (left-recursive expressions, PLAN §2). */
uint32_t cst_mark(void) { return cst_lcount ? cst_lcount - 1 : 0; }

/* Open a node retroactively spanning from a previously-taken mark. */
void cst_hook_open_at(uint16_t kind, uint32_t first_leaf) {
    if (!cst_current)
        return;
    cst_spec_push(kind, first_leaf);
}

/* Number of source leaves captured so far. */
uint32_t cst_leafcount(void) { return cst_lcount; }

/* Record a node with an explicit half-open leaf range [first,last) — used for a
 * macro invocation, whose written name/args span is known only after expansion
 * (slice J, PLAN §4). Empty ranges are dropped by cst_nest_specs. */
void cst_hook_wrap(uint16_t kind, uint32_t first_leaf, uint32_t last_leaf) {
    if (!cst_current || last_leaf <= first_leaf)
        return;
    int32_t si = cst_spec_push(kind, first_leaf);
    cst_sbuf[si].last_leaf = last_leaf;
    cst_sstop--; /* not a stack bracket; its range is fully specified */
}

void cst_hook_close(void) {
    if (!cst_current || cst_sstop <= 1) /* never pop the TU root here */
        return;
    int32_t si = cst_sstack[--cst_sstop];
    /* Exclude the lookahead token belonging to the next construct. */
    cst_sbuf[si].last_leaf = cst_lcount ? cst_lcount - 1 : 0;
}

/* Leading-trivia length of a leaf span: the run of whitespace and comments
 * before the token (slice G). Excluded from the structural hash (PLAN §3) so
 * whitespace/comment-only edits don't perturb H_s. */
static uint32_t cst_leading_trivia(const uint8_t *src, uint32_t off, uint32_t len) {
    uint32_t i = 0;
    while (i < len) {
        uint8_t c = src[off + i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
            c == '\v') {
            i++;
        } else if (c == '/' && i + 1 < len && src[off + i + 1] == '/') {
            i += 2;
            while (i < len && src[off + i] != '\n')
                i++;
        } else if (c == '/' && i + 1 < len && src[off + i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(src[off + i] == '*' && src[off + i + 1] == '/'))
                i++;
            if (i + 1 < len)
                i += 2;
        } else {
            break;
        }
    }
    return i;
}

static void cst_emit_leaf(uint32_t i) {
    uint32_t off = cst_lbuf[i].off, len = cst_lbuf[i].len;
    uint32_t tr = cst_leading_trivia(cst_current->src, off, len);
    CstTrivia tv;
    tv.kind = CST_TRIV_WS;
    tv.offset = 0;
    tv.length = tr;
    cst_leaf(cst_current, cst_lbuf[i].kind, off, len, tr ? &tv : NULL, tr ? 1 : 0);
}

/* Order specs for range-based nesting: first_leaf asc, then wider range first
 * (last_leaf desc), then — for exactly-coincident ranges — later-opened first
 * (id desc). A grouping wrapper (Initializer/Declaration/…) is opened *after*
 * the sub-expression it contains (retroactive range-wrap), so when a wrapper's
 * span coincides byte-for-byte with its single child (e.g. `int c = (int)a;`
 * where the Initializer equals the Cast) the higher id is the semantic parent
 * and must sort first so the containment stack makes it the parent. */
static int cst_spec_cmp(const void *pa, const void *pb) {
    uint32_t a = *(const uint32_t *)pa, b = *(const uint32_t *)pb;
    if (cst_sbuf[a].first_leaf != cst_sbuf[b].first_leaf)
        return cst_sbuf[a].first_leaf < cst_sbuf[b].first_leaf ? -1 : 1;
    if (cst_sbuf[a].last_leaf != cst_sbuf[b].last_leaf)
        return cst_sbuf[a].last_leaf > cst_sbuf[b].last_leaf ? -1 : 1;
    return a > b ? -1 : (a < b ? 1 : 0);
}

/* Rebuild spec parent/child links purely from [first_leaf,last_leaf) containment
 * (not the open-time parent), so retroactively-opened expression nodes nest
 * correctly. All grammar constructs are either disjoint or nested, never partial-
 * overlapping, so a stack over the sorted specs yields the true tree. */
static void cst_nest_specs(void) {
    uint32_t n = cst_scount, k, m = 0;
    uint32_t *order = cst_xrealloc(NULL, (n ? n : 1) * sizeof *order);
    int32_t *stk = cst_xrealloc(NULL, (n ? n : 1) * sizeof *stk);
    for (k = 0; k < n; k++) {
        cst_sbuf[k].parent = -1;
        cst_sbuf[k].child_head = cst_sbuf[k].child_tail = cst_sbuf[k].sib = -1;
        /* Drop empty specs (first==last): retroactive wraps around macro-
         * expanded expressions capture no source leaves and would otherwise
         * pile up into a pathological linear chain. Keep the root always. */
        if (k == 0 || cst_sbuf[k].last_leaf > cst_sbuf[k].first_leaf)
            order[m++] = k;
    }
    n = m;
    qsort(order, n, sizeof *order, cst_spec_cmp);
    uint32_t top = 0;
    for (k = 0; k < n; k++) {
        int32_t si = (int32_t)order[k];
        int overlap = 0;
        while (top > 0) {
            int32_t tp = stk[top - 1];
            if (cst_sbuf[tp].first_leaf <= cst_sbuf[si].first_leaf &&
                cst_sbuf[si].last_leaf <= cst_sbuf[tp].last_leaf)
                break;
            /* tp does not contain si. If tp still extends past si's start, si
             * is neither disjoint-after tp nor nested in it — a *partial*
             * overlap, which the disjoint-or-nested invariant forbids. This
             * arises when an independently-recorded span (e.g. a parser
             * Declaration whose start mark is stale across an #include) straddles
             * a PP-boundary node. Drop si rather than let cst_materialize tile a
             * leaf under two siblings (which would break round-trip §8.1). */
            if (cst_sbuf[tp].last_leaf > cst_sbuf[si].first_leaf)
                overlap = 1;
            top--;
        }
        if (overlap) {
            cst_sbuf[si].parent = -1; /* not linked → never materialized */
            continue;
        }
        int32_t parent = top > 0 ? stk[top - 1] : -1;
        cst_sbuf[si].parent = parent;
        if (parent >= 0) {
            if (cst_sbuf[parent].child_head < 0)
                cst_sbuf[parent].child_head = si;
            else
                cst_sbuf[cst_sbuf[parent].child_tail].sib = si;
            cst_sbuf[parent].child_tail = si;
        }
        stk[top++] = si;
    }
    free(order);
    free(stk);
}

/* Materialize spec `si` into the live SoA arena, interleaving its own leaves
 * with descendant specs in source order. Returns the created node id. */
static CstLocal cst_materialize(int32_t si) {
    CstNodeSpec *s = &cst_sbuf[si];
    CstLocal node = cst_node_open(cst_current, s->kind);
    uint32_t i = s->first_leaf;
    int32_t cj = s->child_head;
    while (cj >= 0) {
        uint32_t cfirst = cst_sbuf[cj].first_leaf;
        for (; i < cfirst && i < s->last_leaf; i++)
            cst_emit_leaf(i);
        cst_materialize(cj);
        if (cst_sbuf[cj].last_leaf > i)
            i = cst_sbuf[cj].last_leaf;
        cj = cst_sbuf[cj].sib;
    }
    for (; i < s->last_leaf; i++)
        cst_emit_leaf(i);
    cst_node_close(cst_current, node);
    return node;
}

CstArena *cst_hook_end(void) {
    CstArena *a = cst_current;
    if (a) {
        /* Debug tripwire (PLAN §6): every grammar cst_hook_open must have a
         * matching cst_hook_close, so only the TU root remains on the stack.
         * Verified balanced across the corpus for the bracketed single-exit
         * functions (block/type_decl/struct_decl). */
        CST_ASSERT(cst_sstop == 1);
        /* Close any specs left open (defensive), then the TU root spans all. */
        while (cst_sstop > 1)
            cst_hook_close();
        cst_sbuf[0].last_leaf = cst_lcount;
        /* Pad an uncaptured tail leaf so the tree tiles the whole source. */
        uint32_t tail = cst_lcount ? cst_lbuf[cst_lcount - 1].off +
                                         cst_lbuf[cst_lcount - 1].len
                                   : 0;
        if (tail < a->src_len) {
            cst_hook_token(tail, a->src_len);
            cst_sbuf[0].last_leaf = cst_lcount;
        }
        /* Nest specs by leaf-range containment (handles retroactive wraps). */
        cst_nest_specs();
        cst_materialize(0);
        cst_rehash_all(a);
        cst_index_build(a);
        /* Resolve buffered use->def source offsets into node-ids (slice I). */
        uint32_t u;
        for (u = 0; u < cst_uses_count; u++) {
            CstLocal un = cst_node_at(a, cst_uses[u].use_off);
            CstLocal dn = cst_node_at(a, cst_uses[u].def_off);
            if (un != CST_NONE && dn != CST_NONE && un != dn)
                cst_set_sym_ref(a, un, cst_id(a->file_id, dn));
        }
    }
    cst_current = NULL;
    return a;
}

void cst_hook_leaf(uint16_t tok_kind, uint32_t byte_off, uint32_t len) {
    (void)tok_kind;
    cst_hook_token(byte_off, byte_off + len);
}

#pragma pop_macro("free")
#pragma pop_macro("realloc")
#pragma pop_macro("malloc")

#endif /* CONFIG_MCC_CST */
