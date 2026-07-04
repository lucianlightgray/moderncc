extern int printf(const char *, ...);

%:define STRINGIZE(x) %:x
%:define PASTE(a, b) a%:%:b

int PASTE(di, graph)(void) <%
    return 7;
%>

int main(void) <%
    int a<:3:> = <%10, 20, 30%>;
    int sum = 0;
    for (int i = 0; i < 3; i++)
        sum += a<:i:>;

    int t = (sum > 0) ? 1 : 0;
    int m = sum % 7;
    const char *s = STRINGIZE(hi);
    int d = digraph();
    printf("%d %d %d\n", a<:1:>, t, m);
    return (a<:1:> == 20 && sum == 60 && t == 1 && m == 4 && s<:0:> == 'h' && d == 7) ? 0 : 1;
%>
