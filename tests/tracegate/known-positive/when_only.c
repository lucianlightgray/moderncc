int good_open(int x) { MCC_TRACE_WHEN(x, "enter\n");
	if (x) { MCC_TRACE_WHEN(x, "br\n"); return x; }
	return 0;
}

int missing_open(int x) {
	return x + 1;
}

int wrong_message(int x) { MCC_TRACE_WHEN(x, "wrong\n");
	return x + 2;
}
