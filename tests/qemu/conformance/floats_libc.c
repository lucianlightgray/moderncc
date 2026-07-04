#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	char buf[64];

	long double ld = 3.5L;
	snprintf(buf, sizeof buf, "%.2Lf", ld);
	if (strcmp(buf, "3.50"))
		return 1;

	snprintf(buf, sizeof buf, "%.1f %.1Lf", 1.5, 2.5L);
	if (strcmp(buf, "1.5 2.5"))
		return 2;

	snprintf(buf, sizeof buf, "%d %.1f %.1Lf", 7, 0.5, 0.25L);
	if (strcmp(buf, "7 0.5 0.2") && strcmp(buf, "7 0.5 0.3"))
		return 3;

	char *end;
	double v = strtod("2.5", &end);
	if (v != 2.5 || *end != '\0')
		return 4;

	long double lv = strtold("6.5", &end);
	if (lv != 6.5L || *end != '\0')
		return 5;

	return 0;
}
