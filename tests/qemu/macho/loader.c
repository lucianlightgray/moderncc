#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <ucontext.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <sys/prctl.h>
#include <stddef.h>

#define LC_SEGMENT_64 0x19
#define LC_MAIN 0x80000028u
#define MACOS_CLASS 0x2000000u

struct mh {
    uint32_t magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved;
};
struct lc {
    uint32_t cmd, cmdsize;
};
struct seg {
    uint32_t cmd, cmdsize;
    char segname[16];
    uint64_t vmaddr, vmsize, fileoff, filesize;
    uint32_t maxprot, initprot, nsects, flags;
};
struct main_c {
    uint32_t cmd, cmdsize;
    uint64_t entryoff, stacksize;
};

static void sigsys(int s, siginfo_t *si, void *uc_) {
    (void)s;
    (void)si;
    ucontext_t *uc = uc_;
    greg_t *r = uc->uc_mcontext.gregs;
    unsigned long nr = (unsigned long)r[REG_RAX] & 0xffffff;
    switch (nr) {
    case 1:
        _exit((int)r[REG_RDI]);
    case 4:
        r[REG_RAX] = write((int)r[REG_RDI], (void *)r[REG_RSI], (size_t)r[REG_RDX]);
        break;
    default:
        _exit(120);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s mach-o\n", argv[0]);
        return 2;
    }
    int fd = open(argv[1], O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st)) {
        perror("open");
        return 2;
    }
    uint8_t *buf = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
        perror("mmap file");
        return 2;
    }

    struct mh *h = (void *)buf;
    if (h->magic != 0xfeedfacf) {
        fprintf(stderr, "not a 64-bit Mach-O\n");
        return 2;
    }

    uint64_t entry = 0, text_vmaddr = 0;
    int have_text = 0;
    uint8_t *p = buf + sizeof(struct mh);
    for (uint32_t i = 0; i < h->ncmds; i++) {
        struct lc *c = (void *)p;
        if (c->cmd == LC_SEGMENT_64) {
            struct seg *sg = (void *)p;
            if (strcmp(sg->segname, "__PAGEZERO") && strcmp(sg->segname, "__LINKEDIT") && sg->vmsize) {
                void *m = mmap((void *)sg->vmaddr, sg->vmsize,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                if (m == MAP_FAILED) {
                    perror("mmap seg");
                    return 2;
                }
                if (sg->filesize)
                    memcpy((void *)sg->vmaddr, buf + sg->fileoff, sg->filesize);
                if (!strcmp(sg->segname, "__TEXT")) {
                    text_vmaddr = sg->vmaddr;
                    have_text = 1;
                }
            }
        } else if (c->cmd == LC_MAIN) {
            entry = ((struct main_c *)p)->entryoff;
        }
        p += c->cmdsize;
    }
    if (!have_text || !entry) {
        fprintf(stderr, "no __TEXT / LC_MAIN\n");
        return 2;
    }
    void (*go)(void) = (void (*)(void))(text_vmaddr + entry);

    struct sigaction sa = {0};
    sa.sa_sigaction = sigsys;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, 0);
    struct sock_filter filt[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, MACOS_CLASS, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog prog = {sizeof(filt) / sizeof(filt[0]), filt};
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0)) {
        perror("seccomp");
        return 2;
    }

    go();
    return 121;
}
