extern int printf(const char *, ...);

#define MS __attribute__((__ms_struct__))
#define GS __attribute__((__gcc_struct__))

struct smp_ms0 {
	unsigned char m0 : 7;
	int m1 : 11;
	int m2 : 5;
	int : 0;
	char m4 : 8;
	unsigned short m5 : 4;
	unsigned char m6 : 3;
	int m7 : 23;
} MS;

struct smp_ms1 {
	char a;
	long : 0;
	char : 0;
	int : 0;
	char b;
} MS;

struct smp_ms2 {
	char a : 8;
	int : 0;
	char b;
	char c;
} MS;

struct smp_ms3 {
	char a : 8;
	char : 0;
	int : 0;
	char b;
	char c;
} MS;

struct smp_ms4 {
	unsigned short a : 3;
	unsigned short b : 9;
	unsigned int : 0;
	unsigned char c : 7;
} MS;

struct smp_ms5 {
	char foo : 4;
	short : 0;
	char bar;
} MS;

struct smp_gs0 {
	unsigned char m0 : 7;
	int m1 : 11;
	int m2 : 5;
	int : 0;
	char m4 : 8;
} GS;

struct smp_plain {
	char a;
	int b : 3;
	char c;
};

union smp_msu {
	int a;
} __attribute__((__ms_struct__, __packed__));

struct smp_mspack {
	char c;
	union smp_msu u;
};

int main(void)
{
	printf("msstruct %d %d %d %d %d %d %d %d %d\n", (int)sizeof(struct smp_ms0),
				 (int)sizeof(struct smp_ms1), (int)sizeof(struct smp_ms2),
				 (int)sizeof(struct smp_ms3), (int)sizeof(struct smp_ms4),
				 (int)sizeof(struct smp_ms5), (int)sizeof(struct smp_gs0),
				 (int)sizeof(struct smp_plain), (int)sizeof(struct smp_mspack));
	return 0;
}
