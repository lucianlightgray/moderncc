static int \u00e9 = 40;
static int caf\ u00e9 = 2;

int main(void) {
	int ok = 1;
	é += 1;
	café += 3;
	if (é != 41)
		ok = 0;
	if (café != 5)
		ok = 0;

	int arr<:3:> =
			<
					% 10, 20, 30 %>;
	if (arr <
					: 0 : >
					+arr <
					: 2 : >
			!=
			40)
		ok = 0;

	if (sizeof(u8"x") != 2)
		ok = 0;

	return ok ? 0 : 1;
}
