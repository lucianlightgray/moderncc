#ifdef MCC_EMBED_JIT

#include "mcc.h"
#include "mccast.h"
#include "mccgate.h"
#include "mccjit_internal.h"

#include <string.h>


MCCJIT_LOCAL void mccjit_buf_init(MccjitBuf *b) { MCC_TRACE("enter\n");
	b->data = NULL;
	b->len = 0;
	b->cap = 0;
	b->oom = 0;
}

MCCJIT_LOCAL void mccjit_buf_free(MccjitBuf *b) { MCC_TRACE("enter\n");
	mcc_free(b->data);
	b->data = NULL;
	b->len = b->cap = 0;
}

static void mccjit_buf_put(MccjitBuf *b, const void *p, size_t n) { MCC_TRACE("enter\n");
	if (b->oom)
		return;
	if (b->len + n > b->cap) { MCC_TRACE("br\n");
		size_t ncap = b->cap ? b->cap * 2 : 256;
		unsigned char *nd;
		while (ncap < b->len + n)
			ncap *= 2;
		nd = mcc_realloc(b->data, ncap);
		if (!nd) { MCC_TRACE("br\n");
			b->oom = 1;
			return;
		}
		b->data = nd;
		b->cap = ncap;
	}
	memcpy(b->data + b->len, p, n);
	b->len += n;
}

static void mccjit_put_u8(MccjitBuf *b, uint8_t v) { MCC_TRACE("enter\n");
	mccjit_buf_put(b, &v, 1);
}
static void mccjit_put_u16(MccjitBuf *b, uint16_t v) { MCC_TRACE("enter\n");
	unsigned char t[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
	mccjit_buf_put(b, t, 2);
}
static void mccjit_put_u32(MccjitBuf *b, uint32_t v) { MCC_TRACE("enter\n");
	unsigned char t[4];
	int i;
	for (i = 0; i < 4; i++)
		t[i] = (unsigned char)(v >> (i * 8));
	mccjit_buf_put(b, t, 4);
}
static void mccjit_put_u64(MccjitBuf *b, uint64_t v) { MCC_TRACE("enter\n");
	unsigned char t[8];
	int i;
	for (i = 0; i < 8; i++)
		t[i] = (unsigned char)(v >> (i * 8));
	mccjit_buf_put(b, t, 8);
}
static void mccjit_put_str(MccjitBuf *b, const char *s) { MCC_TRACE("enter\n");
	uint32_t n = s ? (uint32_t)strlen(s) : 0;
	mccjit_put_u32(b, n);
	if (n)
		mccjit_buf_put(b, s, n);
}

typedef struct MccjitReader {
	const unsigned char *data;
	size_t len;
	size_t pos;
	int err;
} MccjitReader;

static int mccjit_have(MccjitReader *r, size_t n) { MCC_TRACE("enter\n");
	if (r->err || r->pos + n > r->len) { MCC_TRACE("br\n");
		r->err = 1;
		return 0;
	}
	return 1;
}
static uint8_t mccjit_get_u8(MccjitReader *r) { MCC_TRACE("enter\n");
	uint8_t v = 0;
	if (mccjit_have(r, 1)) { MCC_TRACE("br\n");
		v = r->data[r->pos];
		r->pos += 1;
	}
	return v;
}
static uint16_t mccjit_get_u16(MccjitReader *r) { MCC_TRACE("enter\n");
	uint16_t v = 0;
	if (mccjit_have(r, 2)) { MCC_TRACE("br\n");
		v = (uint16_t)(r->data[r->pos] | (r->data[r->pos + 1] << 8));
		r->pos += 2;
	}
	return v;
}
static uint32_t mccjit_get_u32(MccjitReader *r) { MCC_TRACE("enter\n");
	uint32_t v = 0;
	int i;
	if (mccjit_have(r, 4)) { MCC_TRACE("br\n");
		for (i = 0; i < 4; i++)
			v |= (uint32_t)r->data[r->pos + i] << (i * 8);
		r->pos += 4;
	}
	return v;
}
static uint64_t mccjit_get_u64(MccjitReader *r) { MCC_TRACE("enter\n");
	uint64_t v = 0;
	int i;
	if (mccjit_have(r, 8)) { MCC_TRACE("br\n");
		for (i = 0; i < 8; i++)
			v |= (uint64_t)r->data[r->pos + i] << (i * 8);
		r->pos += 8;
	}
	return v;
}
static char *mccjit_get_str(MccjitReader *r) { MCC_TRACE("enter\n");
	uint32_t n = mccjit_get_u32(r);
	char *s;
	if (r->err || r->pos + n > r->len) { MCC_TRACE("br\n");
		r->err = 1;
		return NULL;
	}
	s = mcc_malloc(n + 1);
	if (!s) { MCC_TRACE("br\n");
		r->err = 1;
		return NULL;
	}
	if (n)
		memcpy(s, r->data + r->pos, n);
	s[n] = 0;
	r->pos += n;
	return s;
}

typedef struct MccjitHandles {
	uint64_t *raw;
	int64_t *token_v;
	uint8_t *role;
	uint32_t count;
	uint32_t cap;
	int oom;
} MccjitHandles;

static void mccjit_handles_init(MccjitHandles *h) { MCC_TRACE("enter\n");
	h->raw = NULL;
	h->token_v = NULL;
	h->role = NULL;
	h->count = 0;
	h->cap = 0;
	h->oom = 0;
}
static void mccjit_handles_free(MccjitHandles *h) { MCC_TRACE("enter\n");
	mcc_free(h->raw);
	mcc_free(h->token_v);
	mcc_free(h->role);
	mccjit_handles_init(h);
}

static uint32_t mccjit_handles_intern(MccjitHandles *h, uint64_t raw, unsigned role) { MCC_TRACE("enter\n");
	uint32_t i;
	if (raw == 0)
		return 0;
	for (i = 0; i < h->count; i++)
		if (h->raw[i] == raw) { MCC_TRACE("br\n");
			if (role != MCCJIT_ROLE_PLAIN && h->role[i] == MCCJIT_ROLE_PLAIN)
				h->role[i] = (uint8_t)role;
			return i + 1;
		}
	if (h->count == h->cap) { MCC_TRACE("br\n");
		uint32_t ncap = h->cap ? h->cap * 2 : 16;
		uint64_t *nr = mcc_realloc(h->raw, ncap * sizeof *nr);
		int64_t *nv = mcc_realloc(h->token_v, ncap * sizeof *nv);
		uint8_t *no = mcc_realloc(h->role, ncap * sizeof *no);
		if (nr)
			h->raw = nr;
		if (nv)
			h->token_v = nv;
		if (no)
			h->role = no;
		if (!nr || !nv || !no) { MCC_TRACE("br\n");
			h->oom = 1;
			return 0;
		}
		h->cap = ncap;
	}
	h->raw[h->count] = raw;
	h->token_v[h->count] = (int64_t)(intptr_t)((Sym *)(uintptr_t)raw)->v;
	h->role[h->count] = (uint8_t)role;
	h->count++;
	return h->count;
}

static void mccjit_handles_expand(MccjitHandles *h, uint32_t i) { MCC_TRACE("enter\n");
	Sym *s = (Sym *)(uintptr_t)h->raw[i];
	Sym *p;
	if (!s)
		return;
	switch (h->role[i]) { MCC_TRACE("br\n");
	case MCCJIT_ROLE_PTR:
		if (s->type.ref)
			mccjit_handles_intern(h, (uint64_t)(uintptr_t)s->type.ref,
														mccjit_role_for_base(s->type.t));
		break;
	case MCCJIT_ROLE_FUNC:
		if (s->type.ref)
			mccjit_handles_intern(h, (uint64_t)(uintptr_t)s->type.ref,
														mccjit_role_for_base(s->type.t));
		for (p = s->next; p; p = p->next)
			if (p->type.ref)
				mccjit_handles_intern(h, (uint64_t)(uintptr_t)p->type.ref,
															mccjit_role_for_base(p->type.t));
		break;
	case MCCJIT_ROLE_NAMED:
		if (s->type.ref)
			mccjit_handles_intern(h, (uint64_t)(uintptr_t)s->type.ref,
														mccjit_role_for_base(s->type.t));
		break;
	case MCCJIT_ROLE_STRUCT:
		for (p = s->next; p; p = p->next)
			if (p->type.ref)
				mccjit_handles_intern(h, (uint64_t)(uintptr_t)p->type.ref,
															mccjit_role_for_base(p->type.t));
		break;
	default:
		break;
	}
}

static int mccjit_data_sym_info(Sym *s, unsigned long *poff, unsigned long *psize) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	ElfSym *es;
	if (!s1 || !s || !(s->r & VT_SYM))
		return 0;
	if (!rodata_section)
		return 0;
	es = elfsym(s);
	if (!es || es->st_shndx == SHN_UNDEF || es->st_size == 0)
		return 0;
	if (es->st_shndx != rodata_section->sh_num)
		return 0;
	if (es->st_size > MCCJIT_DATA_MAX)
		return 0;
	if ((unsigned long)es->st_value + es->st_size > rodata_section->data_offset)
		return 0;
	if (rodata_section->reloc) { MCC_TRACE("br\n");
		Section *rel = rodata_section->reloc;
		unsigned long n = rel->data_offset / sizeof(ElfW_Rel), k;
		ElfW_Rel *r = (ElfW_Rel *)rel->data;
		for (k = 0; k < n; k++)
			if (r[k].r_offset >= (addr_t)es->st_value &&
					r[k].r_offset < (addr_t)es->st_value + es->st_size)
				return 0;
	}
	*poff = es->st_value;
	*psize = es->st_size;
	return 1;
}

static void mccjit_emit_type_record(MccjitBuf *buf, MccjitHandles *h, uint32_t i) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	Sym *s = (Sym *)(uintptr_t)h->raw[i];
	unsigned role = h->role[i];
	mccjit_put_u8(buf, (uint8_t)role);
	switch (role) { MCC_TRACE("br\n");
	case MCCJIT_ROLE_NAMED:
	case MCCJIT_ROLE_PTR:
		mccjit_put_u32(buf, (uint32_t)s->type.t);
		mccjit_put_u32(buf, s->type.ref
													 ? mccjit_handles_intern(
																 h, (uint64_t)(uintptr_t)s->type.ref,
																 mccjit_role_for_base(s->type.t))
													 : 0);
		break;
	case MCCJIT_ROLE_FUNC: {
		Sym *p;
		uint32_t np = 0;
		mccjit_put_u32(buf, (uint32_t)s->type.t);
		mccjit_put_u32(buf, s->type.ref
													 ? mccjit_handles_intern(
																 h, (uint64_t)(uintptr_t)s->type.ref,
																 mccjit_role_for_base(s->type.t))
													 : 0);
		mccjit_put_u32(buf, (uint32_t)s->f.func_type);
		mccjit_put_u32(buf, (uint32_t)s->f.func_call);
		for (p = s->next; p; p = p->next)
			np++;
		mccjit_put_u32(buf, np);
		for (p = s->next; p; p = p->next) { MCC_TRACE("br\n");
			mccjit_put_u32(buf, (uint32_t)p->type.t);
			mccjit_put_u32(buf, p->type.ref
														 ? mccjit_handles_intern(
																	 h, (uint64_t)(uintptr_t)p->type.ref,
																	 mccjit_role_for_base(p->type.t))
														 : 0);
		}
		break;
	}
	case MCCJIT_ROLE_STRUCT: {
		Sym *p;
		uint32_t nf = 0;
		mccjit_put_u32(buf, (uint32_t)s->type.t);
		mccjit_put_u32(buf, (uint32_t)s->c);
		mccjit_put_u32(buf, (uint32_t)s->r);
		for (p = s->next; p; p = p->next)
			nf++;
		mccjit_put_u32(buf, nf);
		for (p = s->next; p; p = p->next) { MCC_TRACE("br\n");
			int ft = (p->v & ~(SYM_FIELD | SYM_STRUCT));
			const char *fn = (ft >= TOK_IDENT && ft < SYM_FIRST_ANOM)
													 ? get_tok_str(ft, NULL)
													 : "";
			mccjit_put_str(buf, fn);
			mccjit_put_u32(buf, (uint32_t)p->c);
			mccjit_put_u32(buf, (uint32_t)p->type.t);
			mccjit_put_u32(buf, p->type.ref
														 ? mccjit_handles_intern(
																	 h, (uint64_t)(uintptr_t)p->type.ref,
																	 mccjit_role_for_base(p->type.t))
														 : 0);
		}
		break;
	}
	case MCCJIT_ROLE_DATA: {
		unsigned long off = 0, sz = 0, bi;
		mccjit_data_sym_info(s, &off, &sz);
		mccjit_put_u32(buf, (uint32_t)sz);
		for (bi = 0; bi < sz; bi++)
			mccjit_put_u8(buf, rodata_section->data[off + bi]);
		break;
	}
	default:
		break;
	}
}

MCCJIT_LOCAL int mccjit_intent_serialize(const AstArena *a, Sym *sym, MccjitBuf *buf) { MCC_TRACE("enter\n");
	MccjitHandles handles;
	AstLocal count, n;
	uint32_t k;
	if (!a || !buf)
		return -1;
	count = ast_count(a);
	mccjit_handles_init(&handles);

	for (n = 0; n < count; n++) { MCC_TRACE("br\n");
		uint64_t tref = ast_type_ref(a, n);
		uint64_t nsym = ast_sym(a, n);
		if (tref)
			mccjit_handles_intern(&handles, tref,
														mccjit_role_for_base(ast_type_t(a, n)));
		if (nsym)
			mccjit_handles_intern(&handles, nsym,
														(ast_op(a, n) & VT_SYM) ? MCCJIT_ROLE_NAMED
																										: MCCJIT_ROLE_PLAIN);
	}
	for (k = 0; k < handles.count; k++)
		mccjit_handles_expand(&handles, k);
	if (handles.oom) { MCC_TRACE("br\n");
		mccjit_handles_free(&handles);
		return -1;
	}

	for (k = 0; k < handles.count; k++) { MCC_TRACE("br\n");
		int64_t tv = handles.token_v[k];
		if (handles.role[k] == MCCJIT_ROLE_NAMED &&
				!(tv >= TOK_IDENT && tv < SYM_FIRST_ANOM)) { MCC_TRACE("br\n");
			Sym *s = (Sym *)(uintptr_t)handles.raw[k];
			unsigned long off, sz;
			if (mccjit_data_sym_info(s, &off, &sz)) { MCC_TRACE("br\n");
				handles.role[k] = (uint8_t)MCCJIT_ROLE_DATA;
				continue;
			}
			mccjit_handles_free(&handles);
			return -1;
		}
	}

	mccjit_put_u32(buf, MCCJIT_INTENT_MAGIC);
	mccjit_put_u32(buf, MCCJIT_INTENT_FORMAT);
	mccjit_put_u64(buf, mccjit_salt_witness());
	mccjit_put_u64(buf, sym ? (uint64_t)(int64_t)sym->v : 0);
	mccjit_put_u32(buf, (uint32_t)count);
	mccjit_put_u32(buf, (uint32_t)ast_root(a));

	mccjit_put_u32(buf, handles.count);
	for (k = 0; k < handles.count; k++) { MCC_TRACE("br\n");
		int64_t tv = handles.token_v[k];
		mccjit_put_u64(buf, handles.raw[k]);
		mccjit_put_u64(buf, (uint64_t)tv);
		if (tv >= TOK_IDENT && tv < SYM_FIRST_ANOM)
			mccjit_put_str(buf, get_tok_str((int)tv, NULL));
		else
			mccjit_put_str(buf, "");
		mccjit_emit_type_record(buf, &handles, k);
	}

	for (n = 0; n < count; n++) { MCC_TRACE("br\n");
		uint32_t nc = ast_nchild(a, n), i;
		uint64_t nsym = ast_sym(a, n);
		uint64_t tref = ast_type_ref(a, n);
		mccjit_put_u16(buf, ast_kind(a, n));
		mccjit_put_u32(buf, (uint32_t)ast_op(a, n));
		mccjit_put_u32(buf, (uint32_t)ast_type_t(a, n));
		mccjit_put_u64(buf, ast_ival(a, n));
		mccjit_put_u64(buf, ast_fbits(a, n));
		mccjit_put_u32(buf, nsym ? mccjit_handles_intern(
															 &handles, nsym,
															 (ast_op(a, n) & VT_SYM) ? MCCJIT_ROLE_NAMED
																											 : MCCJIT_ROLE_PLAIN)
														 : 0);
		mccjit_put_u32(buf, tref ? mccjit_handles_intern(
															 &handles, tref, mccjit_role_for_base(ast_type_t(a, n)))
														 : 0);
		mccjit_put_u64(buf, ast_cst(a, n));
		mccjit_put_u32(buf, nc);
		for (i = 0; i < nc; i++)
			mccjit_put_u32(buf, (uint32_t)ast_child(a, n, i));
	}

	{
		Sym *sig = sym ? sym->type.ref : NULL;
		Sym *p;
		uint32_t np = 0;
		const char *fname = (sym && sym->v >= TOK_IDENT && sym->v < SYM_FIRST_ANOM)
														? get_tok_str(sym->v, NULL)
														: "";
		mccjit_put_str(buf, fname);
		mccjit_put_u32(buf, sig ? (uint32_t)sig->type.t : (uint32_t)VT_INT);
		mccjit_put_u32(buf, sig ? (uint32_t)sig->f.func_type : (uint32_t)FUNC_NEW);
		for (p = sig ? sig->next : NULL; p; p = p->next)
			np++;
		mccjit_put_u32(buf, np);
		for (p = sig ? sig->next : NULL; p; p = p->next) { MCC_TRACE("br\n");
			const char *pn = (p->v >= TOK_IDENT && p->v < SYM_FIRST_ANOM)
													 ? get_tok_str(p->v, NULL)
													 : "";
			Sym *ls = (p->v >= TOK_IDENT && p->v < SYM_FIRST_ANOM) ? sym_find(p->v) : NULL;
			int64_t poff = (ls && (ls->r & VT_VALMASK) == VT_LOCAL) ? (int64_t)ls->c
																															: (int64_t)p->c;
			mccjit_put_u32(buf, (uint32_t)p->type.t);
			mccjit_put_u64(buf, (uint64_t)poff);
			mccjit_put_str(buf, pn);
		}
	}

	mccjit_handles_free(&handles);
	return buf->oom ? -1 : 0;
}


MCCJIT_LOCAL void mccjit_intent_release(MccjitIntent *it) { MCC_TRACE("enter\n");
	uint32_t i;
	if (!it)
		return;
	if (it->arena)
		ast_arena_free(it->arena);
	if (it->handle_name)
		for (i = 0; i < it->handle_count; i++)
			mcc_free(it->handle_name[i]);
	if (it->param_name)
		for (i = 0; i < it->nparam; i++)
			mcc_free(it->param_name[i]);
	if (it->recs)
		for (i = 0; i < it->handle_count; i++) { MCC_TRACE("br\n");
			uint32_t j;
			if (it->recs[i].fnm)
				for (j = 0; j < it->recs[i].nparam; j++)
					mcc_free(it->recs[i].fnm[j]);
			mcc_free(it->recs[i].fnm);
			mcc_free(it->recs[i].foff);
			mcc_free(it->recs[i].pt);
			mcc_free(it->recs[i].pr);
			mcc_free(it->recs[i].data);
		}
	mcc_free(it->recs);
	mcc_free(it->handle_raw);
	mcc_free(it->handle_token_v);
	mcc_free(it->handle_name);
	mcc_free(it->fn_name);
	mcc_free(it->param_type_t);
	mcc_free(it->param_off);
	mcc_free(it->param_name);
	it->arena = NULL;
	it->handle_raw = NULL;
	it->handle_token_v = NULL;
	it->handle_name = NULL;
	it->recs = NULL;
	it->fn_name = NULL;
	it->param_type_t = NULL;
	it->param_off = NULL;
	it->param_name = NULL;
}

static Sym *mccjit_build_rec(MccjitIntent *it, uint32_t id1) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	uint32_t i;
	MccjitTypeRec *r;
	Sym *res = NULL;
	if (!id1 || id1 > it->handle_count || !it->recs)
		return NULL;
	i = id1 - 1;
	r = &it->recs[i];
	if (r->done)
		return r->built;
	if (r->building)
		return NULL;
	r->building = 1;
	switch (r->role) { MCC_TRACE("br\n");
	case MCCJIT_ROLE_PTR: {
		CType pc;
		pc.t = (int)r->a;
		pc.ref = mccjit_build_rec(it, r->b);
		res = sym_push(SYM_FIELD, &pc, 0, -1);
		break;
	}
	case MCCJIT_ROLE_FUNC: {
		Sym *sr = sym_push2(&global_stack, SYM_FIELD, 0, 0);
		Sym *first = NULL, **plast = &first, *p;
		uint32_t k;
		sr->type.t = (int)r->a;
		sr->type.ref = mccjit_build_rec(it, r->b);
		sr->f.func_call = (int)r->d;
		sr->f.func_type = r->c ? (int)r->c : FUNC_NEW;
		sr->f.func_args = r->nparam;
		for (k = 0; k < r->nparam; k++) { MCC_TRACE("br\n");
			p = sym_push2(&global_stack, SYM_FIELD, (int)r->pt[k], 0);
			p->type.ref = mccjit_build_rec(it, r->pr[k]);
			p->r = VT_LOCAL | VT_LVAL;
			p->a.inited = 1;
			*plast = p;
			plast = &p->next;
		}
		sr->next = first;
		res = sr;
		break;
	}
	case MCCJIT_ROLE_STRUCT: {
		Sym *first = NULL, **plast = &first, *p;
		uint32_t k;
		Sym *ss = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, (int)r->a, (int)r->c);
		ss->r = (unsigned short)r->d;
		r->built = ss;
		r->done = 1;
		r->building = 0;
		res = ss;
		for (k = 0; k < r->nparam; k++) { MCC_TRACE("br\n");
			const char *fn = it->recs[i].fnm ? it->recs[i].fnm[k] : NULL;
			int ftok = (fn && fn[0]) ? (tok_alloc(fn, (int)strlen(fn))->tok | SYM_FIELD)
															 : (anon_sym++ | SYM_FIELD);
			p = sym_push2(&global_stack, ftok, (int)r->pt[k], (int)r->foff[k]);
			p->type.ref = mccjit_build_rec(it, r->pr[k]);
			*plast = p;
			plast = &p->next;
		}
		ss->next = first;
		break;
	}
	case MCCJIT_ROLE_NAMED: {
		CType ct;
		const char *nm = it->handle_name ? it->handle_name[i] : NULL;
		if (nm && nm[0]) { MCC_TRACE("br\n");
			ct.t = (int)r->a;
			ct.ref = mccjit_build_rec(it, r->b);
			res = external_global_sym(tok_alloc(nm, (int)strlen(nm))->tok, &ct);
		}
		break;
	}
	case MCCJIT_ROLE_DATA: {
		unsigned long off, pad;
		unsigned char *p;
		if (!rodata_section)
			break;
		pad = (16 - (rodata_section->data_offset & 15)) & 15;
		if (pad)
			section_ptr_add(rodata_section, pad);
		off = rodata_section->data_offset;
		p = section_ptr_add(rodata_section, r->datalen ? r->datalen : 1);
		if (r->datalen && r->data)
			memcpy(p, r->data, r->datalen);
		res = get_sym_ref(&char_pointer_type, rodata_section, off, r->datalen);
		break;
	}
	default:
		break;
	}
	r->built = res;
	r->done = 1;
	r->building = 0;
	return res;
}

MCCJIT_LOCAL int mccjit_intent_deserialize(const void *buf, size_t len,
																					 MccjitIntent *out) { MCC_TRACE("enter\n");
	MccjitReader r;
	AstArena *a;
	uint32_t count, i, hc;
	uint32_t *nc_of = NULL;
	uint32_t **kids = NULL;
	int rc = -1;

	if (!buf || !out)
		return -1;
	memset(out, 0, sizeof *out);
	r.data = buf;
	r.len = len;
	r.pos = 0;
	r.err = 0;

	if (mccjit_get_u32(&r) != MCCJIT_INTENT_MAGIC)
		return -1;
	if (mccjit_get_u32(&r) != MCCJIT_INTENT_FORMAT)
		return -1;
	out->salt = mccjit_get_u64(&r);
	out->anchor_sym_v = (int64_t)mccjit_get_u64(&r);
	count = mccjit_get_u32(&r);
	(void)mccjit_get_u32(&r);

	hc = mccjit_get_u32(&r);
	if (r.err)
		return -1;
	if (hc) { MCC_TRACE("br\n");
		out->handle_raw = mcc_malloc(hc * sizeof *out->handle_raw);
		out->handle_token_v = mcc_malloc(hc * sizeof *out->handle_token_v);
		out->handle_name = mcc_mallocz(hc * sizeof *out->handle_name);
		out->recs = mcc_mallocz(hc * sizeof *out->recs);
		if (!out->handle_raw || !out->handle_token_v || !out->handle_name ||
				!out->recs)
			goto done;
	}
	out->handle_count = hc;
	for (i = 0; i < hc; i++) { MCC_TRACE("br\n");
		uint64_t raw = mccjit_get_u64(&r);
		int64_t tv = (int64_t)mccjit_get_u64(&r);
		char *nm = mccjit_get_str(&r);
		MccjitTypeRec *rec = &out->recs[i];
		if (r.err)
			goto done;
		out->handle_raw[i] = raw;
		out->handle_name[i] = nm;
		if (nm && nm[0] && mcc_state)
			tv = tok_alloc(nm, (int)strlen(nm))->tok;
		out->handle_token_v[i] = tv;
		rec->role = mccjit_get_u8(&r);
		if (rec->role == MCCJIT_ROLE_NAMED)
			out->has_external = 1;
		switch (rec->role) { MCC_TRACE("br\n");
		case MCCJIT_ROLE_NAMED:
		case MCCJIT_ROLE_PTR:
			rec->a = mccjit_get_u32(&r);
			rec->b = mccjit_get_u32(&r);
			break;
		case MCCJIT_ROLE_FUNC: {
			uint32_t k;
			rec->a = mccjit_get_u32(&r);
			rec->b = mccjit_get_u32(&r);
			rec->c = mccjit_get_u32(&r);
			rec->d = mccjit_get_u32(&r);
			rec->nparam = mccjit_get_u32(&r);
			if (r.err)
				goto done;
			if (rec->nparam) { MCC_TRACE("br\n");
				rec->pt = mcc_mallocz(rec->nparam * sizeof *rec->pt);
				rec->pr = mcc_mallocz(rec->nparam * sizeof *rec->pr);
				if (!rec->pt || !rec->pr)
					goto done;
			}
			for (k = 0; k < rec->nparam; k++) { MCC_TRACE("br\n");
				rec->pt[k] = mccjit_get_u32(&r);
				rec->pr[k] = mccjit_get_u32(&r);
			}
			break;
		}
		case MCCJIT_ROLE_STRUCT: {
			uint32_t k;
			rec->a = mccjit_get_u32(&r);
			rec->c = mccjit_get_u32(&r);
			rec->d = mccjit_get_u32(&r);
			rec->nparam = mccjit_get_u32(&r);
			if (r.err)
				goto done;
			if (rec->nparam) { MCC_TRACE("br\n");
				rec->pt = mcc_mallocz(rec->nparam * sizeof *rec->pt);
				rec->pr = mcc_mallocz(rec->nparam * sizeof *rec->pr);
				rec->foff = mcc_mallocz(rec->nparam * sizeof *rec->foff);
				rec->fnm = mcc_mallocz(rec->nparam * sizeof *rec->fnm);
				if (!rec->pt || !rec->pr || !rec->foff || !rec->fnm)
					goto done;
			}
			for (k = 0; k < rec->nparam; k++) { MCC_TRACE("br\n");
				rec->fnm[k] = mccjit_get_str(&r);
				rec->foff[k] = mccjit_get_u32(&r);
				rec->pt[k] = mccjit_get_u32(&r);
				rec->pr[k] = mccjit_get_u32(&r);
			}
			break;
		}
		case MCCJIT_ROLE_DATA: {
			uint32_t bi;
			rec->datalen = mccjit_get_u32(&r);
			if (r.err || rec->datalen > MCCJIT_DATA_MAX)
				goto done;
			if (rec->datalen) { MCC_TRACE("br\n");
				rec->data = mcc_malloc(rec->datalen);
				if (!rec->data)
					goto done;
				for (bi = 0; bi < rec->datalen; bi++)
					rec->data[bi] = mccjit_get_u8(&r);
			}
			break;
		}
		default:
			break;
		}
		if (r.err)
			goto done;
	}
	if (r.err)
		goto done;

	if (mcc_state)
		for (i = 0; i < hc; i++)
			mccjit_build_rec(out, i + 1);

	a = ast_arena_new();
	if (!a)
		goto done;
	out->arena = a;

	if (count) { MCC_TRACE("br\n");
		nc_of = mcc_mallocz((count) * (sizeof *nc_of));
		kids = mcc_mallocz((count) * (sizeof *kids));
		if (!nc_of || !kids)
			goto done;
	}

	for (i = 0; i < count; i++) { MCC_TRACE("br\n");
		uint16_t kind = mccjit_get_u16(&r);
		int32_t op = (int32_t)mccjit_get_u32(&r);
		int32_t type_t = (int32_t)mccjit_get_u32(&r);
		uint64_t ival = mccjit_get_u64(&r);
		uint64_t fbits = mccjit_get_u64(&r);
		uint32_t sym_id = mccjit_get_u32(&r);
		uint32_t typeref_id = mccjit_get_u32(&r);
		uint64_t cst = mccjit_get_u64(&r);
		uint32_t nc = mccjit_get_u32(&r), j;
		Sym *sym_new = (sym_id && sym_id <= hc) ? mccjit_build_rec(out, sym_id) : NULL;
		Sym *tref_new =
				(typeref_id && typeref_id <= hc) ? mccjit_build_rec(out, typeref_id) : NULL;
		uint64_t sym_raw = (uint64_t)(uintptr_t)sym_new;
		uint64_t tref_raw = (uint64_t)(uintptr_t)tref_new;
		AstLocal node;
		if (r.err)
			goto done;
		if ((op & VT_VALMASK) == VT_LOCAL && !(op & VT_SYM))
			sym_raw = 0;
		node = ast_node(a, kind);
		ast_set_op(a, node, op);
		ast_set_type(a, node, type_t, tref_raw);
		ast_set_ival(a, node, ival);
		ast_set_fbits(a, node, fbits);
		ast_set_sym(a, node, sym_raw);
		ast_set_cst(a, node, cst);
		nc_of[i] = nc;
		if (nc) { MCC_TRACE("br\n");
			kids[i] = mcc_malloc(nc * sizeof **kids);
			if (!kids[i])
				goto done;
			for (j = 0; j < nc; j++)
				kids[i][j] = mccjit_get_u32(&r);
		}
	}
	if (r.err)
		goto done;

	for (i = 0; i < count; i++) { MCC_TRACE("br\n");
		uint32_t j;
		for (j = 0; j < nc_of[i]; j++)
			if (kids[i][j] < count)
				ast_add_child(a, i, kids[i][j]);
	}

	out->fn_name = mccjit_get_str(&r);
	out->ret_type_t = mccjit_get_u32(&r);
	out->func_type = mccjit_get_u32(&r);
	out->nparam = mccjit_get_u32(&r);
	if (r.err)
		goto done;
	if (out->nparam) { MCC_TRACE("br\n");
		out->param_type_t = mcc_mallocz(out->nparam * sizeof *out->param_type_t);
		out->param_off = mcc_mallocz(out->nparam * sizeof *out->param_off);
		out->param_name = mcc_mallocz(out->nparam * sizeof *out->param_name);
		if (!out->param_type_t || !out->param_off || !out->param_name)
			goto done;
	}
	for (i = 0; i < out->nparam; i++) { MCC_TRACE("br\n");
		out->param_type_t[i] = mccjit_get_u32(&r);
		out->param_off[i] = (int64_t)mccjit_get_u64(&r);
		out->param_name[i] = mccjit_get_str(&r);
		if (r.err)
			goto done;
	}
	rc = 0;

done:
	if (kids)
		for (i = 0; i < count; i++)
			mcc_free(kids[i]);
	mcc_free(kids);
	mcc_free(nc_of);
	if (rc != 0)
		mccjit_intent_release(out);
	return rc;
}

MCCJIT_LOCAL Sym *mccjit_rebuild_sym(const MccjitIntent *it) { MCC_TRACE("enter\n");
	CType functype;
	Sym *sr, *first = NULL, **plast = &first, *p;
	uint32_t i;
	if (!it || !it->fn_name || !it->fn_name[0])
		return NULL;

	sr = sym_push2(&global_stack, SYM_FIELD, 0, 0);
	sr->type.t = (int)it->ret_type_t;
	sr->type.ref = NULL;
	sr->f.func_call = FUNC_CDECL;
	sr->f.func_type = it->func_type ? (int)it->func_type : FUNC_NEW;
	sr->f.func_args = it->nparam;

	for (i = 0; i < it->nparam; i++) { MCC_TRACE("br\n");
		const char *pn = it->param_name ? it->param_name[i] : NULL;
		int ptok = (pn && pn[0]) ? tok_alloc(pn, (int)strlen(pn))->tok : SYM_FIELD;
		p = sym_push2(&global_stack, ptok, (int)it->param_type_t[i], 0);
		p->type.ref = NULL;
		p->r = VT_LOCAL | VT_LVAL;
		p->a.inited = 1;
		*plast = p;
		plast = &p->next;
	}
	sr->next = first;

	functype.t = VT_FUNC;
	functype.ref = sr;
	return external_global_sym(tok_alloc(it->fn_name, (int)strlen(it->fn_name))->tok,
														 &functype);
}
#endif /* MCC_EMBED_JIT */
