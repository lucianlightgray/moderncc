static _Bool gb_init = (_BitInt(128))5;
static _Bool gb_hi_init = (_BitInt(128))1 << 100;
static _Bool gb_assign;
static int gi_assign;

int main(void) {
	_BitInt(128) v5 = 5;
	_BitInt(128) vhi = (_BitInt(128))1 << 100;
	_BitInt(128) big = ((_BitInt(128))0x1122334455667788ULL << 64) | (_BitInt(128))0x99aabbccddeeff00ULL;

	_Bool lb_init = v5;
	_Bool lb_hi_init = vhi;
	_Bool lb_big_init = big;
	int li_init = v5;
	int li_hi_init = vhi;
	int li_big_init = big;
	long ll_big_init = big;
	short ls_big_init = big;
	char lc_big_init = big;

	_Bool lb_a;
	int li_a;
	short ls_a;
	char lc_a;
	lb_a = v5;
	li_a = big;
	ls_a = big;
	lc_a = v5;
	gb_assign = v5;
	gi_assign = big;

	if (gb_init != 1) return 1;
	if (gb_hi_init != 1) return 2;
	if (lb_init != 1) return 3;
	if (lb_hi_init != 1) return 4;
	if (lb_big_init != 1) return 5;
	if (li_init != 5) return 7;
	if (li_hi_init != 0) return 8;
	if (li_big_init != -571539712) return 9;
	if (ll_big_init != -7373874951294615808L) return 10;
	if (ls_big_init != (short)0xff00) return 11;
	if (lc_big_init != 0) return 12;
	if (lb_a != 1) return 13;
	if (li_a != -571539712) return 14;
	if (ls_a != (short)0xff00) return 15;
	if (lc_a != 5) return 16;
	if (gb_assign != 1) return 17;
	if (gi_assign != -571539712) return 18;
	if (*(unsigned char *)&lb_a != 1) return 19;
	if (*(unsigned char *)&gb_assign != 1) return 20;
	return 0;
}
