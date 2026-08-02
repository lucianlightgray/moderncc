extern int puts(const char *);
extern int g_counter;
extern const char *g_name;

int use(void) {
    g_counter += 1;
    puts(g_name ? g_name : "hi");
    return g_counter;
}
