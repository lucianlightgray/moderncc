#if defined(CONFIG_MCC_CST) && CONFIG_MCC_CST

#include "mcccst.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CST_ASSERT
#include <assert.h>
#define CST_ASSERT(x) assert(x)
#endif

#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef realloc
#undef free

#define CST_ID_NONE ((CstId)0xffffffffffffffffull)


struct CstArena {
    uint32_t file_id;

    uint16_t *kind;
    CstLocal *parent;
    CstLocal *first_child;
    CstLocal *last_child;
    CstLocal *next_sib;
    uint32_t *width;
    CstHash  *struct_hash;
    CstHash  *trivia_hash;
    uint64_t *sym_ref;
    uint32_t *slot_key;

    uint16_t *tok_kind;
    uint32_t *leaf_off;
    uint32_t *leaf_len;
    uint32_t *tok_rel;
    uint32_t *triv_start;
    uint32_t *triv_count;

    CstLocal count;
    CstLocal cap;

    CstTrivia *trivia;
    uint32_t trivia_count, trivia_cap;

    uint8_t *src;
    uint32_t src_len, src_cap;

    CstLocal *stack;
    uint32_t stack_top, stack_cap;

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
    CstLocal parent = a->parent[node];
    if (parent != CST_NONE)
        a->width[parent] += a->width[node];
}

static int cst_is_leaf_kind(uint16_t k) {
    return k == CST_Token || k == CST_Comment;
}

static CstLocal cst_leaf_kinded(CstArena *a, uint16_t node_kind, uint16_t tok_kind,
                                uint32_t off, uint32_t len, const CstTrivia *trivia,
                                uint32_t ntrivia) {
    CstLocal parent = a->stack_top ? a->stack[a->stack_top - 1] : CST_NONE;
    CstLocal n = cst_alloc_node(a, node_kind);
    cst_link_child(a, parent, n);

    uint32_t tok_rel = 0, i;
    uint32_t ts = a->trivia_count;
    for (i = 0; i < ntrivia; i++) {
        if (a->trivia_count >= a->trivia_cap) {
            a->trivia_cap = a->trivia_cap ? a->trivia_cap * 2 : 32;
            a->trivia = cst_xrealloc(a->trivia, a->trivia_cap * sizeof *a->trivia);
        }
        a->trivia[a->trivia_count++] = trivia[i];
        tok_rel += trivia[i].length;
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

CstLocal cst_leaf(CstArena *a, uint16_t tok_kind, uint32_t off, uint32_t len,
                  const CstTrivia *trivia, uint32_t ntrivia) {
    return cst_leaf_kinded(a, CST_Token, tok_kind, off, len, trivia, ntrivia);
}

uint16_t cst_kind(const CstArena *a, CstLocal n) { return a->kind[n]; }
CstLocal cst_parent(const CstArena *a, CstLocal n) { return a->parent[n]; }
CstLocal cst_first_child(const CstArena *a, CstLocal n) { return a->first_child[n]; }
CstLocal cst_next_sib(const CstArena *a, CstLocal n) { return a->next_sib[n]; }
uint32_t cst_width(const CstArena *a, CstLocal n) { return a->width[n]; }
CstLocal cst_node_count(const CstArena *a) { return a->count; }
CstLocal cst_root(const CstArena *a) { return a->count ? 0 : CST_NONE; }


uint32_t cst_own_file(CstArena *a, const char *name, const uint8_t *bytes,
                      size_t n) {
    (void)name;
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
    h.lo = cst_hash_bytes(0xC0FFEEull ^ ((uint64_t)tok_kind << 1), bytes, len);
    h.hi = cst_hash_bytes(0x5EED1234ull ^ ((uint64_t)tok_kind << 7 | 1), bytes, len);
    return h;
}

CstHash cst_hash_internal(uint16_t kind, const CstHash *child, uint32_t n) {
    CstHash h;
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
    CstHash stackbuf[16];
    CstHash *ch = stackbuf;
    uint32_t cap = 16, cnt = 0;
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
        if (a->kind[c] == CST_Comment) {
            a->struct_hash[c].lo = a->struct_hash[c].hi = 0;
            continue;
        }
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
    } else if (a->kind[n] == CST_Comment) {
        h.lo = cst_mix64(h.lo, ((uint64_t)CST_TRIV_BLOCK_COMMENT << 32) | a->leaf_len[n]);
        h.hi = cst_hash_bytes(h.hi, a->src + a->leaf_off[n], a->leaf_len[n]);
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

static void cst_rehash_dirty(CstArena *a, CstLocal n, const uint8_t *dirty) {
    CstLocal c;
    for (c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c])
        cst_rehash_dirty(a, c, dirty);
    if (!dirty[n])
        return;
    if (a->kind[n] == CST_Comment) {
        a->struct_hash[n].lo = a->struct_hash[n].hi = 0;
        return;
    }
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
        if (a->kind[c] == CST_Comment)
            continue;
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
    if (cst_is_leaf_kind(a->kind[n])) {
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
    uint32_t leaves = 0, i;
    for (i = 0; i < a->count; i++)
        if (cst_is_leaf_kind(a->kind[i]))
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


void cst_set_sym_ref(CstArena *a, CstLocal use, CstId def) {
    a->sym_ref[use] = def;
}
CstId cst_sym_ref(const CstArena *a, CstLocal use) { return a->sym_ref[use]; }


void cst_mark_branch(CstArena *a, CstLocal node, uint32_t branch_ord) {
    a->slot_key[node] = branch_ord + 1;
}
uint32_t cst_branch_ord(const CstArena *a, CstLocal node) {
    uint32_t s = a->slot_key[node];
    return s ? s - 1 : 0;
}
static int cst_is_branch(const CstArena *a, CstLocal node) {
    return a->slot_key[node] != 0;
}

void cst_set_include_target(CstArena *a, CstLocal node, uint32_t template_id) {
    a->sym_ref[node] = cst_id(template_id, 0);
}
uint32_t cst_include_target(const CstArena *a, CstLocal node) {
    return cst_id_file(a->sym_ref[node]);
}

struct CstStore {
    struct {
        CstHash key;
        CstArena *tmpl;
    } *ent;
    uint32_t count, cap;
};

CstStore *cst_store_new(void) {
    CstStore *s = cst_xrealloc(NULL, sizeof *s);
    memset(s, 0, sizeof *s);
    return s;
}

void cst_store_free(CstStore *s) {
    if (!s)
        return;
    for (uint32_t i = 0; i < s->count; i++)
        cst_arena_free(s->ent[i].tmpl);
    free(s->ent);
    free(s);
}

uint32_t cst_store_count(const CstStore *s) { return s->count; }

CstArena *cst_store_get(const CstStore *s, uint32_t id) {
    return id < s->count ? s->ent[id].tmpl : NULL;
}

uint32_t cst_store_intern(CstStore *s, CstArena *tmpl) {
    CstHash key = cst_struct_hash(tmpl, cst_root(tmpl));
    for (uint32_t i = 0; i < s->count; i++) {
        if (!cst_hash_eq(s->ent[i].key, key))
            continue;
#if !defined(NDEBUG)
        {
            CstArena *ex = s->ent[i].tmpl;
            size_t na = cst_render_identity(ex, NULL, 0);
            size_t nb = cst_render_identity(tmpl, NULL, 0);
            int bad = na != nb;
            if (!bad && na) {
                uint8_t *ba = cst_xrealloc(NULL, na), *bb = cst_xrealloc(NULL, nb);
                cst_render_identity(ex, ba, na);
                cst_render_identity(tmpl, bb, nb);
                bad = memcmp(ba, bb, na) != 0;
                free(ba);
                free(bb);
            }
            if (bad) {
                fprintf(stderr,
                        "mcccst: FATAL hash collision in the content-addressed "
                        "store: two distinct file bodies share H_s "
                        "%016llx%016llx. The cst_hash_* formula must be fixed.\n",
                        (unsigned long long)key.hi, (unsigned long long)key.lo);
                abort();
            }
        }
#endif
        cst_arena_free(tmpl);
        return i;
    }
    if (s->count >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->ent = cst_xrealloc(s->ent, s->cap * sizeof *s->ent);
    }
    s->ent[s->count].key = key;
    s->ent[s->count].tmpl = tmpl;
    return s->count++;
}

struct CstBinding {
    uint32_t template_id;
    struct {
        CstLocal cond;
        uint32_t which;
    } *sel;
    uint32_t nsel, selcap;
    CstBinding **inc;
    uint32_t ninc, inccap;
};

CstBinding *cst_binding_new(uint32_t template_id) {
    CstBinding *b = cst_xrealloc(NULL, sizeof *b);
    memset(b, 0, sizeof *b);
    b->template_id = template_id;
    return b;
}

void cst_binding_free(CstBinding *b) {
    if (!b)
        return;
    for (uint32_t i = 0; i < b->ninc; i++)
        cst_binding_free(b->inc[i]);
    free(b->inc);
    free(b->sel);
    free(b);
}

uint32_t cst_binding_template(const CstBinding *b) { return b->template_id; }

void cst_binding_select(CstBinding *b, CstLocal cond, uint32_t which) {
    for (uint32_t i = 0; i < b->nsel; i++)
        if (b->sel[i].cond == cond) {
            b->sel[i].which = which;
            return;
        }
    if (b->nsel >= b->selcap) {
        b->selcap = b->selcap ? b->selcap * 2 : 8;
        b->sel = cst_xrealloc(b->sel, b->selcap * sizeof *b->sel);
    }
    b->sel[b->nsel].cond = cond;
    b->sel[b->nsel].which = which;
    b->nsel++;
}

uint32_t cst_binding_selected(const CstBinding *b, CstLocal cond) {
    for (uint32_t i = 0; i < b->nsel; i++)
        if (b->sel[i].cond == cond)
            return b->sel[i].which;
    return 0;
}

void cst_binding_add_include(CstBinding *b, CstBinding *child) {
    if (b->ninc >= b->inccap) {
        b->inccap = b->inccap ? b->inccap * 2 : 4;
        b->inc = cst_xrealloc(b->inc, b->inccap * sizeof *b->inc);
    }
    b->inc[b->ninc++] = child;
}

static size_t cst_emit_bytes(const CstArena *a, CstLocal n, uint8_t *out,
                             size_t cap, size_t pos) {
    uint32_t len = a->leaf_len[n];
    if (out && pos + len <= cap)
        memcpy(out + pos, a->src + a->leaf_off[n], len);
    return pos + len;
}

static size_t cst_render_node(const CstStore *s, const CstArena *a, CstLocal n,
                              const CstBinding *b, uint32_t *inc_cursor,
                              uint8_t *out, size_t cap, size_t pos) {
    uint16_t k = a->kind[n];
    if (cst_is_leaf_kind(k))
        return cst_emit_bytes(a, n, out, cap, pos);

    if (b && k == CST_IncludeDirective) {
        if (*inc_cursor < b->ninc) {
            const CstBinding *child = b->inc[(*inc_cursor)++];
            const CstArena *ct = cst_store_get(s, child->template_id);
            uint32_t cc = 0;
            return cst_render_node(s, ct, cst_root(ct), child, &cc, out, cap, pos);
        }
    }

    if (b && k == CST_PPConditional) {
        uint32_t sel = cst_binding_selected(b, n);
        for (CstLocal c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c]) {
            if (!cst_is_branch(a, c))
                continue;
            if (cst_branch_ord(a, c) != sel)
                continue;
            pos = cst_render_node(s, a, c, b, inc_cursor, out, cap, pos);
        }
        return pos;
    }

    for (CstLocal c = a->first_child[n]; c != CST_NONE; c = a->next_sib[c])
        pos = cst_render_node(s, a, c, b, inc_cursor, out, cap, pos);
    return pos;
}

size_t cst_render(const CstStore *s, const CstBinding *b, uint8_t *out,
                  size_t cap) {
    const CstArena *t = cst_store_get(s, b->template_id);
    if (!t || t->count == 0)
        return 0;
    uint32_t cursor = 0;
    return cst_render_node(s, t, cst_root(t), b, &cursor, out, cap, 0);
}

size_t cst_render_identity(const CstArena *tmpl, uint8_t *out, size_t cap) {
    if (tmpl->count == 0)
        return 0;
    uint32_t cursor = 0;
    return cst_render_node(NULL, tmpl, cst_root(tmpl), NULL, &cursor, out, cap, 0);
}


static size_t cst_reflect_walk(const CstArena *a, CstLocal n, uint8_t *out,
                               size_t cap, size_t pos) {
    if (cst_is_leaf_kind(a->kind[n])) {
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

#define CST_MAGIC 0x5453434Du
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
        return NULL;
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


static int cst_check_tiling(const CstArena *a, CstLocal n) {
    if (cst_is_leaf_kind(a->kind[n]))
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
    if (a->width[root] != a->src_len) {
        snprintf(msg, msgcap, "root width %u != src %u", a->width[root],
                 a->src_len);
        return 1;
    }
    if (!cst_check_tiling(a, root)) {
        snprintf(msg, msgcap, "width tiling violated");
        return 1;
    }
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

static CstArena *cst_current;

typedef struct CstLeafSpec {
    uint16_t kind;
    uint32_t off, len;
} CstLeafSpec;

typedef struct CstNodeSpec {
    uint16_t kind;
    uint32_t first_leaf, last_leaf;
    int32_t parent;
    int32_t child_head, child_tail, sib;
} CstNodeSpec;

static CstLeafSpec *cst_lbuf;
static uint32_t cst_lcount, cst_lcap;
static CstNodeSpec *cst_sbuf;
static uint32_t cst_scount, cst_scap;
static int32_t *cst_sstack;
static uint32_t cst_sstop, cst_sscap;

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

static CstStore *cst_session_store;
static uint32_t *cst_inc_tmpl;
static uint32_t cst_inc_count, cst_inc_cap;

CstStore *cst_hook_store(void) { return cst_session_store; }

static int cst_line_cond(const uint8_t *s, uint32_t a, uint32_t b) {
    uint32_t i = a;
    while (i < b && (s[i] == ' ' || s[i] == '\t'))
        i++;
    if (i >= b || s[i] != '#')
        return 0;
    i++;
    while (i < b && (s[i] == ' ' || s[i] == '\t'))
        i++;
    uint32_t w = i;
    while (i < b && s[i] >= 'a' && s[i] <= 'z')
        i++;
    uint32_t n = i - w;
#define CST_KW(str) (n == sizeof(str) - 1 && memcmp(s + w, str, n) == 0)
    if (CST_KW("if") || CST_KW("ifdef") || CST_KW("ifndef"))
        return 1;
    if (CST_KW("else") || CST_KW("elif"))
        return 2;
    if (CST_KW("endif"))
        return 3;
#undef CST_KW
    return 0;
}

static void cst_build_sourcefile(CstArena *a, const uint8_t *src, uint32_t len) {
    cst_node_open(a, CST_TranslationUnit);
    struct {
        CstLocal cond;
        uint32_t brno;
        CstLocal branch;
    } st[64];
    int sp = 0;
    uint32_t p = 0;
    while (p < len) {
        uint32_t ls = p;
        while (p < len && src[p] != '\n')
            p++;
        if (p < len)
            p++;
        uint32_t n = p - ls;
        int dir = cst_line_cond(src, ls, p);
        if (dir == 1) {
            CstLocal cond = cst_node_open(a, CST_PPConditional);
            cst_leaf(a, 0, ls, n, NULL, 0);
            if (sp < 64) {
                st[sp].cond = cond;
                st[sp].brno = 0;
                st[sp].branch = CST_NONE;
                sp++;
            }
        } else if (dir == 2 && sp > 0) {
            if (st[sp - 1].branch != CST_NONE) {
                cst_node_close(a, st[sp - 1].branch);
                st[sp - 1].branch = CST_NONE;
            }
            cst_leaf(a, 0, ls, n, NULL, 0);
            st[sp - 1].brno++;
        } else if (dir == 3 && sp > 0) {
            if (st[sp - 1].branch != CST_NONE) {
                cst_node_close(a, st[sp - 1].branch);
                st[sp - 1].branch = CST_NONE;
            }
            cst_leaf(a, 0, ls, n, NULL, 0);
            cst_node_close(a, st[sp - 1].cond);
            sp--;
        } else {
            if (sp > 0 && st[sp - 1].branch == CST_NONE) {
                CstLocal bn = cst_node_open(a, CST_CompoundStmt);
                cst_mark_branch(a, bn, st[sp - 1].brno);
                st[sp - 1].branch = bn;
            }
            cst_leaf(a, 0, ls, n, NULL, 0);
        }
    }
    while (sp > 0) {
        if (st[sp - 1].branch != CST_NONE)
            cst_node_close(a, st[sp - 1].branch);
        cst_node_close(a, st[sp - 1].cond);
        sp--;
    }
    cst_node_close(a, cst_root(a));
}

void cst_hook_include(const char *path, int from_main_file) {
    if (!cst_current)
        return;
    if (!cst_session_store)
        cst_session_store = cst_store_new();
    CstArena *t = cst_arena_new(0);
    if (cst_slurp(t, path) != 0) {
        cst_arena_free(t);
        return;
    }
    uint32_t len = 0;
    const uint8_t *bytes = cst_source(t, &len);
    cst_build_sourcefile(t, bytes, len);
    cst_rehash_all(t);
    uint32_t id = cst_store_intern(cst_session_store, t);
    if (from_main_file) {
        if (cst_inc_count >= cst_inc_cap) {
            cst_inc_cap = cst_inc_cap ? cst_inc_cap * 2 : 16;
            cst_inc_tmpl = cst_xrealloc(cst_inc_tmpl,
                                        cst_inc_cap * sizeof *cst_inc_tmpl);
        }
        cst_inc_tmpl[cst_inc_count++] = id;
    }
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
    if (cst_session_store) {
        cst_store_free(cst_session_store);
        cst_session_store = NULL;
    }
    cst_inc_count = 0;
    for (uint32_t i = 0; i < cst_defoff_cap; i++)
        cst_defoff[i] = CST_OFF_NONE;
    cst_spec_push(CST_TranslationUnit, 0);
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
    uint32_t first = cst_lcount ? cst_lcount - 1 : 0;
    cst_spec_push(kind, first);
}

uint32_t cst_mark(void) { return cst_lcount ? cst_lcount - 1 : 0; }

void cst_hook_open_at(uint16_t kind, uint32_t first_leaf) {
    if (!cst_current)
        return;
    cst_spec_push(kind, first_leaf);
}

uint32_t cst_leafcount(void) { return cst_lcount; }

void cst_hook_wrap(uint16_t kind, uint32_t first_leaf, uint32_t last_leaf) {
    if (!cst_current || last_leaf <= first_leaf)
        return;
    int32_t si = cst_spec_push(kind, first_leaf);
    cst_sbuf[si].last_leaf = last_leaf;
    cst_sstop--;
}

void cst_hook_close(void) {
    if (!cst_current || cst_sstop <= 1)
        return;
    int32_t si = cst_sstack[--cst_sstop];
    cst_sbuf[si].last_leaf = cst_lcount ? cst_lcount - 1 : 0;
}

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
    const uint8_t *src = cst_current->src;
    uint32_t tr = cst_leading_trivia(src, off, len);

    uint32_t seg = off;
    uint32_t p = off, end = off + tr;
    while (p < end) {
        uint8_t c = src[p];
        if (c == '/' && p + 1 < end && src[p + 1] == '/') {
            p += 2;
            while (p < end && src[p] != '\n')
                p++;
            cst_leaf_kinded(cst_current, CST_Comment, 0, seg, p - seg, NULL, 0);
            seg = p;
        } else if (c == '/' && p + 1 < end && src[p + 1] == '*') {
            p += 2;
            while (p + 1 < end && !(src[p] == '*' && src[p + 1] == '/'))
                p++;
            if (p + 1 < end)
                p += 2;
            cst_leaf_kinded(cst_current, CST_Comment, 0, seg, p - seg, NULL, 0);
            seg = p;
        } else {
            p++;
        }
    }
    uint32_t tail = end - seg;
    CstTrivia tv;
    tv.kind = CST_TRIV_WS;
    tv.offset = 0;
    tv.length = tail;
    cst_leaf_kinded(cst_current, CST_Token, cst_lbuf[i].kind, seg,
                    (off + len) - seg, tail ? &tv : NULL, tail ? 1 : 0);
}

static int cst_spec_cmp(const void *pa, const void *pb) {
    uint32_t a = *(const uint32_t *)pa, b = *(const uint32_t *)pb;
    if (cst_sbuf[a].first_leaf != cst_sbuf[b].first_leaf)
        return cst_sbuf[a].first_leaf < cst_sbuf[b].first_leaf ? -1 : 1;
    if (cst_sbuf[a].last_leaf != cst_sbuf[b].last_leaf)
        return cst_sbuf[a].last_leaf > cst_sbuf[b].last_leaf ? -1 : 1;
    return a > b ? -1 : (a < b ? 1 : 0);
}

static void cst_nest_specs(void) {
    uint32_t n = cst_scount, k, m = 0;
    uint32_t *order = cst_xrealloc(NULL, (n ? n : 1) * sizeof *order);
    int32_t *stk = cst_xrealloc(NULL, (n ? n : 1) * sizeof *stk);
    for (k = 0; k < n; k++) {
        cst_sbuf[k].parent = -1;
        cst_sbuf[k].child_head = cst_sbuf[k].child_tail = cst_sbuf[k].sib = -1;
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
            if (cst_sbuf[tp].last_leaf > cst_sbuf[si].first_leaf)
                overlap = 1;
            top--;
        }
        if (overlap) {
            cst_sbuf[si].parent = -1;
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
        CST_ASSERT(cst_sstop == 1);
        while (cst_sstop > 1)
            cst_hook_close();
        cst_sbuf[0].last_leaf = cst_lcount;
        uint32_t tail = cst_lcount ? cst_lbuf[cst_lcount - 1].off +
                                         cst_lbuf[cst_lcount - 1].len
                                   : 0;
        if (tail < a->src_len) {
            cst_hook_token(tail, a->src_len);
            cst_sbuf[0].last_leaf = cst_lcount;
        }
        cst_nest_specs();
        cst_materialize(0);
        cst_rehash_all(a);
        cst_index_build(a);
        uint32_t u;
        for (u = 0; u < cst_uses_count; u++) {
            CstLocal un = cst_node_at(a, cst_uses[u].use_off);
            CstLocal dn = cst_node_at(a, cst_uses[u].def_off);
            if (un != CST_NONE && dn != CST_NONE && un != dn)
                cst_set_sym_ref(a, un, cst_id(a->file_id, dn));
        }
        uint32_t ki = 0, nn;
        for (nn = 0; nn < a->count && ki < cst_inc_count; nn++)
            if (a->kind[nn] == CST_IncludeDirective)
                cst_set_include_target(a, nn, cst_inc_tmpl[ki++]);
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

#endif
