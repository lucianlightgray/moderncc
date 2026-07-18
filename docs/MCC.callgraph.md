# MCC Call Graph

> Derived from [`MCC.md`](MCC.md). Edges are name-based: a call is *internal* if the callee name is defined somewhere in the project, else *external* (libc / syscalls / builtins). Machine-readable form: [`MCC.callgraph.json`](MCC.callgraph.json); Graphviz: [`MCC.callgraph.dot`](MCC.callgraph.dot).

## Summary

| Metric | Count |
| --- | --- |
| Function definitions | 5893 |
| Unique function names | 4779 |
| Internal call edges (distinct caller→callee) | 11964 |
| Distinct external callees | 753 |
| Recursive functions | 132 |
| Leaf functions (call no internal fn) | 2041 |
| Root functions (never called internally) | 1165 |
| Ambiguous names (defined in >1 file) | 296 |

## Most-called internal functions (by distinct callers)

| # | Function | Callers | Calls out |
| --- | --- | --- | --- |
| 1 | `ast_kind` | 165 | 0 |
| 2 | `strcmp` | 135 | 1 |
| 3 | `memcpy` | 116 | 2 |
| 4 | `ast_op` | 112 | 0 |
| 5 | `free` | 112 | 6 |
| 6 | `get_tok_str` | 99 | 5 |
| 7 | `mcc_free` | 98 | 0 |
| 8 | `ast_first_child` | 92 | 0 |
| 9 | `strlen` | 88 | 1 |
| 10 | `ast_child` | 84 | 0 |
| 11 | `o` | 83 | 3 |
| 12 | `ast_add_child` | 76 | 0 |
| 13 | `ast_nchild` | 71 | 0 |
| 14 | `next` | 64 | 12 |
| 15 | `ast_next_sib` | 61 | 0 |
| 16 | `mcc_delete` | 59 | 6 |
| 17 | `ast_set_op` | 59 | 0 |
| 18 | `ast_node` | 57 | 1 |
| 19 | `type_size` | 56 | 1 |
| 20 | `ast_count` | 56 | 0 |
| 21 | `expect` | 56 | 0 |
| 22 | `strchr` | 56 | 1 |
| 23 | `section_ptr_add` | 55 | 1 |
| 24 | `ast_set_ival` | 51 | 0 |
| 25 | `mcc_malloc` | 50 | 0 |
| 26 | `memset` | 49 | 1 |
| 27 | `ast_set_type` | 46 | 0 |
| 28 | `malloc` | 44 | 5 |
| 29 | `strncmp` | 44 | 1 |
| 30 | `ast_ival` | 43 | 0 |
| 31 | `greloca` | 41 | 2 |
| 32 | `mcc_realloc` | 41 | 0 |
| 33 | `gv` | 40 | 13 |
| 34 | `ts_path` | 39 | 1 |
| 35 | `strstr` | 39 | 1 |
| 36 | `gen_le32` | 38 | 3 |
| 37 | `skip` | 37 | 3 |
| 38 | `parse_operand` | 37 | 20 |
| 39 | `ast_ident_etype` | 36 | 10 |
| 40 | `ast_clear_children` | 35 | 0 |

## Highest fan-out (calls the most distinct internal functions)

| # | Function | Calls out | Callers |
| --- | --- | --- | --- |
| 1 | `main` | 885 | 2 |
| 2 | `gfunc_call` | 53 | 24 |
| 3 | `asm_opcode` | 53 | 1 |
| 4 | `suite_template` | 42 | 1 |
| 5 | `ast_replay_value` | 40 | 3 |
| 6 | `load` | 39 | 17 |
| 7 | `ast_replay_bb` | 36 | 3 |
| 8 | `ast_func_end` | 34 | 1 |
| 9 | `elf_output_file` | 34 | 1 |
| 10 | `decode` | 33 | 1 |
| 11 | `unary` | 33 | 3 |
| 12 | `store` | 31 | 8 |
| 13 | `gfunc_prolog` | 29 | 2 |
| 14 | `mccjit_intent_serialize` | 26 | 3 |
| 15 | `mcc_parse_args` | 26 | 2 |
| 16 | `ast_tile_apply` | 26 | 1 |
| 17 | `ast_cse_stmts` | 25 | 2 |
| 18 | `gen_opf` | 24 | 1 |
| 19 | `scan_file` | 24 | 1 |
| 20 | `block` | 24 | 4 |
| 21 | `cst_capture_end` | 23 | 1 |
| 22 | `ast_ivsr_run` | 23 | 1 |
| 23 | `ast_pre_run` | 23 | 1 |
| 24 | `ast_abs_try` | 22 | 1 |
| 25 | `mccjit_recompile_common` | 22 | 3 |
| 26 | `ast_tco_run` | 22 | 1 |
| 27 | `gen_opi` | 21 | 2 |
| 28 | `ast_inline_graft` | 21 | 1 |
| 29 | `mccjit_wrap_one` | 21 | 1 |
| 30 | `gfunc_epilog` | 21 | 2 |
| 31 | `foldmath_eval` | 20 | 1 |
| 32 | `ast_cprop_stmt` | 20 | 2 |
| 33 | `init_putv` | 20 | 6 |
| 34 | `parse_operand` | 20 | 37 |
| 35 | `opi` | 20 | 1 |
| 36 | `gen_function` | 20 | 2 |
| 37 | `ast_inline_copy_expr` | 20 | 2 |
| 38 | `parse_include` | 20 | 2 |
| 39 | `expr_cond` | 20 | 3 |
| 40 | `gen_vla_alloc` | 19 | 2 |

## Most-used external symbols (libc / syscalls / builtins)

| # | Symbol | Call sites |
| --- | --- | --- |
| 1 | `printf` | 805 |
| 2 | `mcc_error` | 199 |
| 3 | `snprintf` | 185 |
| 4 | `fprintf` | 179 |
| 5 | `fclose` | 72 |
| 6 | `asm` | 72 |
| 7 | `fopen` | 59 |
| 8 | `__atomic_compare_exchange` | 56 |
| 9 | `MCC_TRACE` | 56 |
| 10 | `__atomic_load` | 54 |
| 11 | `assert` | 51 |
| 12 | `dprintf` | 48 |
| 13 | `getenv` | 48 |
| 14 | `mcc_error_noabort` | 47 |
| 15 | `mcc_warning` | 35 |
| 16 | `AST_SET_DESYNC` | 35 |
| 17 | `fflush` | 34 |
| 18 | `strrchr` | 30 |
| 19 | `fread` | 28 |
| 20 | `remove` | 26 |
| 21 | `mmap` | 25 |
| 22 | `va_start` | 25 |
| 23 | `va_end` | 25 |
| 24 | `fputs` | 22 |
| 25 | `fwrite` | 22 |
| 26 | `CHECK` | 19 |
| 27 | `for_each_elem` | 19 |
| 28 | `qsort` | 18 |
| 29 | `longjmp` | 17 |
| 30 | `open` | 17 |
| 31 | `mcc_warning_c` | 17 |
| 32 | `munmap` | 16 |
| 33 | `sqrt` | 16 |
| 34 | `close` | 16 |
| 35 | `strdup` | 16 |
| 36 | `atoi` | 15 |
| 37 | `fp` | 15 |
| 38 | `setjmp` | 14 |
| 39 | `fputc` | 14 |
| 40 | `va_arg` | 14 |

## Recursive functions (132)

`Hanoi`, `_tcschr`, `_tcspbrk`, `_tcsrchr`, `_tcsstr`, `aggr_has_const_member_rec`, `arm64_hfa_aux`, `asm_expr_unary`, `ast_bf_has_label`, `ast_cprop_arm_clean`, `ast_cprop_opaque`, `ast_cprop_rewrite`, `ast_cse_stmts`, `ast_cse_subst`, `ast_dep_affine_acc`, `ast_dep_collect_rec`, `ast_dse_kill_reads`, `ast_dup_sub`, `ast_eval_slice_kind_ok`, `ast_eval_slice_livein`, `ast_eval_slice_rec`, `ast_eval_slice_wtype`, `ast_fold_rec`, `ast_hash_of`, `ast_ident_etype`, `ast_ident_rec`, `ast_ident_same_scan`, `ast_ih_node`, `ast_inline_copy_expr`, `ast_inline_expr_pure`, `ast_interchange_body_ok`, `ast_ivsr_count_writes`, `ast_ivsr_scan`, `ast_li_refs_off`, `ast_licm_operands_ok`, `ast_licm_subst`, `ast_licm_written`, `ast_loop_has_unstructured`, `ast_ltemp_count_occ`, `ast_ltemp_scan`, `ast_narrow_rec`, `ast_pre_occurs`, `ast_promo_weigh`, `ast_replay_bb`, `ast_replay_value`, `ast_sel_safe`, `ast_sethi_num`, `ast_slice_copy_into`, `ast_slice_maximal_rec`, `ast_slice_subtree_size`, `ast_slice_walk`, `ast_subtree_reads_local`, `ast_subtree_refs_local`, `ast_subtree_span`, `ast_tco_reads_off`, `block`, `check_fields`, `classify_x86_64_inner`, `combo_dfs`, `combo_product_deepen`, `create_seq`, `create_trie`, `cst_binding_free`, `cst_check_tiling`, `cst_compute_struct`, `cst_compute_trivia`, `cst_index_walk`, `cst_materialize`, `cst_reflect_walk`, `cst_rehash_dirty`, `cst_render_node`, `csum`, `decl`, `decl_initializer`, `deep`, `dump_rec`, `expr_cond`, `expr_infix`, `fact`, `factorial`, `fib`, `find`, `find_field`, `frexp`, `func_vla_arg_code`, `fuzz_expr`, `fuzz_stmt`, `gcase`, `gen_op`, `gen_opil`, `gv_dup`, `host_dir_walk`, `host_rmrf`, `hv_emit_tree`, `inif`, `init_putv`, `inloop`, `ld_add_file_list`, `load`, `macho_load_dll`, `mcc_debug_finish`, `mcc_get_debug_info`, `mcc_get_dwarf_info`, `mccjit_build_rec`, `mccjit_find_kind`, `mccjit_slice_walk_eq`, `mccjit_subtree_count`, `mixed`, `move_ref_to_global`, `node_free`, `nontail`, `pkg_copy_cb`, `pmatch`, `post_type`, `quicksort`, `reg_pass_rec`, `reorder`, `rot3`, `s6_2_1_recurse`, `sizeof_test`, `splay_printtree`, `ssum`, `stage_cb`, `suma`, `swap2`, `tgamma`, `ts_fnmatch`, `type_decl_1`, `type_size`, `type_to_str`, `unary`, `usum`

## Ambiguous names (296)

_Same function name defined in multiple files; call-graph edges to these names are aggregated across all definitions._

| Name | Defs | Locations |
| --- | --- | --- |
| `main` | 509 | examples/ex1.c:4, examples/ex2.c:65, examples/ex3.c:10, examples/ex4.c:6, examples/ex5.c:4, runtime/win32/examples/fib.c:11 … |
| `f` | 19 | tests/cst/hashinv/changed.c:1, tests/cst/hashinv/compact.c:1, tests/cst/hashinv/spaced.c:1, tests/diagnostics/dg-error/function_redefinition.c:2, tests/diagnostics/dg-error/function_redefinition.c:3, tests/diagnostics/dg-error/too_many_arguments.c:2 … |
| `__attribute__` | 16 | runtime/lib/mccasan.c:107, runtime/lib/unwind-stub.c:22, runtime/lib/unwind-stub.c:27, runtime/lib/unwind-stub.c:35, runtime/lib/unwind-stub.c:41, tests/cli/vis.c:1 … |
| `BUILTIN` | 15 | runtime/lib/builtin.c:70, runtime/lib/builtin.c:74, runtime/lib/builtin.c:82, runtime/lib/builtin.c:86, runtime/lib/builtin.c:94, runtime/lib/builtin.c:98 … |
| `foo` | 15 | tests/exec/codegen/dead_code.c:20, tests/exec/codegen/nodata_wanted.c:2, tests/exec/codegen/nodata_wanted.c:9, tests/exec/codegen/nodata_wanted.c:20, tests/exec/errors/errors_and_warnings.c:152, tests/exec/errors/errors_and_warnings.c:161 … |
| `f1` | 9 | tests/diff/parts/legacy_expr.h:213, tests/exec/bounds/backtrace.c:13, tests/exec/bounds/backtrace.c:31, tests/exec/bounds/backtrace.c:52, tests/exec/errors/errors_and_warnings.c:327, tests/exec/structs_unions/return_struct_in_reg.c:38 … |
| `f2` | 8 | tests/exec/bounds/backtrace.c:9, tests/exec/bounds/backtrace.c:47, tests/exec/errors/errors_and_warnings.c:66, tests/exec/errors/errors_and_warnings.c:335, tests/exec/structs_unions/return_struct_in_reg.c:39, tests/exec/structs_unions/struct_byval.c:37 … |
| `add` | 7 | tests/ast/replay/inline.c:1, tests/cst/fixtures/basic.c:7, tests/embed/api_basic.c:10, tests/embed/api_threaded.c:54, tests/exec/bounds/bound_signal.c:18, tests/exec/functions_abi/func_pointers.c:3 … |
| `is_id` | 7 | tools/ckconfig.c:42, tools/ckretired.c:13, tools/deadgate.c:5, tools/hostgate.c:10, tools/idiomgate.c:17, tools/targetgate.c:11 … |
| `fib` | 6 | examples/ex3.c:3, runtime/win32/examples/fib.c:4, tests/diff/parts/legacy_numeric.h:59, tests/embed/api_threaded.c:330, tests/qemu/conformance/control.c:1, tests/sanitize/smoke.c:3 |
| `store` | 6 | src/arch/arm/arm-gen.c:548, src/arch/arm64/arm64-gen.c:670, src/arch/i386/i386-gen.c:307, src/arch/riscv64/riscv64-gen.c:358, src/arch/x86_64/x86_64-gen.c:480, tests/exec/programs/grep.c:249 |
| `gfunc_sret` | 6 | src/arch/arm/arm-gen.c:792, src/arch/arm64/arm64-gen.c:1635, src/arch/i386/i386-gen.c:426, src/arch/riscv64/riscv64-gen.c:810, src/arch/x86_64/x86_64-gen.c:739, src/arch/x86_64/x86_64-gen.c:1163 |
| `gfunc_call` | 6 | src/arch/arm/arm-gen.c:1037, src/arch/arm64/arm64-gen.c:1078, src/arch/i386/i386-gen.c:457, src/arch/riscv64/riscv64-gen.c:531, src/arch/x86_64/x86_64-gen.c:771, src/arch/x86_64/x86_64-gen.c:1208 |
| `gfunc_prolog` | 6 | src/arch/arm/arm-gen.c:1096, src/arch/arm64/arm64-gen.c:1346, src/arch/i386/i386-gen.c:579, src/arch/riscv64/riscv64-gen.c:739, src/arch/x86_64/x86_64-gen.c:893, src/arch/x86_64/x86_64-gen.c:1455 |
| `gfunc_epilog` | 6 | src/arch/arm/arm-gen.c:1200, src/arch/arm64/arm64-gen.c:1683, src/arch/i386/i386-gen.c:658, src/arch/riscv64/riscv64-gen.c:844, src/arch/x86_64/x86_64-gen.c:958, src/arch/x86_64/x86_64-gen.c:1599 |
| `gen_opf` | 6 | src/arch/arm/arm-gen.c:1482, src/arch/arm/arm-gen.c:1630, src/arch/arm64/arm64-gen.c:2127, src/arch/i386/i386-gen.c:848, src/arch/riscv64/riscv64-gen.c:1229, src/arch/x86_64/x86_64-gen.c:1890 |
| `rt_get_caller_pc` | 6 | src/mccrun.c:1042, src/mccrun.c:1060, src/mccrun.c:1078, src/mccrun.c:1096, src/mccrun.c:1115, src/mccrun.c:1121 |
| `slurp` | 6 | tests/cli/runner.c:153, tests/diff3/runner.c:90, tests/exec/runner.c:149, tests/fuzz/runner.c:42, tests/support/seccmp.c:20, tools/objcheck.c:8 |
| `scan_file` | 6 | tools/ckretired.c:27, tools/deadgate.c:47, tools/hostgate.c:76, tools/idiomgate.c:124, tools/targetgate.c:82, tools/tracegate.c:210 |
| `gen_le32` | 5 | src/arch/arm/arm-asm.c:112, src/arch/arm64/arm64-asm.c:96, src/arch/i386/i386-gen.c:35, src/arch/riscv64/riscv64-asm.c:82, src/arch/x86_64/x86_64-gen.c:63 |
| `mcc_disasm_insn` | 5 | src/arch/arm/arm-dis.c:606, src/arch/arm64/arm64-dis.c:881, src/arch/i386/i386-dis.c:903, src/arch/riscv64/riscv64-dis.c:499, src/arch/x86_64/x86_64-dis.c:1134 |
| `mcc_disasm_reloc_size` | 5 | src/arch/arm/arm-dis.c:638, src/arch/arm64/arm64-dis.c:904, src/arch/i386/i386-dis.c:911, src/arch/riscv64/riscv64-dis.c:503, src/arch/x86_64/x86_64-dis.c:1142 |
| `mcc_disasm_reloc_addend_bias` | 5 | src/arch/arm/arm-dis.c:652, src/arch/arm64/arm64-dis.c:936, src/arch/i386/i386-dis.c:932, src/arch/riscv64/riscv64-dis.c:514, src/arch/x86_64/x86_64-dis.c:1161 |
| `o` | 5 | src/arch/arm/arm-gen.c:87, src/arch/arm64/arm64-gen.c:80, src/arch/i386/i386-gen.c:28, src/arch/riscv64/riscv64-gen.c:74, src/arch/x86_64/x86_64-gen.c:56 |
| `gsym_addr` | 5 | src/arch/arm/arm-gen.c:231, src/arch/arm64/arm64-gen.c:213, src/arch/i386/i386-gen.c:42, src/arch/riscv64/riscv64-gen.c:106, src/arch/x86_64/x86_64-gen.c:91 |
| `load` | 5 | src/arch/arm/arm-gen.c:405, src/arch/arm64/arm64-gen.c:504, src/arch/i386/i386-gen.c:166, src/arch/riscv64/riscv64-gen.c:196, src/arch/x86_64/x86_64-gen.c:225 |
| `gen_bounds_prolog` | 5 | src/arch/arm/arm-gen.c:677, src/arch/arm64/arm64-gen.c:795, src/arch/i386/i386-gen.c:1054, src/arch/riscv64/riscv64-gen.c:430, src/arch/x86_64/x86_64-gen.c:617 |
| `gen_bounds_epilog` | 5 | src/arch/arm/arm-gen.c:688, src/arch/arm64/arm64-gen.c:805, src/arch/i386/i386-gen.c:1070, src/arch/riscv64/riscv64-gen.c:440, src/arch/x86_64/x86_64-gen.c:626 |
| `gen_fill_nops` | 5 | src/arch/arm/arm-gen.c:1241, src/arch/arm64/arm64-gen.c:1714, src/arch/i386/i386-gen.c:51, src/arch/riscv64/riscv64-gen.c:889, src/arch/x86_64/x86_64-gen.c:1631 |
| `gjmp` | 5 | src/arch/arm/arm-gen.c:1250, src/arch/arm64/arm64-gen.c:1723, src/arch/i386/i386-gen.c:699, src/arch/riscv64/riscv64-gen.c:898, src/arch/x86_64/x86_64-gen.c:1636 |
| `gjmp_addr` | 5 | src/arch/arm/arm-gen.c:1259, src/arch/arm64/arm64-gen.c:1731, src/arch/i386/i386-gen.c:703, src/arch/riscv64/riscv64-gen.c:905, src/arch/x86_64/x86_64-gen.c:1640 |
| `gjmp_cond` | 5 | src/arch/arm/arm-gen.c:1263, src/arch/arm64/arm64-gen.c:1764, src/arch/i386/i386-gen.c:714, src/arch/riscv64/riscv64-gen.c:927, src/arch/x86_64/x86_64-gen.c:1659 |
| `gen_opi` | 5 | src/arch/arm/arm-gen.c:1290, src/arch/arm64/arm64-gen.c:2104, src/arch/i386/i386-gen.c:720, src/arch/riscv64/riscv64-gen.c:1208, src/arch/x86_64/x86_64-gen.c:1740 |
| `gen_cvt_itof` | 5 | src/arch/arm/arm-gen.c:1823, src/arch/arm64/arm64-gen.c:2287, src/arch/i386/i386-gen.c:956, src/arch/riscv64/riscv64-gen.c:1377, src/arch/x86_64/x86_64-gen.c:2129 |
| `gen_cvt_ftoi` | 5 | src/arch/arm/arm-gen.c:1900, src/arch/arm64/arm64-gen.c:2314, src/arch/i386/i386-gen.c:981, src/arch/riscv64/riscv64-gen.c:1400, src/arch/x86_64/x86_64-gen.c:2222 |
| `gen_cvt_ftof` | 5 | src/arch/arm/arm-gen.c:1954, src/arch/arm64/arm64-gen.c:2340, src/arch/i386/i386-gen.c:997, src/arch/riscv64/riscv64-gen.c:1423, src/arch/x86_64/x86_64-gen.c:2165 |
| `gen_increment_tcov` | 5 | src/arch/arm/arm-gen.c:1966, src/arch/arm64/arm64-gen.c:2370, src/arch/i386/i386-gen.c:1009, src/arch/riscv64/riscv64-gen.c:1465, src/arch/x86_64/x86_64-gen.c:2274 |
| `ggoto` | 5 | src/arch/arm/arm-gen.c:1987, src/arch/arm64/arm64-gen.c:2383, src/arch/i386/i386-gen.c:1033, src/arch/riscv64/riscv64-gen.c:1490, src/arch/x86_64/x86_64-gen.c:2281 |
| `gen_vla_sp_save` | 5 | src/arch/arm/arm-gen.c:1992, src/arch/arm64/arm64-gen.c:2435, src/arch/i386/i386-gen.c:1116, src/arch/riscv64/riscv64-gen.c:1495, src/arch/x86_64/x86_64-gen.c:2286 |
| `gen_vla_sp_restore` | 5 | src/arch/arm/arm-gen.c:2000, src/arch/arm64/arm64-gen.c:2441, src/arch/i386/i386-gen.c:1120, src/arch/riscv64/riscv64-gen.c:1504, src/arch/x86_64/x86_64-gen.c:2290 |

