#include <stdint.h>
#include <stdio.h>

extern int32_t aux_table[8];
extern const char *aux_names[3];
extern int32_t *aux_ptr;
extern int32_t *aux_priv_ptr;
extern const char aux_chars[6];

static int32_t local_table[4] = {10, 20, 30, 40};
static const char *local_msg = "local-string";
static int32_t *local_ptr = &local_table[2];
static const char *const local_names[2] = {"one", "two"};

int main(void) {
	int32_t s = 0;
	int i;
	for (i = 0; i < 8; i++)
		s += aux_table[i];
	printf("sum=%d\n", s);
	printf("names=%s,%s,%s\n", aux_names[0], aux_names[1], aux_names[2]);
	printf("auxptr=%d\n", *aux_ptr);
	printf("auxpriv=%d\n", *aux_priv_ptr);
	printf("auxchars=%s\n", aux_chars);
	printf("localptr=%d\n", *local_ptr);
	printf("msg=%s\n", local_msg);
	printf("localnames=%s/%s\n", local_names[0], local_names[1]);
	printf("lit=%s\n", "inline-literal");
	printf("litchar=%d\n", "abcdef"[3]);
	return 0;
}
