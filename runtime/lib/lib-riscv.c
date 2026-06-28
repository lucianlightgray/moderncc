void __clear_cache(void *beg, void *end)
{
    __riscv64_clear_cache(beg, end);
}
