






#include <stdio.h>

static int g2[8][8];
static int g3[4][4][4];

static int diag_read(void) {
	int i, j, s = 0;
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			g2[i][j] = i * 10 + j;
	for (i = 0; i < 8; i++)
		s = s + g2[i][i];
	return s;
}

static int diag_compound(void) {
	int i, j, s = 0;
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			g2[i][j] = 0;
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			g2[i][j] += i + j;
	for (i = 0; i < 8; i++)
		s += g2[i][i];
	return s;
}

static int anti_diag(void) {
	int i, s = 0;
	for (i = 0; i < 8; i++)
		s += g2[i][7 - i];
	return s;
}

static int row_then_col(void) {
	int i, s = 0;
	for (i = 0; i < 8; i++)
		s += g2[3][i];
	for (i = 0; i < 8; i++)
		s += g2[i][3];
	return s;
}

static int three_d(void) {
	int i, j, k, s = 0;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k++)
				g3[i][j][k] = i * 100 + j * 10 + k;
	for (i = 0; i < 4; i++)
		s += g3[i][i][i];
	for (i = 0; i < 4; i++)
		s += g3[i][i][0];
	return s;
}

static int local_2d(void) {
	int a[6][6];
	int i, j, s = 0;
	for (i = 0; i < 6; i++)
		for (j = 0; j < 6; j++)
			a[i][j] = i * j + 1;
	for (i = 0; i < 6; i++)
		s += a[i][i];
	return s;
}

static int reverse_diag(void) {
	int i, s = 0;
	for (i = 7; i >= 0; i--)
		s += g2[i][i];
	return s;
}

static int ptr_row_walk(void) {
	int i, s = 0;
	for (i = 0; i < 8; i++) {
		int *r = g2[i];
		s += r[i] + r[0];
	}
	return s;
}

int main(void) {
	printf("diag_read %d\n", diag_read());
	printf("diag_compound %d\n", diag_compound());
	printf("anti_diag %d\n", anti_diag());
	printf("row_then_col %d\n", row_then_col());
	printf("three_d %d\n", three_d());
	printf("local_2d %d\n", local_2d());
	printf("reverse_diag %d\n", reverse_diag());
	printf("ptr_row_walk %d\n", ptr_row_walk());
	return 0;
}
