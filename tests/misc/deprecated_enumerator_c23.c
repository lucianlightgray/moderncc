enum F { C [[deprecated]], D };
int use(void) { return (int)C + (int)D; }
