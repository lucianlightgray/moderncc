---
name: mcc-jit-unification
description: Runtime-JIT trigger model (--jit / MCC_JIT / MCC_CONFIG_JIT) and the embed-registry pollution bug/fix
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dc1c909-1340-4259-be40-36cfb496a3a2
---

The JIT trigger is runtime, not compile-time: `--jit`/`--no-jit` CLI flags + `MCC_JIT` env
(0/1, read by the baked `.init_array` constructor) + CMake `MCC_CONFIG_JIT` (default ON →
`MCC_JIT_DEFAULT`). Precedence: env > flag (for -run) > CMake default. `--embed-jit` bakes the
machinery. Real `mcc -run` JIT is `MCC_OUTPUT_MEMORY` with `embed_jit==0` — so you cannot gate
embed machinery on `embed_jit` alone; MEMORY output is itself an intentional trigger
(`ast_jit_env`, `mccjit_embed_finalize`, the `mccast.c:12742` mode-6 transform all key off
`embed_jit || output==MCC_OUTPUT_MEMORY`).

**Embed-registry pollution bug (fixed 2026-07-14, commit 1a89f809).** `mccjit_embed_fns` is a
process-global list. Once `ast_jit_env` became true for any MEMORY compile, internal
`mccjit_stash_one` helpers (selftest-only) ran the dispatch transform, whose `mccjit_embed_note`
(`mccast.c:12778`) appended the stashed fn to that global. Nothing clears it (stash never
relocates), so a later `mcc_relocate` of any internal MEMORY state ran `mccjit_embed_finalize`
over stale entries → `extern void *__mccjit_slot_<fn>` + registry with no slot definition →
"unresolved reference to '__mccjit_slot_f'" / "baseline recompile returned NULL". Regressed 7
jit selftests. **The original TODO diagnosis (transform re-firing inside the recompile's
`ast_reemit_extern`) was wrong**: `ast_reemit_extern → ast_reemit → ast_replay_body` never
reaches the 12742 transform (the transform fires only during the stash). Fix = a file-static
`mccjit_internal_compile` in `src/mccjit_embed.c`, set around the compile in `mccjit_stash_one`
and around reemit+relocate in `mccjit_recompile_common`; both `mccjit_embed_note` (producer) and
`mccjit_embed_finalize` (consumer) early-return while set. Validate: `ctest -R jit/` = 28/28 in
cmake-linux-gcc under amd64 docker (JIT is x86_64/Linux; run via `--platform linux/amd64`).

**Done (commit d099aa73):** JIT of string-using functions via `MCCJIT_ROLE_DATA`
(`MCCJIT_INTENT_FORMAT` 4→5). Anonymous rodata string literals are serialized as raw bytes and
rematerialized as a fresh 16-byte-aligned rodata sym in `mccjit_build_rec`. Key realization that
made it simple: replay takes the SValue type from the AST node (`ast_type_t`/`ast_type_ref`) and
uses the rebuilt sym only as the relocation target (`sv.sym`), and rebuilt `Sym*` are stored
directly into the arena — so NO type/array reconstruction and NO anon-token remapping are
needed; a `char*` `get_sym_ref` over the bytes suffices. `mccjit_data_sym_info` guards: rodata
only (immutable), in-bounds elfsym, size-capped, and no relocations in the byte range (else a
raw-byte copy drops pointer relocs → miscompile); anything else bails to AOT. Test:
`jit/selftest-strlit`. Pre-existing unrelated bug still open: recompiling a pointer-RETURNING
function segfaults in `ast_reemit` (no string involved).

Related: [[embedjit-arm64-vla-o1-crash]], [[build-dir-prefix]], [[commit-push-main]],
[[no-code-comments]].
