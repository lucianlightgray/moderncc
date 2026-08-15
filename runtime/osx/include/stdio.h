#ifndef _MCC_OSX_STDIO_H
#define _MCC_OSX_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __sFILE FILE;

extern FILE *__stdinp;
extern FILE *__stdoutp;
extern FILE *__stderrp;

#define stdin __stdinp
#define stdout __stdoutp
#define stderr __stderrp

#define EOF (-1)
#define BUFSIZ 1024
#define FILENAME_MAX 1024
#define FOPEN_MAX 20
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef NULL
#define NULL ((void *)0)
#endif

int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vprintf(const char *, va_list);
int vfprintf(FILE *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);

int puts(const char *);
int fputs(const char *, FILE *);
int putchar(int);
int fputc(int, FILE *);
int putc(int, FILE *);

int fgetc(FILE *);
int getc(FILE *);
int getchar(void);
char *fgets(char *, int, FILE *);

FILE *fopen(const char *, const char *);
FILE *freopen(const char *, const char *, FILE *);
int fclose(FILE *);
int fflush(FILE *);

size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);

int fseek(FILE *, long, int);
long ftell(FILE *);
void rewind(FILE *);

int feof(FILE *);
int ferror(FILE *);
void clearerr(FILE *);
int setvbuf(FILE *, char *, int, size_t);

int remove(const char *);
int rename(const char *, const char *);
void perror(const char *);

#ifdef __cplusplus
}
#endif

#endif
