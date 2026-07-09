static int s7_23_sign(int v) {
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

void s7_23_string_test(void) {
	char buf[16];

	strcpy(buf, "abcdefgh");
	void *mr = memmove(buf + 2, buf, 5);
	printf("memmove %s ret=%d\n", buf, mr == buf + 2);

	printf("memcmp %d %d\n", s7_23_sign(memcmp("abc", "abd", 3)),
				 s7_23_sign(memcmp("abd", "abc", 3)));
	printf("strcmp %d %d %d\n", s7_23_sign(strcmp("a", "b")),
				 s7_23_sign(strcmp("b", "a")),
				 s7_23_sign(strcmp("eq", "eq")));
	printf("strncmp %d %d\n", s7_23_sign(strncmp("abcX", "abcY", 3)),
				 s7_23_sign(strncmp("abcX", "abcY", 4)));

	char nb[8];
	memset(nb, '#', sizeof nb);
	strncpy(nb, "ABCDE", 3);
	printf("strncpy1 %c%c%c%c\n", nb[0], nb[1], nb[2], nb[3]);
	char nb2[6];
	memset(nb2, '#', sizeof nb2);
	strncpy(nb2, "AB", 5);
	printf("strncpy2 %d%d%d %c\n", nb2[2], nb2[3], nb2[4], nb2[5]);

	char cb[16];
	strcpy(cb, "go");
	strncat(cb, "XYZ", 2);
	printf("strncat %s len=%d\n", cb, (int)strlen(cb));

	char h[] = "banana";
	printf("strchr %d strrchr %d\n",
				 (int)(strchr(h, 'a') - h), (int)(strrchr(h, 'a') - h));
	char xyz[] = "xyz";
	printf("memchr %d\n", (int)((char *)memchr(xyz, 'y', 3) - xyz));
	char hel[] = "hello";
	printf("strpbrk %d\n", (int)(strpbrk(hel, "lo") - hel));
	printf("strspn %d strcspn %d\n",
				 (int)strspn("abcXYZ", "cba"), (int)strcspn("abcXYZ", "Z"));
	char s6[] = "abcdef";
	printf("strstr %d empty %d\n",
				 (int)(strstr(s6, "cd") - s6), (int)(strstr(s6, "") - s6));
	printf("strchr_null %d\n", strchr("abc", 'z') == NULL);

	char tb[] = ",,1,2,,3,";
	char out[32];
	out[0] = 0;
	for (char *t = strtok(tb, ","); t; t = strtok(NULL, ","))
		strcat(out, t);
	printf("strtok %s\n", out);

	char sb[4];
	printf("memset %d\n", memset(sb, 'Q', 4) == sb);

	const char *A = "apple", *B = "banana";
	char xa[64], xb[64];
	strxfrm(xa, A, sizeof xa);
	strxfrm(xb, B, sizeof xb);
	printf("xfrm %d\n", s7_23_sign(strcmp(xa, xb)) == s7_23_sign(strcoll(A, B)));
}

void s7_23_tgmath_test(void) {

	long double _Complex ldc = 0;
	float _Complex fc = 0;
	double _Complex dc = 0;

	printf("p3 %d %d %d %d\n",
				 (int)(sizeof(sqrt((float)1)) == sizeof(float)),
				 (int)(sizeof(sqrt((double)1)) == sizeof(double)),
				 (int)(sizeof(sqrt(1)) == sizeof(double)),
				 (int)(sizeof(sqrt((long double)1)) == sizeof(long double)));

	printf("p4 %d %d\n",
				 (int)(sizeof(sqrt(dc)) == 2 * sizeof(double)),
				 (int)(sizeof(sqrt(fc)) == 2 * sizeof(float)));

	printf("p5 %d %d %d\n",
				 (int)(sizeof(fabs(fc)) == sizeof(float)),
				 (int)(sizeof(fabs(dc)) == sizeof(double)),
				 (int)(sizeof(fabs((long double)1)) == sizeof(long double)));

	printf("p7 %d %d\n",
				 (int)(sizeof(creal(ldc)) == sizeof(long double)),
				 (int)(sizeof(cimag(fc)) == sizeof(float)));

	printf("p6 %d %d\n",
				 (int)(sizeof(nexttoward((float)1, 2.0L)) == sizeof(float)),
				 (int)(sizeof(ldexp((float)1, 3)) == sizeof(float)));
}
