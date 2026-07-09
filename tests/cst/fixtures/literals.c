const char *greeting = "hello,\tworld\n\"quoted\"\\backslash";
const char *concat = "adjacent"
										 " string"
										 " literals";
char newline = '\n';
char nul = '\0';

int hexnum = 0xDEADbeef;
long bignum = 1234567890123L;
double pi = 3.14159e0;
float tiny = .5f;
unsigned u = 0777u;

int lengths(void) {
	return (int)(sizeof "hello" + sizeof(greeting));
}
