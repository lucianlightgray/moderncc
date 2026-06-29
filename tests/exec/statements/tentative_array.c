/* 6.9.2: file-scope tentative definitions with incomplete array type are
   completed (as one element at end of TU, or by a later definition). */
extern int printf(const char *, ...);

int a1[];                        /* completed to one element at end of TU */
int a2[]; int a2[3] = {7, 8, 9}; /* completed by a later complete definition */
int a3[]; int a3[4];             /* completed by a later tentative definition */

int main(void)
{
    a1[0] = 5;
    int ok = a1[0] == 5
          && sizeof a2 == 12 && a2[2] == 9
          && sizeof a3 == 16;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
