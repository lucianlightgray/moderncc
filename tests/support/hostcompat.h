#ifndef MCC_TEST_HOSTCOMPAT_H
#define MCC_TEST_HOSTCOMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define HC_UNUSED __attribute__((unused))
#else
#define HC_UNUSED
#endif

#ifdef _WIN32
#include <direct.h>
#define HC_MKDIR(p) _mkdir(p)
#define HC_RMDIR(p) _rmdir(p)
#define HC_EXE_SFX ".exe"
#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif
#else
#include <sys/stat.h>
#include <unistd.h>
#define HC_MKDIR(p) mkdir((p), 0777)
#define HC_RMDIR(p) rmdir(p)
#define HC_EXE_SFX ""
#endif

static const char *hc_work = ".";

HC_UNUSED static void hc_set_workdir(const char *w) {
    hc_work = w;
}

HC_UNUSED static const char *hc_envv(const char *k, const char *d) {
    const char *v = getenv(k);
    return (v && *v) ? v : d;
}

HC_UNUSED static void hc_set_c_locale(void) {
#ifdef _WIN32
    _putenv("LC_ALL=C");
#else
    setenv("LC_ALL", "C", 1);
#endif
}

#ifdef _WIN32

static void hc_script(const char *cmd, char *path, size_t pn) {
    FILE *sf;
    snprintf(path, pn, "%s/_hccmd.sh", hc_work);
    sf = fopen(path, "wb");
    if (sf) {
        fputs(cmd, sf);
        fputc('\n', sf);
        fclose(sf);
    }
}

HC_UNUSED static FILE *hc_popen_sh(const char *cmd) {
    char path[1024], line[1200];
    hc_script(cmd, path, sizeof path);
    snprintf(line, sizeof line, "\"\"%s\" \"%s\"\"",
             hc_envv("MCC_TEST_SH", "sh"), path);
    return popen(line, "r");
}

HC_UNUSED static int hc_system_sh(const char *cmd) {
    char path[1024], line[1200];
    hc_script(cmd, path, sizeof path);
    snprintf(line, sizeof line, "\"\"%s\" \"%s\"\"",
             hc_envv("MCC_TEST_SH", "sh"), path);
    return system(line);
}

HC_UNUSED static FILE *hc_popen_cmd(const char *cmd) {
    char *wrapped = malloc(strlen(cmd) + 3);
    FILE *f;
    sprintf(wrapped, "\"%s\"", cmd);
    f = popen(wrapped, "r");
    free(wrapped);
    return f;
}

#define HC_POPEN_SH(c) hc_popen_sh(c)
#define HC_SYSTEM_SH(c) hc_system_sh(c)
#define HC_POPEN_CMD(c) hc_popen_cmd(c)

#else

#define HC_POPEN_SH(c) popen((c), "r")
#define HC_SYSTEM_SH(c) system(c)
#define HC_POPEN_CMD(c) popen((c), "r")

#endif

#endif
