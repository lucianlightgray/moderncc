extern unsigned char al_a[], al_b[], al_c[];

int main(void) {
	if ((unsigned long)al_b - (unsigned long)al_a != 4) return 1;
	if ((unsigned long)al_c - (unsigned long)al_b != 4) return 2;
	if ((unsigned long)al_c - (unsigned long)al_a != 8) return 3;
	if (al_a[0] != 0x11) return 4;
	if (al_b[0] != 0x22) return 5;
	if (al_c[0] != 0x33) return 6;
	return 0;
}
