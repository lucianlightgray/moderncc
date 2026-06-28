















#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>










char    *documentation[] = {
   "grep searches a file for a given pattern.  Execute by",
   "   grep [flags] regular_expression file_list\n",
   "Flags are single characters preceded by '-':",
   "   -c      Only a count of matching lines is printed",
   "   -f      Print file name for matching lines switch, see below",
   "   -n      Each line is preceded by its line number",
   "   -v      Only print non-matching lines\n",
   "The file_list is a list of files (wildcards are acceptable on RSX modes).",
   "\nThe file name is normally printed if there is a file given.",
   "The -f flag reverses this action (print name no file, not if more).\n",
   0 };

char    *patdoc[] = {
   "The regular_expression defines the pattern to search for.  Upper- and",
   "lower-case are always ignored.  Blank lines never match.  The expression",
   "should be quoted to prevent file-name translation.",
   "x      An ordinary character (not mentioned below) matches that character.",
   "'\\'    The backslash quotes any character.  \"\\$\" matches a dollar-sign.",
   "'^'    A circumflex at the beginning of an expression matches the",
   "       beginning of a line.",
   "'$'    A dollar-sign at the end of an expression matches the end of a line.",
   "'.'    A period matches any character except \"new-line\".",
   "':a'   A colon matches a class of characters described by the following",
   "':d'     character.  \":a\" matches any alphabetic, \":d\" matches digits,",
   "':n'     \":n\" matches alphanumerics, \": \" matches spaces, tabs, and",
   "': '     other control characters, such as new-line.",
   "'*'    An expression followed by an asterisk matches zero or more",
   "       occurrences of that expression: \"fo*\" matches \"f\", \"fo\"",
   "       \"foo\", etc.",
   "'+'    An expression followed by a plus sign matches one or more",
   "       occurrences of that expression: \"fo+\" matches \"fo\", etc.",
   "'-'    An expression followed by a minus sign optionally matches",
   "       the expression.",
   "'[]'   A string enclosed in square brackets matches any character in",
   "       that string, but no others.  If the first character in the",
   "       string is a circumflex, the expression matches any character",
   "       except \"new-line\" and the characters in the string.  For",
   "       example, \"[xyz]\" matches \"xx\" and \"zyx\", while \"[^xyz]\"",
   "       matches \"abc\" but not \"axb\".  A range of characters may be",
   "       specified by two characters separated by \"-\".  Note that,",
   "       [a-z] matches alphabetics, while [z-a] never matches.",
   "The concatenation of regular expressions is a regular expression.",
   0};

#define LMAX    512
#define PMAX    256

#define CHAR    1
#define BOL     2
#define EOL     3
#define ANY     4
#define CLASS   5
#define NCLASS  6
#define STAR    7
#define PLUS    8
#define MINUS   9
#define ALPHA   10
#define DIGIT   11
#define NALPHA  12
#define PUNCT   13
#define RANGE   14
#define ENDPAT  15

int cflag=0, fflag=0, nflag=0, vflag=0, nfile=0, debug=0;

char *pp, lbuf[LMAX], pbuf[PMAX];

char *cclass();
char *pmatch();
void store(int);
void error(char *);
void badpat(char *, char *, char *);
int match(void);



void file(char *s)
{
   printf("File %s:\n", s);
}


void cant(char *s)
{
   fprintf(stderr, "%s: cannot open\n", s);
}


void help(char **hp)
{
   char   **dp;

   for (dp = hp; *dp; ++dp)
      printf("%s\n", *dp);
}


void usage(char *s)
{
   fprintf(stderr, "?GREP-E-%s\n", s);
   fprintf(stderr,
         "Usage: grep [-cfnv] pattern [file ...].  grep ? for help\n");
   exit(1);
}


void compile(char *source)
{
   char  *s;
   char  *lp;
   int   c;
   int            o;
   char           *spp;

   s = source;
   if (debug)
      printf("Pattern = \"%s\"\n", s);
   pp = pbuf;
   while (c = *s++) {



      if (c == '*' || c == '+' || c == '-') {
         if (pp == pbuf ||
               (o=pp[-1]) == BOL ||
               o == EOL ||
               o == STAR ||
               o == PLUS ||
               o == MINUS)
            badpat("Illegal occurrence op.", source, s);
         store(ENDPAT);
         store(ENDPAT);
         spp = pp;
         while (--pp > lp)
            *pp = pp[-1];
         *pp =   (c == '*') ? STAR :
            (c == '-') ? MINUS : PLUS;
         pp = spp;
         continue;
      }



      lp = pp;
      switch(c) {

         case '^':
            store(BOL);
            break;

         case '$':
            store(EOL);
            break;

         case '.':
            store(ANY);
            break;

         case '[':
            s = cclass(source, s);
            break;

         case ':':
            if (*s) {
               switch(tolower(c = *s++)) {

                  case 'a':
                  case 'A':
                     store(ALPHA);
                     break;

                  case 'd':
                  case 'D':
                     store(DIGIT);
                     break;

                  case 'n':
                  case 'N':
                     store(NALPHA);
                     break;

                  case ' ':
                     store(PUNCT);
                     break;

                  default:
                     badpat("Unknown : type", source, s);

               }
               break;
            }
            else    badpat("No : type", source, s);

         case '\\':
            if (*s)
               c = *s++;

         default:
            store(CHAR);
            store(tolower(c));
      }
   }
   store(ENDPAT);
   store(0);
   if (debug) {
      for (lp = pbuf; lp < pp;) {
         if ((c = (*lp++ & 0377)) < ' ')
            printf("\\%o ", c);
         else    printf("%c ", c);
      }
      printf("\n");
   }
}


char *cclass(char *source, char *src)


{
   char   *s;
   char   *cp;
   int    c;
   int             o;

   s = src;
   o = CLASS;
   if (*s == '^') {
      ++s;
      o = NCLASS;
   }
   store(o);
   cp = pp;
   store(0);
   while ((c = *s++) && c!=']') {
      if (c == '\\') {
         if ((c = *s++) == '\0')
            badpat("Class terminates badly", source, s);
         else    store(tolower(c));
      }
      else if (c == '-' &&
            (pp - cp) > 1 && *s != ']' && *s != '\0') {
         c = pp[-1];
         pp[-1] = RANGE;
         store(c);
         c = *s++;
         store(tolower(c));
      }
      else {
         store(tolower(c));
      }
   }
   if (c != ']')
      badpat("Unterminated class", source, s);
   if ((c = (pp - cp)) >= 256)
      badpat("Class too large", source, s);
   if (c == 0)
      badpat("Empty class", source, s);
   *cp = c;
   return(s);
}


void store(int op)
{
   if (pp >= &pbuf[PMAX])
      error("Pattern too complex\n");
   *pp++ = op;
}


void badpat(char *message, char *source, char *stop)



{
   fprintf(stderr, "-GREP-E-%s, pattern is\"%s\"\n", message, source);
   fprintf(stderr, "-GREP-E-Stopped at byte %ld, '%c'\n",
         stop-source, stop[-1]);
   error("?GREP-E-Bad pattern\n");
}


void grep(FILE *fp, char *fn)


{
   int lno, count, m;

   lno = 0;
   count = 0;
   while (fgets(lbuf, LMAX, fp)) {
      ++lno;
      m = match();
      if ((m && !vflag) || (!m && vflag)) {
         ++count;
         if (!cflag) {
            if (fflag && fn) {
               file(fn);
               fn = 0;
            }
            if (nflag)
               printf("%d\t", lno);
            printf("%s\n", lbuf);
         }
      }
   }
   if (cflag) {
      if (fflag && fn)
         file(fn);
      printf("%d\n", count);
   }
}


int match()
{
   char   *l;

   for (l = lbuf; *l; ++l) {
      if (pmatch(l, pbuf))
         return(1);
   }
   return(0);
}


char *pmatch(char *line, char *pattern)


{
   char   *l;
   char   *p;
   char   c;
   char            *e;
   int             op;
   int             n;
   char            *are;

   l = line;
   if (debug > 1)
      printf("pmatch(\"%s\")\n", line);
   p = pattern;
   while ((op = *p++) != ENDPAT) {
      if (debug > 1)
         printf("byte[%ld] = 0%o, '%c', op = 0%o\n",
               l-line, *l, *l, op);
      switch(op) {

         case CHAR:
            if (tolower(*l++) != *p++)
               return(0);
            break;

         case BOL:
            if (l != lbuf)
               return(0);
            break;

         case EOL:
            if (*l != '\0')
               return(0);
            break;

         case ANY:
            if (*l++ == '\0')
               return(0);
            break;

         case DIGIT:
            if ((c = *l++) < '0' || (c > '9'))
               return(0);
            break;

         case ALPHA:
            c = tolower(*l++);
            if (c < 'a' || c > 'z')
               return(0);
            break;

         case NALPHA:
            c = tolower(*l++);
            if (c >= 'a' && c <= 'z')
               break;
            else if (c < '0' || c > '9')
               return(0);
            break;

         case PUNCT:
            c = *l++;
            if (c == 0 || c > ' ')
               return(0);
            break;

         case CLASS:
         case NCLASS:
            c = tolower(*l++);
            n = *p++ & 0377;
            do {
               if (*p == RANGE) {
                  p += 3;
                  n -= 2;
                  if (c >= p[-2] && c <= p[-1])
                     break;
               }
               else if (c == *p++)
                  break;
            } while (--n > 1);
            if ((op == CLASS) == (n <= 1))
               return(0);
            if (op == CLASS)
               p += n - 2;
            break;

         case MINUS:
            e = pmatch(l, p);
            while (*p++ != ENDPAT);
            if (e)
               l = e;
            break;

         case PLUS:
            if ((l = pmatch(l, p)) == 0)
               return(0);
         case STAR:
            are = l;
            while (*l && (e = pmatch(l, p)))
               l = e;
            while (*p++ != ENDPAT);
            while (l > are) {
               if (e = pmatch(l, p))
                  return(e);
               --l;
            }
            if (e = pmatch(l, p))
               return(e);
            return(0);

         default:
            printf("Bad op code %d\n", op);
            error("Cannot happen -- match\n");
      }
   }
   return(l);
}


void error(char *s)
{
   fprintf(stderr, "%s", s);
   exit(1);
}


int main(int argc, char **argv)
{
   char   *p;
   int    c, i;
   int             gotpattern;

   FILE            *f;

   if (argc <= 1)
      usage("No arguments");
   if (argc == 2 && argv[1][0] == '?' && argv[1][1] == 0) {
      help(documentation);
      help(patdoc);
      return 0;
   }
   nfile = argc-1;
   gotpattern = 0;
   for (i=1; i < argc; ++i) {
      p = argv[i];
      if (*p == '-') {
         ++p;
         while (c = *p++) {
            switch(tolower(c)) {

               case '?':
                  help(documentation);
                  break;

               case 'C':
               case 'c':
                  ++cflag;
                  break;

               case 'D':
               case 'd':
                  ++debug;
                  break;

               case 'F':
               case 'f':
                  ++fflag;
                  break;

               case 'n':
               case 'N':
                  ++nflag;
                  break;

               case 'v':
               case 'V':
                  ++vflag;
                  break;

               default:
                  usage("Unknown flag");
            }
         }
         argv[i] = 0;
         --nfile;
      } else if (!gotpattern) {
         compile(p);
         argv[i] = 0;
         ++gotpattern;
         --nfile;
      }
   }
   if (!gotpattern)
      usage("No pattern");
   if (nfile == 0)
      grep(stdin, 0);
   else {
      fflag = fflag ^ (nfile > 0);
      for (i=1; i < argc; ++i) {
         if (p = argv[i]) {
            if ((f=fopen(p, "r")) == NULL)
               cant(p);
            else {
               grep(f, p);
               fclose(f);
            }
         }
      }
   }
   return 0;
}

/* vim: set expandtab ts=4 sw=3 sts=3 tw=80 :*/
