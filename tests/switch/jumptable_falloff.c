/* Known-positive for T-mac-30099: a NON-exhaustive switch (falls off the end
 * on some inputs, no default) must STILL be diagnosed "might return no value"
 * under the jump-table strategy — proving the fix did not disable the check. */
int f(int x) {
	switch (x) {
	case 0: return 1;
	case 1: return 2;
	case 2: return 3;
	}
}
