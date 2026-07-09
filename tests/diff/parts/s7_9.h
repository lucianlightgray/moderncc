void s7_9_iso646_test(void) {
	int a = 6, b = 3;

	printf("and=%d or=%d not=%d not_eq=%d\n",
				 (a and b) ? 1 : 0, (0 or b) ? 1 : 0, (not 0), (a not_eq b));

	printf("bitand=%d bitor=%d xor=%d compl=%d\n",
				 (a bitand b), (a bitor b), (a xor b), (compl a));

	{
		int x = 12;
		x and_eq 10;
		printf("and_eq=%d\n", x);
	}
	{
		int x = 12;
		x or_eq 1;
		printf("or_eq=%d\n", x);
	}
	{
		int x = 12;
		x xor_eq 5;
		printf("xor_eq=%d\n", x);
	}
#if INT_MAX >= 32767 and LONG_MAX > 0 or 0
	printf("iso646_in_if=1\n");
#else
	printf("iso646_in_if=0\n");
#endif
}

void s7_9_limits_test(void) {

	printf("CB=%d SCMIN=%d SCMAX=%d UCMAX=%d\n",
				 CHAR_BIT >= 8,
				 SCHAR_MIN <= -127,
				 SCHAR_MAX >= 127,
				 UCHAR_MAX >= 255);
	printf("SHMIN=%d SHMAX=%d USHMAX=%d MBLEN=%d\n",
				 SHRT_MIN <= -32767,
				 SHRT_MAX >= 32767,
				 USHRT_MAX >= 65535,
				 MB_LEN_MAX >= 1);
	printf("IMIN=%d IMAX=%d UIMAX=%d\n",
				 INT_MIN <= -32767,
				 INT_MAX >= 32767,
				 UINT_MAX >= 65535u);
	printf("LMIN=%d LMAX=%d ULMAX=%d\n",
				 LONG_MIN <= -2147483647L,
				 LONG_MAX >= 2147483647L,
				 ULONG_MAX >= 4294967295uL);
	printf("LLMIN=%d LLMAX=%d ULLMAX=%d\n",
				 LLONG_MIN <= -9223372036854775807LL,
				 LLONG_MAX >= 9223372036854775807LL,
				 ULLONG_MAX >= 18446744073709551615uLL);

	printf("CHAR_MIN_ok=%d CHAR_MAX_ok=%d\n",
				 (CHAR_MIN == SCHAR_MIN) || (CHAR_MIN == 0),
				 (CHAR_MAX == SCHAR_MAX) || (CHAR_MAX == UCHAR_MAX));

#if (INT_MAX >= 32767) && (LLONG_MIN < 0) && (ULLONG_MAX > 0)
	printf("limits_in_if=1\n");
#else
	printf("limits_in_if=0\n");
#endif

	printf("g_int=%d g_uint=%d g_ulong=%d g_llong=%d\n",
				 _Generic(INT_MAX, int: 1, default: 0),
				 _Generic(UINT_MAX, unsigned int: 1, default: 0),
				 _Generic(ULONG_MAX, unsigned long: 1, default: 0),
				 _Generic(LLONG_MAX, long long: 1, default: 0));
}

void s7_9_locale_test(void) {
	struct lconv *lc;
	char *r;

	int distinct =
			(LC_ALL != LC_COLLATE) && (LC_ALL != LC_CTYPE) &&
			(LC_ALL != LC_MONETARY) && (LC_ALL != LC_NUMERIC) &&
			(LC_ALL != LC_TIME) && (LC_COLLATE != LC_CTYPE) &&
			(LC_COLLATE != LC_MONETARY) && (LC_COLLATE != LC_NUMERIC) &&
			(LC_COLLATE != LC_TIME) && (LC_CTYPE != LC_MONETARY) &&
			(LC_CTYPE != LC_NUMERIC) && (LC_CTYPE != LC_TIME) &&
			(LC_MONETARY != LC_NUMERIC) && (LC_MONETARY != LC_TIME) &&
			(LC_NUMERIC != LC_TIME);
	printf("lc_distinct=%d\n", distinct);

	r = setlocale(LC_ALL, "C");
	printf("setC=%s\n", r ? r : "(null)");

	r = setlocale(LC_ALL, NULL);
	printf("query_nonnull=%d\n", r != NULL);

	r = setlocale(LC_ALL, "this_is_not_a_locale_name_at_all_xyzzy");
	printf("bogus_null=%d\n", r == NULL);

	lc = localeconv();
	printf("lc_nonnull=%d\n", lc != NULL);
	printf("dp=[%s] ts=[%s] grp_empty=%d\n",
				 lc->decimal_point, lc->thousands_sep, lc->grouping[0] == 0);
	printf("mdp_empty=%d cur_empty=%d ps_empty=%d ns_empty=%d\n",
				 lc->mon_decimal_point[0] == 0,
				 lc->currency_symbol[0] == 0,
				 lc->positive_sign[0] == 0,
				 lc->negative_sign[0] == 0);
	printf("frac=%d p_cs=%d int_frac=%d n_sign=%d\n",
				 lc->frac_digits == CHAR_MAX,
				 lc->p_cs_precedes == CHAR_MAX,
				 lc->int_frac_digits == CHAR_MAX,
				 lc->n_sign_posn == CHAR_MAX);
}
