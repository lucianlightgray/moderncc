enum E { A __attribute__((deprecated)), B };
enum F { C [[deprecated]], D };
int main(void) { return ((int)B - 1) + ((int)D - 1); }
