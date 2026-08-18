#pragma region MySection
int f(void){ return 1; }
#pragma endregion MySection
int main(void){ return f() ? 0 : 1; }
