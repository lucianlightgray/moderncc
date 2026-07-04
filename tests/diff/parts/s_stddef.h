struct s_stddef_st {
	char c;
	int i;
	double d;
	char arr[8];
};

void s_stddef_offsetof(void) {
	struct s_stddef_st v;
	printf("s_stddef off %d %d %d %d %d\n",
		   (int)offsetof(struct s_stddef_st, c) == (int)((char *)&v.c - (char *)&v),
		   (int)offsetof(struct s_stddef_st, i) == (int)((char *)&v.i - (char *)&v),
		   (int)offsetof(struct s_stddef_st, d) == (int)((char *)&v.d - (char *)&v),
		   (int)offsetof(struct s_stddef_st, arr) == (int)((char *)&v.arr - (char *)&v),
		   (int)offsetof(struct s_stddef_st, arr[3]) == (int)((char *)&v.arr[3] - (char *)&v));
}

void s_stddef_stdint(void) {

	printf("s_stddef sz %d %d %d %d %d %d %d %d\n",
		   (int)sizeof(int8_t), (int)sizeof(int16_t), (int)sizeof(int32_t), (int)sizeof(int64_t),
		   (int)sizeof(uint8_t), (int)sizeof(uint16_t), (int)sizeof(uint32_t), (int)sizeof(uint64_t));

	printf("s_stddef lim %lld %lld %llu %lld %lld %llu %lld %lld %llu %lld %lld %llu\n",
		   (long long)INT8_MIN, (long long)INT8_MAX, (unsigned long long)UINT8_MAX,
		   (long long)INT16_MIN, (long long)INT16_MAX, (unsigned long long)UINT16_MAX,
		   (long long)INT32_MIN, (long long)INT32_MAX, (unsigned long long)UINT32_MAX,
		   (long long)INT64_MIN, (long long)INT64_MAX, (unsigned long long)UINT64_MAX);

	printf("s_stddef c %d %d %d %d %d\n",
		   (int)(INT32_C(1000000) == 1000000),
		   (int)((UINT64_C(0) - UINT64_C(1)) > UINT64_C(0)),
		   (int)((UINT32_C(0) - UINT32_C(1)) > UINT32_C(0)),
		   (int)(INTMAX_C(5) == 5),
		   (int)((UINTMAX_C(0) - UINTMAX_C(1)) > UINTMAX_C(0)));

	printf("s_stddef minv %d %d %d %d %d %d %d\n",
		   (int)(SIZE_MAX >= 65535u),
		   (int)(PTRDIFF_MIN <= -65535 && PTRDIFF_MAX >= 65535),
		   (int)(INTMAX_MIN <= -9223372036854775807LL && INTMAX_MAX >= 9223372036854775807LL),
		   (int)(UINTMAX_MAX >= 18446744073709551615ULL),
		   (int)(INTPTR_MIN <= -32767 && INTPTR_MAX >= 32767),
		   (int)(UINTPTR_MAX >= 65535u),
		   (int)(sizeof(int_least64_t) >= 8 && sizeof(int_fast32_t) >= 4));
}
