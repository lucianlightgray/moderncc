/* D1b — declaration structure via retroactive range-wrap (docs/CST.md §D1b).
 * Must produce Declaration (dumped "Decl"), FunctionDef ("Func"), ParamList,
 * Enum, TypeName, Initializer ("Init") and Label. */
enum color { RED,
						 GREEN = 5,
						 BLUE };

int global_with_init = 42; /* Declaration + Initializer     */

int decl_kinds(int a, int b) {
	/* FunctionDef + ParamList       */
	int arr[3] = {1, 2, 3}; /* Declaration + Initializer     */
	int c = (int)a;					/* TypeName (in the cast)        */
	enum color k = BLUE;		/* Enum use + Initializer        */
retry:										/* Label                         */
	if (c > 0) {
		c = c - 1;
		goto retry;
	}
	return arr[0] + b + k;
}
