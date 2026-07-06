#include <stdio.h>

int main(int argc, char **argv) {
	FILE *fp, *op;
	unsigned long n = 0;
	int c;

	if (argc < 4) {
		fprintf(stderr, "usage: bin2c <in> <out.c> <sym>\n");
		return 1;
	}

	fp = fopen(argv[1], "rb");
	op = fopen(argv[2], "wb");
	if (!fp || !op) {
		fprintf(stderr, "bin2c: file error\n");
		return 1;
	}

	fprintf(op, "/* Generated from %s by tools/bin2c.c — do not edit. */\n", argv[1]);
	fprintf(op, "const unsigned char %s[] = {\n", argv[3]);
	while ((c = getc(fp)) != EOF) {
		fprintf(op, "0x%02x,", (unsigned char)c);
		if (++n % 16 == 0)
			putc('\n', op);
	}
	fprintf(op, "\n};\n");
	fprintf(op, "const unsigned int %s_len = %luu;\n", argv[3], n);

	fclose(fp);
	fclose(op);
	return 0;
}
