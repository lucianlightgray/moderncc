/* Exercises the imported-symbol / import-thunk path (the class that surfaced
   the stale-.text corruption, commit 80ea9843). Calls an external function and
   touches an external global: ELF resolves these GOT-indirect, arm64-PE direct
   / via import thunks in the final link. The reloc classifier should mark the
   GOT-vs-direct swaps BENIGN and the CALL26 to `puts` IDENTICAL. */
extern int puts(const char *);
extern int g_counter;
extern const char *g_name;

int use(void) {
    g_counter += 1;
    puts(g_name ? g_name : "hi");
    return g_counter;
}
