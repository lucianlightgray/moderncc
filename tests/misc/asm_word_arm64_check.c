extern unsigned char w0[], w1[], w2[], dtgt[], dwrd[], dend[];
int main(void){
	if (*(unsigned*)w0 != 0x11223344u) return 1;
	if ((unsigned long)w1 - (unsigned long)w0 != 4) return 2;
	if ((unsigned long)w2 - (unsigned long)w1 != 2) return 3;
	if (*(unsigned short*)w1 != 0xAABBu) return 4;
	if (*(int*)dwrd != (int)((unsigned long)dtgt - (unsigned long)w0)) return 5;
	if ((unsigned long)dend - (unsigned long)dwrd != 4) return 6;
	return 0;
}
