static int classify(int x) {
	int r = 0;
	switch (x) {
	case 0:
		r += 1;
	case 1:
		r += 2;
		break;
	case 5 ... 7:
		r += 10;
		break;
	default:
		r = 100;
	}
	return r;
}

int main(void) {
	return classify(0) + classify(1) + classify(6) + classify(1);
}
