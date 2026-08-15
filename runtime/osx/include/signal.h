#ifndef _MCC_OSX_SIGNAL_H
#define _MCC_OSX_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int sig_atomic_t;
typedef unsigned int sigset_t;

#ifndef _MCC_PID_T_DEFINED
#define _MCC_PID_T_DEFINED
typedef int pid_t;
#endif

struct __siginfo;

union __sigaction_u {
	void (*__sa_handler)(int);
	void (*__sa_sigaction)(int, struct __siginfo *, void *);
};

struct sigaction {
	union __sigaction_u __sigaction_u;
	sigset_t sa_mask;
	int sa_flags;
};

#define sa_handler __sigaction_u.__sa_handler
#define sa_sigaction __sigaction_u.__sa_sigaction

typedef void (*sig_t)(int);

#define SIG_DFL (void (*)(int))0
#define SIG_IGN (void (*)(int))1
#define SIG_ERR ((void (*)(int))-1)

#define SIG_BLOCK 1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGEMT 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGBUS 10
#define SIGSEGV 11
#define SIGSYS 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGURG 16
#define SIGSTOP 17
#define SIGTSTP 18
#define SIGCONT 19
#define SIGCHLD 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGIO 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGINFO 29
#define SIGUSR1 30
#define SIGUSR2 31

void (*signal(int, void (*)(int)))(int);
int raise(int);
int kill(pid_t, int);
int sigaction(int, const struct sigaction *, struct sigaction *);
int sigemptyset(sigset_t *);
int sigfillset(sigset_t *);
int sigaddset(sigset_t *, int);
int sigdelset(sigset_t *, int);
int sigismember(const sigset_t *, int);
int sigprocmask(int, const sigset_t *, sigset_t *);
int sigsuspend(const sigset_t *);

#ifdef __cplusplus
}
#endif

#endif
