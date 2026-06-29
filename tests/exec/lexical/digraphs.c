/* 6.4.6: digraphs <: :> <% %> %: %:%: spell [ ] { } # ## */
extern int printf(const char *, ...);

%:define STRINGIZE(x) %: x          /* %: as directive introducer and as # */
%:define PASTE(a, b) a %:%: b       /* %:%: as ## */

int PASTE(di, graph)(void) <% return 7; %>   /* -> digraph() */

int main(void) <%
    int a<:3:> = <% 10, 20, 30 %>;
    int sum = 0;
    for (int i = 0; i < 3; i++) sum += a<:i:>;
    /* ensure ':' (ternary, label) and '%' (modulo) still work */
    int t = (sum > 0) ? 1 : 0;
    int m = sum % 7;
    const char *s = STRINGIZE(hi);      /* "hi" */
    int d = digraph();                   /* pasted name -> 7 */
    printf("%d %d %d\n", a<:1:>, t, m);
    return (a<:1:> == 20 && sum == 60 && t == 1 && m == 4
            && s<:0:> == 'h' && d == 7) ? 0 : 1;
%>
