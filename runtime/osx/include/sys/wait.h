#ifndef _MCC_OSX_SYS_WAIT_H
#define _MCC_OSX_SYS_WAIT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MCC_PID_T_DEFINED
#define _MCC_PID_T_DEFINED
typedef int pid_t;
#endif

#define WNOHANG 0x00000001
#define WUNTRACED 0x00000002

#define _W_INT(i) (i)
#define _WSTATUS(x) (_W_INT(x) & 0177)
#define _WSTOPPED 0177

#define WEXITSTATUS(x) ((_W_INT(x) >> 8) & 0x000000ff)
#define WSTOPSIG(x) (_W_INT(x) >> 8)
#define WIFEXITED(x) (_WSTATUS(x) == 0)
#define WIFSIGNALED(x) (_WSTATUS(x) != _WSTOPPED && _WSTATUS(x) != 0)
#define WIFSTOPPED(x) (_WSTATUS(x) == _WSTOPPED && WSTOPSIG(x) != 0x13)
#define WTERMSIG(x) (_WSTATUS(x))

pid_t wait(int *);
pid_t waitpid(pid_t, int *, int);

#ifdef __cplusplus
}
#endif

#endif
