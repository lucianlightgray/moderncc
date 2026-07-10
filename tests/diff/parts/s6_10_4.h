#define s6_10_4_LN 4000
#define s6_10_4_DOPRAGMA(x) _Pragma(#x)

void s6_10_4_preproc_test(void) {

#ifdef __STDC__
	printf("__STDC__=%d\n", __STDC__);
#else
	printf("__STDC__=%d\n", 0);
#endif
	printf("__STDC_HOSTED__=%d\n", __STDC_HOSTED__);
#ifdef __STDC_VERSION__
	printf("__STDC_VERSION__=%ldL\n", __STDC_VERSION__);
#else
	printf("__STDC_VERSION__=%ldL\n", 0L);
#endif

	printf("date_len=%d time_len=%d\n", (int)sizeof(__DATE__) - 1, (int)sizeof(__TIME__) - 1);
	printf("time_colons=%d %d\n", __TIME__[2] == ':', __TIME__[5] == ':');
	printf("date_sp=%d\n", __DATE__[3] == ' ');

	printf("func=%s\n", __func__);

#
	printf("after_null=1\n");

#line 3000
	printf("line_p2=%d\n", __LINE__);

#line 3100
	printf("line_p3a=%d\n", __LINE__);

#line s6_10_4_LN
	printf("line_p4=%d\n", __LINE__);

#pragma s6_10_4_bogus_pragma_name 12 34
	s6_10_4_DOPRAGMA(s6_10_4_unknown_via_operator)
			_Pragma("s6_10_4_also_unknown")
					printf("pragma_ok=1\n");
}
