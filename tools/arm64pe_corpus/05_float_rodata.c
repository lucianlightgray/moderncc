/* Floating point + rodata literal pool: fp constants land in a rodata section
   referenced from .text via ADRP/GOT. Verifies fp codegen matches and that the
   literal-pool addressing diverges only benignly (GOT-vs-direct). */
double poly(double x) { return 3.14159 * x * x + 2.71828 * x - 0.5; }
float fmix(float a, float b) { return a * 1.5f + b / 2.25f; }
int fcmp(double x) { return x > 1.0 ? 1 : (x < -1.0 ? -1 : 0); }
