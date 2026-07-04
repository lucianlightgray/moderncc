void s7_1_ctype(void) {
	int cnt_alpha = 0, cnt_digit = 0, cnt_upper = 0, cnt_lower = 0, cnt_alnum = 0,
		cnt_space = 0, cnt_blank = 0, cnt_xdig = 0, cnt_punct = 0, cnt_cntrl = 0,
		cnt_graph = 0, cnt_print = 0;
	for (int c = 0; c < 128; c++) {
		if (isalpha(c))
			cnt_alpha++;
		if (isdigit(c))
			cnt_digit++;
		if (isupper(c))
			cnt_upper++;
		if (islower(c))
			cnt_lower++;
		if (isalnum(c))
			cnt_alnum++;
		if (isspace(c))
			cnt_space++;
		if (isblank(c))
			cnt_blank++;
		if (isxdigit(c))
			cnt_xdig++;
		if (ispunct(c))
			cnt_punct++;
		if (iscntrl(c))
			cnt_cntrl++;
		if (isgraph(c))
			cnt_graph++;
		if (isprint(c))
			cnt_print++;
	}
	printf("ctype counts %d %d %d %d %d %d %d %d %d %d %d %d\n",
		   cnt_alpha, cnt_digit, cnt_upper, cnt_lower, cnt_alnum, cnt_space,
		   cnt_blank, cnt_xdig, cnt_punct, cnt_cntrl, cnt_graph, cnt_print);

	int consistent = 1;
	for (int c = 0; c < 128; c++)
		if ((isalnum(c) != 0) != (isalpha(c) != 0 || isdigit(c) != 0))
			consistent = 0;
	printf("alnum=alpha|digit %d\n", consistent);

	int gp = 1;
	for (int c = 0; c < 128; c++)
		if ((isgraph(c) != 0) != (isprint(c) != 0 && c != ' '))
			gp = 0;
	printf("graph=print&!sp %d\n", gp);

	printf("blank tab=%d sp=%d a=%d\n",
		   isblank('\t') != 0, isblank(' ') != 0, isblank('a') != 0);

	printf("EOF cls %d %d %d\n", isalpha(EOF) != 0, isspace(EOF) != 0, isdigit(EOF) != 0);

	int rt = 1, ident = 1;
	for (int c = 'A'; c <= 'Z'; c++)
		if (toupper(tolower(c)) != c)
			rt = 0;
	for (int c = 'a'; c <= 'z'; c++)
		if (tolower(toupper(c)) != c)
			rt = 0;
	for (int c = '0'; c <= '9'; c++)
		if (toupper(c) != c || tolower(c) != c)
			ident = 0;
	printf("case rt=%d ident=%d A->%c a->%c\n", rt, ident, tolower('A'), toupper('a'));

	printf("paren %d %d\n", (isalpha)('a') != 0, (isdigit)('5') != 0);

	int (*q)(int) = isalpha;
	printf("fnptr %d %d\n", q('z') != 0, q('7') != 0);
}

void s7_1_complex(void) {
	double complex z = 3.0 + 4.0 * I;

	printf("re=%g im=%g\n", creal(z), cimag(z));

	double complex z2 = creal(z) + cimag(z) * I;
	printf("recompose %d\n", creal(z2) == creal(z) && cimag(z2) == cimag(z));

	double complex zc = creal(z) - cimag(z) * I;
	printf("conj re=%g im=%g\n", creal(zc), cimag(zc));

	double complex w = CMPLX(1.0, 2.0);
	printf("cmplx re=%g im=%g\n", creal(w), cimag(w));

	double complex nz = CMPLX(1.0, -0.0);
	printf("cmplx negzero signbit=%d\n", (int)(1.0 / cimag(nz) < 0));

	double complex ii = I * I;
	printf("I*I re=%g im=%g\n", creal(ii), cimag(ii));

	complex double sum = z + w;
	complex double prod = z * w;
	printf("sum re=%g im=%g\n", creal(sum), cimag(sum));
	printf("prod re=%g im=%g\n", creal(prod), cimag(prod));
}

void s7_1_errno(void) {

	printf("errno macros pos=%d distinct=%d\n",
		   (EDOM > 0 && ERANGE > 0 && EILSEQ > 0),
		   (EDOM != ERANGE && ERANGE != EILSEQ && EDOM != EILSEQ));

#if EDOM > 0 && ERANGE > 0 && EILSEQ > 0
	printf("errno #if ok\n");
#endif

	errno = 0;
	printf("errno0=%d\n", errno);
	errno = ERANGE;
	printf("errno set match=%d\n", errno == ERANGE);
	int *ep = &errno;
	*ep = EDOM;
	printf("errno via ptr match=%d\n", errno == EDOM);
	errno = 0;
	printf("errno reset=%d\n", errno);
}
