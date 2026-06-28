


#ifndef INC42_FIRST
int have_included_42test_h;
#define INC42_FIRST
#elif !defined INC42_SECOND
#define INC42_SECOND
int have_included_42test_h_second;
#else
#define INC42_THIRD
int have_included_42test_h_third;
#endif
