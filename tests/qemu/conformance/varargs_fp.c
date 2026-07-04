#include <stdio.h>
#include <string.h>

int main(void) {
	char buf[128];

	snprintf(buf, sizeof buf, "%.2f", 3.5);
	if (strcmp(buf, "3.50"))
		return 1;

	snprintf(buf, sizeof buf, "%d %.1f %d %.1f", 1, 2.5, 3, 4.5);
	if (strcmp(buf, "1 2.5 3 4.5"))
		return 2;

	snprintf(buf, sizeof buf,
			 "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
			 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5);
	if (strcmp(buf, "0.5 1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5"))
		return 3;

	snprintf(buf, sizeof buf, "%g %e", 100.0, 0.5);
	if (strcmp(buf, "100 5.000000e-01") && strcmp(buf, "100 5.000000e-001"))
		return 4;

	snprintf(buf, sizeof buf, "%lld %.1f", 0x100000000LL, 0.5);
	if (strcmp(buf, "4294967296 0.5"))
		return 5;

	float fv = 1.25f;
	snprintf(buf, sizeof buf, "%.2f", fv);
	if (strcmp(buf, "1.25"))
		return 6;

	snprintf(buf, sizeof buf, "%.1f %d %.1f %d %.1f",
			 0.5, 7, 1.5, 8, 2.5);
	if (strcmp(buf, "0.5 7 1.5 8 2.5"))
		return 7;

	return 0;
}
