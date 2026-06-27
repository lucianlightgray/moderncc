/* Helper header for 188_include_header.c -- exercises C99 6.10.2 source file
   inclusion via a quoted #include, with a classic include guard so a double
   include is harmless. */
#ifndef INC188_H
#define INC188_H

#define INC188_ANSWER 42

static int inc188_triple(int x) { return x * 3; }

#endif /* INC188_H */
