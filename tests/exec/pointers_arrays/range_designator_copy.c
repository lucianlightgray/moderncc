int elems(char *p);
void emit(int v);
void nl(void);

int main(void) {
	int i = 0, j = 1, k;
	char m[][2][3] = {[0 ... 2] = {{3, 4, 5}, {6, 7, 8}}};
	for (k = 0; k < elems(m[0][0]); k++)
		emit(m[i][j][k]);
	nl();
	return 0;
}

#include <stdio.h>

int elems(char *p) {
	return p[0] ? 3 : 3;
}

void emit(int v) {
	printf(" %d", v);
}

void nl(void) {
	printf("\n");
}
