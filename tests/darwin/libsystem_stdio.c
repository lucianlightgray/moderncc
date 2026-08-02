


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

#define CHECK_STR(got, want) do { \
	if (strcmp((got), (want))) { \
		fprintf(stderr, "FAIL %s:%d: got \"%s\" want \"%s\"\n", \
				__FILE__, __LINE__, (got), (want)); \
		fails++; \
	} \
} while (0)

int main(void) {
	char b[256];

	CHECK(setlocale(LC_ALL, "C") != NULL);



	snprintf(b, sizeof b, "%f", 3.5);            CHECK_STR(b, "3.500000");
	snprintf(b, sizeof b, "%.3f", 2.0 / 3.0);    CHECK_STR(b, "0.667");
	snprintf(b, sizeof b, "%e", 1234.5);         CHECK_STR(b, "1.234500e+03");
	snprintf(b, sizeof b, "%g", 0.0001);         CHECK_STR(b, "0.0001");
	snprintf(b, sizeof b, "%g", 0.00001);        CHECK_STR(b, "1e-05");
	snprintf(b, sizeof b, "%a", 1.0);            CHECK_STR(b, "0x1p+0");
	snprintf(b, sizeof b, "%.17g", 0.1);         CHECK_STR(b, "0.10000000000000001");
	snprintf(b, sizeof b, "%08.2f|%+d|%5s|%-5s|", -1.5, 42, "ab", "cd");
	CHECK_STR(b, "-0001.50|+42|   ab|cd   |");
	snprintf(b, sizeof b, "%ld %llu %x %#o %p",
			 (long)-1, (unsigned long long)18446744073709551615ULL,
			 255u, 8u, (void *)0);
	CHECK_STR(b, "-1 18446744073709551615 ff 010 0x0");



	char small[4];
	int n = snprintf(small, sizeof small, "%d", 123456);
	CHECK(n == 6);
	CHECK_STR(small, "123");

	FILE *f = tmpfile();
	CHECK(f != NULL);
	if (f) {
		CHECK(fprintf(f, "%s %d %.2f\n", "line", 7, 1.25) == 12);
		CHECK(fputs("second\n", f) >= 0);
		CHECK(fwrite("raw", 1, 3, f) == 3);
		CHECK(fflush(f) == 0);

		long end = ftell(f);
		CHECK(end == 22);
		CHECK(fseek(f, 0, SEEK_SET) == 0);

		char word[32];
		int d;
		double g;
		CHECK(fscanf(f, "%31s %d %lf", word, &d, &g) == 3);
		CHECK_STR(word, "line");
		CHECK(d == 7);
		CHECK(g == 1.25);

		CHECK(fgets(b, sizeof b, f) != NULL);
		CHECK_STR(b, "\n");
		CHECK(fgets(b, sizeof b, f) != NULL);
		CHECK_STR(b, "second\n");

		int c = fgetc(f);
		CHECK(c == 'r');
		CHECK(ungetc(c, f) == 'r');
		CHECK(fgetc(f) == 'r');

		CHECK(fseek(f, 0, SEEK_END) == 0);
		CHECK(feof(f) == 0);
		CHECK(fgetc(f) == EOF);
		CHECK(feof(f) != 0);
		clearerr(f);
		CHECK(feof(f) == 0);
		fclose(f);
	}



	FILE *g2 = tmpfile();
	CHECK(g2 != NULL);
	if (g2) {
		static char buf[BUFSIZ];
		CHECK(setvbuf(g2, buf, _IOFBF, sizeof buf) == 0);
		for (int i = 0; i < 1000; i++)
			fprintf(g2, "%d\n", i);
		CHECK(fflush(g2) == 0);
		rewind(g2);
		int last = -1, v;
		while (fscanf(g2, "%d", &v) == 1)
			last = v;
		CHECK(last == 999);
		fclose(g2);
	}

	errno = 0;
	CHECK(fopen("/nonexistent/mcc-darwin-test", "r") == NULL);
	CHECK(errno == ENOENT);
	CHECK(strcmp(strerror(ENOENT), "") != 0);

	if (fails) {
		fprintf(stderr, "libsystem_stdio: %d failure(s)\n", fails);
		return 1;
	}
	printf("libsystem_stdio: OK\n");
	return 0;
}
