#ifndef _MCC_OSX_UNISTD_H
#define _MCC_OSX_UNISTD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MCC_PID_T_DEFINED
#define _MCC_PID_T_DEFINED
typedef int pid_t;
#endif

typedef long ssize_t;
typedef long long off_t;

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

void _exit(int);
int close(int);
int dup(int);
int dup2(int, int);
pid_t fork(void);
int pipe(int *);
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);
off_t lseek(int, off_t, int);
int unlink(const char *);
pid_t getpid(void);
pid_t getppid(void);
unsigned int sleep(unsigned int);
unsigned int alarm(unsigned int);
int isatty(int);
int execv(const char *, char *const *);
int execvp(const char *, char *const *);
int execve(const char *, char *const *, char *const *);
char *getcwd(char *, size_t);
int chdir(const char *);

#ifdef __cplusplus
}
#endif

#endif
