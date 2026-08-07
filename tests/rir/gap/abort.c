/* rir-gap-levels: O2,O3 */
/* Replay aborts: the arena models the body but ast_replay_body() cannot run
   it to completion, so the parser's bytes are kept and the body is a true
   coverage gap.

   The shape below is host_runmem_alloc() in src/mcchost.c reduced: a store
   whose value is a call, one of whose arguments is itself a store to a live
   operand.  ast_storeval_* marks the Invoke AST_FB_CALL_STOREVAL_ARG and the
   replay then finds fewer vstack entries than the rotation needs and raises
   "ast-replay: storeval-arg stack underflow".  That AST pass only runs from
   -O2, hence the level restriction above.

   Until 2026-08-06 this fixture was `((struct S *)(unsigned long)p[i])->v`,
   which aborted at every level with "pointer expected" because the outer cast
   changed the static type but emitted no machine code and so left no trace in
   the arena.  rir_hook_cast_type() now records the post-cast CType and that
   whole family replays byte-identically. */
void *my_malloc(unsigned n);
unsigned pagesize(void);

void *f(unsigned *psize) {
	unsigned size = *psize;
	void *ptr;
	ptr = my_malloc(size += pagesize());
	*psize = size;
	return ptr;
}
