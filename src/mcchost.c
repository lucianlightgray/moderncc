#ifdef MCC_CONFIG_TOOLHOST
#include "mcchost.h"
#else
#include "mcc.h"
#endif

#ifndef MCC_CONFIG_BACKTRACE_ONLY

#ifdef _WIN32
#include <process.h>
#include <sys/stat.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <unistd.h>
#endif

ST_FUNC int host_stderr_isatty(void) {
#ifdef _WIN32
	return 0;
#else
	return isatty(2);
#endif
}

ST_FUNC char *host_path_normalize(char *path) {
#ifdef _WIN32
	char *p;
	for (p = path; *p; ++p)
		if (*p == '\\')
			*p = '/';
#endif
	return path;
}

ST_FUNC char *host_path_canonical(const char *path) {
#ifdef _WIN32
	return _fullpath(NULL, path, 260);
#else
	return realpath(path, NULL);
#endif
}

ST_FUNC int host_path_hash_fold(int c) {
#ifdef _WIN32
	return toup(c);
#else
	return c;
#endif
}

ST_FUNC FILE *host_fopen(const char *path, const char *mode) {
	return fopen(path, mode);
}

ST_FUNC MAYBE_UNUSED int host_fclose(FILE *f) {
	return fclose(f);
}

ST_FUNC MAYBE_UNUSED void host_set_exec_bits(const char *file) {
#ifndef _WIN32
	chmod(file, 0777);
#else
	(void)file;
#endif
}

#if defined _WIN32
#if defined LIBMCC_AS_DLL && defined MCC_HOST_AUTO_MCCDIR_W32
static HMODULE mcc_module;
BOOL WINAPI DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved) {
	if (DLL_PROCESS_ATTACH == dwReason)
		mcc_module = hDll;
	return TRUE;
}
#else
#define mcc_module NULL
#endif
#elif defined __APPLE__
extern int _NSGetExecutablePath(char *buf, unsigned int *bufsize);
#endif

ST_FUNC int host_exe_path(char *buf, int size) {
	int n = -1;
#if defined _WIN32
	n = GetModuleFileNameA(mcc_module, buf, size);
	if (n <= 0 || n >= size)
		return -1;
	host_path_normalize(buf);
	return n;
#else
#if defined __linux__ || defined __CYGWIN__
	n = readlink("/proc/self/exe", buf, size - 1);
#elif defined __NetBSD__
	n = readlink("/proc/curproc/exe", buf, size - 1);
#elif defined __FreeBSD__ || defined __DragonFly__
	n = readlink("/proc/curproc/file", buf, size - 1);
#elif defined __APPLE__
	{
		unsigned int sz = size;
		if (_NSGetExecutablePath(buf, &sz) == 0)
			n = (int)strlen(buf);
	}
#endif
	if (n <= 0 || n >= size)
		return -1;
	buf[n] = 0;
	return n;
#endif
}

ST_FUNC FILE *host_temp_c_file(char *path, int size) {
#ifdef _WIN32
	char dir[MAX_PATH];
	static unsigned serial;
	if (!GetTempPathA(sizeof dir, dir))
		return NULL;
	snprintf(path, size, "%smcc-me-%u-%u.c", dir,
					 (unsigned)GetCurrentProcessId(), ++serial);
	return fopen(path, "wb");
#else
	int fd;
	snprintf(path, size, "/tmp/.mccmeXXXXXX.c");
	fd = mkstemps(path, 2);
	if (fd < 0)
		return NULL;
	return fdopen(fd, "w");
#endif
}

#ifdef MCC_HOST_AUTO_MCCDIR_W32
ST_FUNC char *host_w32_mccdir(char *path) {
	char *p;
	if (host_exe_path(path, MAX_PATH) < 0)
		path[0] = 0;
	p = mcc_basename(strlwr(path));
	if (p > path)
		--p;
	*p = 0;
	return path;
}
#endif

ST_FUNC MAYBE_UNUSED int host_system_dir(char *buf, int size) {
#ifdef _WIN32
	GetSystemDirectoryA(buf, size);
	host_path_normalize(buf);
	return 0;
#else
	(void)buf;
	(void)size;
	return -1;
#endif
}

#ifdef _WIN32

static char *host_quote_w32(const char *s) {
	char *o, *r = mcc_malloc(2 * strlen(s) + 3);
	int cbs = 0, quoted = !*s;

	for (o = r; *s; *o++ = *s++) {
		quoted |= *s == ' ' || *s == '\t';
		if (*s == '\\' || *s == '"')
			*o++ = '\\';
		else
			o -= cbs;
		cbs = *s == '\\' ? cbs + 1 : 0;
	}
	if (quoted) {
		memmove(r + 1, r, o++ - r);
		*r = *o++ = '"';
	} else {
		o -= cbs;
	}

	*o = 0;
	return r;
}
#endif

ST_FUNC MAYBE_UNUSED int host_spawn_wait(const char *const *argv) {
#ifdef _WIN32
	int i, n, ret;
	char **qv;
	for (n = 0; argv[n]; ++n)
		;
	qv = mcc_malloc((n + 1) * sizeof *qv);
	for (i = 0; i < n; ++i)
		qv[i] = host_quote_w32(argv[i]);
	qv[n] = NULL;
	ret = _spawnvp(_P_WAIT, argv[0], (const char *const *)qv);
	for (i = 0; i < n; ++i)
		mcc_free(qv[i]);
	mcc_free(qv);
	return ret;
#else
	int pid = fork(), status;
	if (pid == 0) {
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
		return -1;
	return WEXITSTATUS(status);
#endif
}

ST_FUNC MAYBE_UNUSED int host_exec_replace(char **argv) {
#ifdef _WIN32
	int ret;
	char **p;
	for (p = argv; *p; ++p)
		*p = host_quote_w32(*p);
	ret = _spawnvp(_P_NOWAIT, argv[0], (const char *const *)argv);
	if (-1 == ret)
		return ret;
	_cwait(&ret, ret, _WAIT_CHILD);
	exit(ret);
#else
	return execvp(argv[0], argv);
#endif
}

ST_FUNC MAYBE_UNUSED int host_find_tool(const char *name, const char *ext, char *buf, int size) {
#ifdef _WIN32
	return SearchPath(NULL, name, ext, size, buf, NULL) ? 1 : 0;
#else
	const char *path, *p;
	char cand[4096];
	(void)ext;

	if (strchr(name, '/')) {
		if (access(name, X_OK) == 0 && (int)strlen(name) < size) {
			strcpy(buf, name);
			return 1;
		}
		return 0;
	}
	path = getenv("PATH");
	if (!path)
		path = "/usr/local/bin:/usr/bin:/bin";
	for (p = path;;) {
		const char *e = p;
		int dl;
		while (*e && *e != ':')
			++e;
		dl = (int)(e - p);
		if (dl == 0)
			snprintf(cand, sizeof cand, "%s", name);
		else
			snprintf(cand, sizeof cand, "%.*s/%s", dl, p, name);
		if (access(cand, X_OK) == 0) {
			if ((int)strlen(cand) >= size)
				return 0;
			strcpy(buf, cand);
			return 1;
		}
		if (!*e)
			break;
		p = e + 1;
	}
	return 0;
#endif
}

ST_FUNC MAYBE_UNUSED int host_find_tool_any(const char *const *names, const char *ext, char *buf, int size) {
	int i;
	for (i = 0; names[i]; ++i)
		if (host_find_tool(names[i], ext, buf, size))
			return 1;
	return 0;
}

#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef realloc
#undef free

#ifndef _WIN32

static char *host_slurp_fd(int fd) {
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap), *nb;
	ssize_t r;
	if (!buf)
		return NULL;
	for (;;) {
		if (len + 1 >= cap) {
			cap *= 2;
			if (!(nb = realloc(buf, cap))) {
				free(buf);
				return NULL;
			}
			buf = nb;
		}
		r = read(fd, buf + len, cap - len - 1);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			free(buf);
			return NULL;
		}
		if (r == 0)
			break;
		len += (size_t)r;
	}
	buf[len] = 0;
	return buf;
}
#endif

#pragma pop_macro("free")
#pragma pop_macro("realloc")
#pragma pop_macro("malloc")

#ifdef _WIN32

struct host_pipe_read {
	HANDLE h;
	char *buf;
	size_t len;
};
static DWORD WINAPI host_pipe_read_thread(LPVOID ud) {
	struct host_pipe_read *pr = ud;
	char tmp[4096];
	DWORD rd;
	pr->buf = mcc_malloc(1);
	pr->len = 0;
	while (ReadFile(pr->h, tmp, sizeof tmp, &rd, NULL) && rd) {
		char *nb = mcc_realloc(pr->buf, pr->len + rd + 1);
		pr->buf = nb;
		memcpy(pr->buf + pr->len, tmp, rd);
		pr->len += rd;
	}
	pr->buf[pr->len] = 0;
	return 0;
}
#endif

static const char **host_build_argv(const char *const *argv, const HostSpawnOpts *o) {
	int n = 0, l = 0, i, j = 0;
	const char **out;
	if (o && o->launcher)
		while (o->launcher[l])
			++l;
	while (argv[n])
		++n;
	out = mcc_malloc((l + n + 1) * sizeof *out);
	for (i = 0; i < l; ++i)
		out[j++] = o->launcher[i];
	for (i = 0; i < n; ++i)
		out[j++] = argv[i];
	out[j] = NULL;
	return out;
}

ST_FUNC MAYBE_UNUSED int host_spawn_ex(const char *const *argv, const HostSpawnOpts *o) {
	static const HostSpawnOpts nopts;
	const char **full;
	int ret = -1;
	if (!o)
		o = &nopts;
	full = host_build_argv(argv, o);
#ifdef _WIN32
	{
		char *cmd, *p;
		int i, len = 1;
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		SECURITY_ATTRIBUTES sa;
		HANDLE hout = INVALID_HANDLE_VALUE, herr = INVALID_HANDLE_VALUE;
		HANDLE rpo = NULL, wpo = NULL, rpe = NULL, wpe = NULL;
		char *envblk = NULL;

		for (i = 0; full[i]; ++i)
			len += 2 * (int)strlen(full[i]) + 3;
		cmd = mcc_malloc(len);
		for (p = cmd, i = 0; full[i]; ++i) {
			char *q = host_quote_w32(full[i]);
			if (i)
				*p++ = ' ';
			strcpy(p, q);
			p += strlen(q);
			mcc_free(q);
		}
		*p = 0;

		memset(&sa, 0, sizeof sa);
		sa.nLength = sizeof sa;
		sa.bInheritHandle = TRUE;
		memset(&si, 0, sizeof si);
		si.cb = sizeof si;
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

		if (o->stdout_buf) {
			CreatePipe(&rpo, &wpo, &sa, 0);
			SetHandleInformation(rpo, HANDLE_FLAG_INHERIT, 0);
			si.hStdOutput = wpo;
		} else if (o->stdout_file) {
			hout = CreateFileA(o->stdout_file, GENERIC_WRITE, FILE_SHARE_READ,
												 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			si.hStdOutput = hout;
		}
		if (o->stderr_buf) {
			CreatePipe(&rpe, &wpe, &sa, 0);
			SetHandleInformation(rpe, HANDLE_FLAG_INHERIT, 0);
			si.hStdError = wpe;
		} else if (o->stderr_file) {
			if (o->stdout_file && !strcmp(o->stderr_file, o->stdout_file) && hout != INVALID_HANDLE_VALUE)
				si.hStdError = hout;
			else {
				herr = CreateFileA(o->stderr_file, GENERIC_WRITE, FILE_SHARE_READ,
													 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				si.hStdError = herr;
			}
		}
		if (o->env) {
			int tot = 1, k;
			for (k = 0; o->env[k]; ++k)
				tot += (int)strlen(o->env[k]) + 1;
			envblk = mcc_malloc(tot);
			for (p = envblk, k = 0; o->env[k]; ++k) {
				strcpy(p, o->env[k]);
				p += strlen(o->env[k]) + 1;
			}
			*p = 0;
		}
		if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, envblk,
											 o->cwd, &si, &pi)) {
			DWORD ec = 0;
			struct host_pipe_read pre;
			HANDLE terr = NULL;
			if (wpo) {
				CloseHandle(wpo);
				wpo = NULL;
			}
			if (wpe) {
				CloseHandle(wpe);
				wpe = NULL;
			}
			if (o->stderr_buf) {
				pre.h = rpe;
				terr = CreateThread(NULL, 0, host_pipe_read_thread, &pre, 0, NULL);
			}
			if (o->stdout_buf) {
				struct host_pipe_read pro;
				pro.h = rpo;
				host_pipe_read_thread(&pro);
				*o->stdout_buf = pro.buf;
			}
			if (terr) {
				WaitForSingleObject(terr, INFINITE);
				CloseHandle(terr);
				*o->stderr_buf = pre.buf;
			} else if (o->stderr_buf) {
				pre.h = rpe;
				host_pipe_read_thread(&pre);
				*o->stderr_buf = pre.buf;
			}
			WaitForSingleObject(pi.hProcess, INFINITE);
			GetExitCodeProcess(pi.hProcess, &ec);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			ret = (int)ec;
		}
		if (hout != INVALID_HANDLE_VALUE)
			CloseHandle(hout);
		if (herr != INVALID_HANDLE_VALUE)
			CloseHandle(herr);
		if (rpo)
			CloseHandle(rpo);
		if (rpe)
			CloseHandle(rpe);
		if (wpo)
			CloseHandle(wpo);
		if (wpe)
			CloseHandle(wpe);
		mcc_free(cmd);
		mcc_free(envblk);
	}
#else
	{
		int po[2] = {-1, -1}, pe[2] = {-1, -1};
		int want_po = o->stdout_buf != NULL, want_pe = o->stderr_buf != NULL;
		pid_t pid;
		if ((want_po && pipe(po)) || (want_pe && pipe(pe))) {
			mcc_free(full);
			return -1;
		}
		pid = fork();
		if (pid == 0) {
			int fo, fe;
			if (o->cwd && chdir(o->cwd))
				_exit(127);
			if (want_po) {
				dup2(po[1], 1);
				close(po[0]);
				close(po[1]);
			} else if (o->stdout_file &&
								 (fo = open(o->stdout_file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) >= 0) {
				dup2(fo, 1);
				close(fo);
			}
			if (want_pe) {
				dup2(pe[1], 2);
				close(pe[0]);
				close(pe[1]);
			} else if (o->stderr_file) {
				if (o->stdout_file && !strcmp(o->stderr_file, o->stdout_file))
					dup2(1, 2);
				else if ((fe = open(o->stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) >= 0) {
					dup2(fe, 2);
					close(fe);
				}
			}
			if (o->env) {
				extern char **environ;
				environ = (char **)o->env;
			}
			execvp(full[0], (char *const *)full);
			_exit(127);
		}
		if (pid < 0) {
			if (want_po) {
				close(po[0]);
				close(po[1]);
			}
			if (want_pe) {
				close(pe[0]);
				close(pe[1]);
			}
			mcc_free(full);
			return -1;
		}
		if (want_po)
			close(po[1]);
		if (want_pe)
			close(pe[1]);

		if (want_po)
			*o->stdout_buf = host_slurp_fd(po[0]);
		if (want_po)
			close(po[0]);
		if (want_pe)
			*o->stderr_buf = host_slurp_fd(pe[0]);
		if (want_pe)
			close(pe[0]);
		{
			int status;
			if (waitpid(pid, &status, 0) == pid && WIFEXITED(status))
				ret = WEXITSTATUS(status);
		}
	}
#endif
	mcc_free((void *)full);
	return ret;
}

ST_FUNC MAYBE_UNUSED int host_mkdirs(const char *path) {
	char buf[4096];
	char *p;
	size_t n = strlen(path);
	if (n >= sizeof buf)
		return -1;
	memcpy(buf, path, n + 1);
	host_path_normalize(buf);
	for (p = buf + 1; *p; ++p) {
		if (*p != '/')
			continue;
		*p = 0;
#ifdef _WIN32
		_mkdir(buf);
#else
		mkdir(buf, 0777);
#endif
		*p = '/';
	}
#ifdef _WIN32
	if (_mkdir(buf) && errno != EEXIST)
		return -1;
#else
	if (mkdir(buf, 0777) && errno != EEXIST)
		return -1;
#endif
	return 0;
}

ST_FUNC MAYBE_UNUSED int host_cache_dir(char *buf, int size) {
	const char *base;
	char tmp[3072];
#if MCC_HOST_WIN32
	base = getenv("LOCALAPPDATA");
	if (base && base[0])
		snprintf(tmp, sizeof tmp, "%s/mcc", base);
	else {
		base = getenv("USERPROFILE");
		if (!base || !base[0])
			return -1;
		snprintf(tmp, sizeof tmp, "%s/AppData/Local/mcc", base);
	}
#elif MCC_HOST_DARWIN
	base = getenv("HOME");
	if (!base || !base[0])
		return -1;
	snprintf(tmp, sizeof tmp, "%s/Library/Caches/mcc", base);
#else
	base = getenv("XDG_CACHE_HOME");
	if (base && base[0])
		snprintf(tmp, sizeof tmp, "%s/mcc", base);
	else {
		base = getenv("HOME");
		if (!base || !base[0])
			return -1;
		snprintf(tmp, sizeof tmp, "%s/.cache/mcc", base);
	}
#endif
	if ((int)strlen(tmp) >= size)
		return -1;
	if (host_mkdirs(tmp) != 0)
		return -1;
	strcpy(buf, tmp);
	return 0;
}

ST_FUNC MAYBE_UNUSED int host_rmrf(const char *path) {
#if MCC_HOST_WIN32
	return remove(path);
#else
	DIR *d = opendir(path);
	if (d) {
		struct dirent *e;
		char child[4096];
		while ((e = readdir(d))) {
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			snprintf(child, sizeof child, "%s/%s", path, e->d_name);
			host_rmrf(child);
		}
		closedir(d);
		return rmdir(path);
	}
	return remove(path);
#endif
}

ST_FUNC MAYBE_UNUSED int host_copy_file(const char *src, const char *dst, int preserve_exec) {
	FILE *fi = fopen(src, "rb"), *fo;
	char buf[65536];
	size_t r;
	int ok = 1;
	if (!fi)
		return -1;
	if (!(fo = fopen(dst, "wb"))) {
		fclose(fi);
		return -1;
	}
	while ((r = fread(buf, 1, sizeof buf, fi)) > 0)
		if (fwrite(buf, 1, r, fo) != r) {
			ok = 0;
			break;
		}
	if (ferror(fi))
		ok = 0;
	fclose(fi);
	if (fclose(fo))
		ok = 0;
	if (!ok)
		return -1;
#ifndef _WIN32
	if (preserve_exec) {
		struct stat st;
		if (!stat(src, &st))
			chmod(dst, st.st_mode & 0777);
	}
#else
	(void)preserve_exec;
#endif
	return 0;
}

ST_FUNC MAYBE_UNUSED int host_stat(const char *path, int *is_dir, long long *size, long long *mtime) {
#ifdef _WIN32
	struct _stat64 st;
	if (_stat64(path, &st))
		return -1;
#else
	struct stat st;
	if (stat(path, &st))
		return -1;
#endif
	if (is_dir)
		*is_dir = (st.st_mode & S_IFMT) == S_IFDIR;
	if (size)
		*size = (long long)st.st_size;
	if (mtime)
		*mtime = (long long)st.st_mtime;
	return 0;
}

ST_FUNC MAYBE_UNUSED int host_dir_walk(const char *dir, int recursive, host_walk_fn fn, void *ud) {
#ifdef _WIN32
	char pat[4096], child[4096];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	int rc = 0, is_dir;
	snprintf(pat, sizeof pat, "%s/*", dir);
	if ((h = FindFirstFileA(pat, &fd)) == INVALID_HANDLE_VALUE)
		return -1;
	do {
		if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
			continue;
		snprintf(child, sizeof child, "%s/%s", dir, fd.cFileName);
		host_path_normalize(child);
		is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		if ((rc = fn(child, is_dir, ud)))
			break;
		if (recursive && is_dir && (rc = host_dir_walk(child, 1, fn, ud)))
			break;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return rc;
#else
	DIR *d = opendir(dir);
	struct dirent *e;
	char child[4096];
	int rc = 0, is_dir;
	if (!d)
		return -1;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		snprintf(child, sizeof child, "%s/%s", dir, e->d_name);
		if (host_stat(child, &is_dir, NULL, NULL))
			continue;
		if ((rc = fn(child, is_dir, ud)))
			break;
		if (recursive && is_dir && (rc = host_dir_walk(child, 1, fn, ud)))
			break;
	}
	closedir(d);
	return rc;
#endif
}

ST_FUNC MAYBE_UNUSED int host_codesign_adhoc(const char *file) {
#if MCC_CONFIG_CODESIGN
	const char *argv[] = {"codesign", "-f", "-s", "-", file, NULL};
	return host_spawn_wait(argv);
#else
	(void)file;
	return 0;
#endif
}

ST_FUNC MAYBE_UNUSED unsigned host_clock_ms(void) {
#ifdef _WIN32
	return GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + (tv.tv_usec + 500) / 1000;
#endif
}

ST_FUNC MAYBE_UNUSED int host_nproc(void) {
#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
#elif defined _SC_NPROCESSORS_ONLN
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return n > 0 ? (int)n : 1;
#else
	return 1;
#endif
}

#if defined _WIN32 && !defined PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif
ST_FUNC MAYBE_UNUSED void host_sys_info(char *sysname, int ssz, char *release, int rsz,
																				char *machine, int msz) {
#ifdef _WIN32
	if (sysname && ssz)
		snprintf(sysname, ssz, "WIN32");
	if (release && rsz) {
		OSVERSIONINFOA vi;
		memset(&vi, 0, sizeof vi);
		vi.dwOSVersionInfoSize = sizeof vi;
		if (GetVersionExA(&vi))
			snprintf(release, rsz, "%lu.%lu.%lu", (unsigned long)vi.dwMajorVersion,
							 (unsigned long)vi.dwMinorVersion, (unsigned long)vi.dwBuildNumber);
		else
			release[0] = 0;
	}
	if (machine && msz) {
		SYSTEM_INFO si;
		const char *m;
		GetNativeSystemInfo(&si);
		switch (si.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			m = "AMD64";
			break;
		case PROCESSOR_ARCHITECTURE_ARM64:
			m = "ARM64";
			break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			m = "x86";
			break;
		default:
			m = "?";
			break;
		}
		snprintf(machine, msz, "%s", m);
	}
#else
	struct utsname u;
	if (uname(&u)) {
		if (sysname && ssz)
			sysname[0] = 0;
		if (release && rsz)
			release[0] = 0;
		if (machine && msz)
			machine[0] = 0;
		return;
	}
	if (sysname && ssz)
		snprintf(sysname, ssz, "%s", u.sysname);
	if (release && rsz)
		snprintf(release, rsz, "%s", u.release);
	if (machine && msz)
		snprintf(machine, msz, "%s", u.machine);
#endif
}

ST_FUNC char **host_environ(void) {
#ifdef __APPLE__
	extern char ***_NSGetEnviron(void);
	return *_NSGetEnviron();
#elif defined(_WIN32)
	return environ;
#else
	extern char **environ;
	return environ;
#endif
}

#ifdef MCC_CONFIG_STATIC

typedef struct MCCSyms {
	char *str;
	void *ptr;
} MCCSyms;

static MCCSyms mcc_syms[] = {
#if !defined(MCC_CONFIG_MCCBOOT)
#define MCCSYM(a) {               \
											#a,         \
											(void *)&a, \
									},
		MCCSYM(stdin) MCCSYM(stdout) MCCSYM(stderr)
				MCCSYM(printf) MCCSYM(fprintf) MCCSYM(sprintf) MCCSYM(snprintf)
						MCCSYM(vprintf) MCCSYM(vfprintf) MCCSYM(vsnprintf)
								MCCSYM(puts) MCCSYM(fputs) MCCSYM(putchar) MCCSYM(fputc) MCCSYM(putc)
										MCCSYM(getchar) MCCSYM(fgetc) MCCSYM(getc) MCCSYM(fgets) MCCSYM(ungetc)
												MCCSYM(scanf) MCCSYM(fscanf) MCCSYM(sscanf)
														MCCSYM(fopen) MCCSYM(freopen) MCCSYM(fclose) MCCSYM(fflush)
																MCCSYM(fread) MCCSYM(fwrite) MCCSYM(fseek) MCCSYM(ftell) MCCSYM(rewind)
																		MCCSYM(feof) MCCSYM(ferror) MCCSYM(clearerr) MCCSYM(fileno)
																				MCCSYM(perror) MCCSYM(remove) MCCSYM(rename) MCCSYM(setvbuf) MCCSYM(setbuf)
																						MCCSYM(malloc) MCCSYM(calloc) MCCSYM(realloc) MCCSYM(free)
																								MCCSYM(exit) MCCSYM(_Exit) MCCSYM(abort) MCCSYM(atexit)
																										MCCSYM(atoi) MCCSYM(atol) MCCSYM(atoll) MCCSYM(atof)
																												MCCSYM(strtol) MCCSYM(strtoll) MCCSYM(strtoul) MCCSYM(strtoull)
																														MCCSYM(strtod) MCCSYM(strtof) MCCSYM(strtold)
																																MCCSYM(rand) MCCSYM(srand) MCCSYM(qsort) MCCSYM(bsearch)
																																		MCCSYM(abs) MCCSYM(labs) MCCSYM(llabs) MCCSYM(getenv) MCCSYM(system)
																																				MCCSYM(memcpy) MCCSYM(memmove) MCCSYM(memset) MCCSYM(memcmp) MCCSYM(memchr)
																																						MCCSYM(strlen) MCCSYM(strnlen) MCCSYM(strcmp) MCCSYM(strncmp)
																																								MCCSYM(strcpy) MCCSYM(strncpy) MCCSYM(strcat) MCCSYM(strncat)
																																										MCCSYM(strchr) MCCSYM(strrchr) MCCSYM(strstr) MCCSYM(strtok)
																																												MCCSYM(strspn) MCCSYM(strcspn) MCCSYM(strpbrk) MCCSYM(strerror)
																																														MCCSYM(sin) MCCSYM(cos) MCCSYM(tan) MCCSYM(asin) MCCSYM(acos) MCCSYM(atan)
																																																MCCSYM(atan2) MCCSYM(sinh) MCCSYM(cosh) MCCSYM(tanh)
																																																		MCCSYM(exp) MCCSYM(log) MCCSYM(log10) MCCSYM(log2) MCCSYM(pow)
																																																				MCCSYM(sqrt) MCCSYM(cbrt) MCCSYM(ceil) MCCSYM(floor) MCCSYM(round)
																																																						MCCSYM(trunc) MCCSYM(fabs) MCCSYM(fmod) MCCSYM(fmin) MCCSYM(fmax)
																																																								MCCSYM(hypot) MCCSYM(ldexp) MCCSYM(frexp) MCCSYM(modf)
#undef MCCSYM
#endif
																																																										{NULL, NULL},
};

static void *host_static_sym(const char *symbol) {
	MCCSyms *p = mcc_syms;
	while (p->str != NULL) {
		if (!strcmp(p->str, symbol))
			return p->ptr;
		p++;
	}
	return NULL;
}

ST_FUNC void *host_dlopen(const char *name) {
	(void)name;
	return NULL;
}

ST_FUNC void host_dlclose(void *h) {
	(void)h;
}

ST_FUNC MAYBE_UNUSED const char *host_dlerror(void) {
	return "error";
}

ST_FUNC void *host_dlsym(void *h, const char *symbol) {
	(void)h;
	return host_static_sym(symbol);
}

ST_FUNC void *host_dlsym_process(const char *symbol) {
	return host_static_sym(symbol);
}

#else

ST_FUNC void *host_dlopen(const char *name) {
#ifdef _WIN32
	return (void *)LoadLibraryA(name);
#else
	return dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
#endif
}

ST_FUNC void host_dlclose(void *h) {
#ifdef _WIN32
	FreeLibrary((HMODULE)h);
#else
	dlclose(h);
#endif
}

ST_FUNC MAYBE_UNUSED const char *host_dlerror(void) {
#ifdef _WIN32
	return "error";
#else
	return dlerror();
#endif
}

ST_FUNC void *host_dlsym(void *h, const char *symbol) {
#ifdef _WIN32
	return (void *)GetProcAddress((HMODULE)h, symbol);
#else
	return dlsym(h, symbol);
#endif
}

ST_FUNC void *host_dlsym_process(const char *symbol) {
#ifdef _WIN32
	(void)symbol;
	return NULL;
#else
	return dlsym(RTLD_DEFAULT, symbol);
#endif
}

#endif

ST_FUNC MAYBE_UNUSED const char *host_macos_sdk_root(void) {
#ifdef __APPLE__
	static char buf[1024];
	static int done;
	char *sdkroot = NULL, *pos = NULL;
	void *xcs;
	int (*f)(unsigned int, char **);

	if (!done) {
		done = 1;
		xcs = host_dlopen("libxcselect.dylib");
		f = xcs ? host_dlsym(xcs, "xcselect_host_sdk_path") : NULL;
		if (f)
			f(1, &sdkroot);
		if (sdkroot)
			pos = strstr(sdkroot, "SDKs/MacOSX");
		if (pos)
			snprintf(buf, sizeof buf, "%.*s.sdk", (int)(pos - sdkroot + 11), sdkroot);
#pragma push_macro("free")
#undef free
		free(sdkroot);
#pragma pop_macro("free")
	}
	return buf[0] ? buf : NULL;
#else
	return NULL;
#endif
}

ST_FUNC MAYBE_UNUSED const char *host_elf_interp_override(void) {
	return getenv("LD_SO");
}

#ifdef MCC_TARGET_IS_HOST

#ifndef _WIN32
#include <sys/mman.h>
#endif

ST_FUNC size_t host_pagesize(void) {
#if defined _WIN32
	return 4096;
#elif defined _SC_PAGESIZE
	return sysconf(_SC_PAGESIZE);
#elif defined __APPLE__
	return getpagesize();
#else
	return 4096;
#endif
}

ST_FUNC int host_runmem_dual(void) {
#ifdef _WIN32
	return 0;
#else
	static int dual;
	if (!dual) {
#if MCC_CONFIG_RUN_DUALMAP
		dual = 1;
#else
		size_t page = host_pagesize();
		void *p = mmap(NULL, page, PROT_READ | PROT_WRITE,
									 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		dual = -1;
		if (p != MAP_FAILED) {
			if (host_runmem_protect(p, page,
															HOST_RUNMEM_RO ? HOST_PROT_RX : HOST_PROT_RWX) < 0
					&& (errno == EACCES || errno == EPERM))
				dual = 1;
			munmap(p, page);
		}
#endif
	}
	return dual > 0;
#endif
}

ST_FUNC void *host_runmem_alloc(unsigned *psize, int *ptr_diff) {
	unsigned size = *psize;
	void *ptr;
	*ptr_diff = 0;
#ifdef _WIN32
	ptr = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
	if (host_runmem_dual()) {
		void *prw;
		char tmpfname[] = "/tmp/.mccrunXXXXXX";
		int fd = mkstemp(tmpfname);
		unlink(tmpfname);
		ftruncate(fd, size);

		ptr = mmap(NULL, size * 2, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
		prw = mmap((char *)ptr + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
		close(fd);
		if (ptr == MAP_FAILED || prw == MAP_FAILED)
			return NULL;
		*ptr_diff = (char *)prw - (char *)ptr;
		size *= 2;
	} else {
		ptr = mcc_malloc(size += host_pagesize());
	}
#endif
	*psize = size;
	return ptr;
}

ST_FUNC void host_runmem_free(void *ptr, unsigned size) {
#ifdef _WIN32
	(void)size;
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	if (host_runmem_dual()) {
		munmap(ptr, size);
	} else {
		size_t page = host_pagesize();
		host_runmem_protect((void *)((size_t)ptr + (-(size_t)ptr & (page - 1))),
												size - page, HOST_PROT_RW);
		mcc_free(ptr);
	}
#endif
}

ST_FUNC void host_icache_flush(void *ptr, unsigned long length) {
#if !defined _WIN32 && \
		((defined MCC_TARGET_ARM && !MCC_TARGETOS_BSD) || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64)
	void __clear_cache(void *beginning, void *end);
	__clear_cache(ptr, (char *)ptr + length);
#else
	(void)ptr;
	(void)length;
#endif
}

ST_FUNC int host_runmem_protect(void *ptr, unsigned long length, int mode) {
#ifdef _WIN32
	static const unsigned char protect[] = {
			PAGE_EXECUTE_READ,
			PAGE_READONLY,
			PAGE_READWRITE,
			PAGE_EXECUTE_READWRITE};
	DWORD old;
	if (!VirtualProtect(ptr, length, protect[mode], &old))
		return -1;
#else
	static const unsigned char protect[] = {
			PROT_READ | PROT_EXEC,
			PROT_READ,
			PROT_READ | PROT_WRITE,
			PROT_READ | PROT_WRITE | PROT_EXEC};
	if (mprotect(ptr, length, protect[mode]))
		return -1;
	if (mode == HOST_PROT_RX || mode == HOST_PROT_RWX)
		host_icache_flush(ptr, length);
#endif
	return 0;
}

ST_FUNC MAYBE_UNUSED void *host_unwind_register(void *table, unsigned size_bytes, size_t base) {
#ifdef _WIN64
	if (!RtlAddFunctionTable((RUNTIME_FUNCTION *)table,
													 size_bytes / sizeof(RUNTIME_FUNCTION), base))
		return NULL;
	return table;
#else
	(void)table;
	(void)size_bytes;
	(void)base;
	return NULL;
#endif
}

ST_FUNC void host_unwind_unregister(void *table) {
#ifdef _WIN64
	if (table)
		RtlDeleteFunctionTable((RUNTIME_FUNCTION *)table);
#else
	(void)table;
#endif
}

#endif

#endif

#if defined MCC_TARGET_IS_HOST && MCC_CONFIG_DIAG_RT >= 1

#ifndef _WIN32
#include <signal.h>
#ifndef __OpenBSD__
#include <sys/ucontext.h>
#endif
#else
#define ucontext_t CONTEXT
#endif

ST_FUNC int host_fault_regs(void *osctx, HostFaultRegs *rc) {
	ucontext_t *uc = (ucontext_t *)osctx;
	rc->sp = 0;
#if defined _WIN64 && defined __aarch64__
	rc->pc = uc->Pc;
	rc->fp = uc->Fp;
	rc->sp = uc->Sp;
#elif defined _WIN64
	rc->pc = uc->Rip;
	rc->fp = uc->Rbp;
	rc->sp = uc->Rsp;
#elif defined _WIN32
	rc->pc = uc->Eip;
	rc->fp = uc->Ebp;
	rc->sp = uc->Esp;
#elif defined __i386__
#if defined(__APPLE__)
	rc->pc = uc->uc_mcontext->__ss.__eip;
	rc->fp = uc->uc_mcontext->__ss.__ebp;
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
	rc->pc = uc->uc_mcontext.mc_eip;
	rc->fp = uc->uc_mcontext.mc_ebp;
#elif defined(__dietlibc__)
	rc->pc = uc->uc_mcontext.eip;
	rc->fp = uc->uc_mcontext.ebp;
#elif defined(__NetBSD__)
	rc->pc = uc->uc_mcontext.__gregs[_REG_EIP];
	rc->fp = uc->uc_mcontext.__gregs[_REG_EBP];
#elif defined(__OpenBSD__)
	rc->pc = uc->sc_eip;
	rc->fp = uc->sc_ebp;
#elif !defined REG_EIP && defined EIP
	rc->pc = uc->uc_mcontext.gregs[EIP];
	rc->fp = uc->uc_mcontext.gregs[EBP];
#else
	rc->pc = uc->uc_mcontext.gregs[REG_EIP];
	rc->fp = uc->uc_mcontext.gregs[REG_EBP];
#endif
#elif defined(__x86_64__)
#if defined(__APPLE__)
	rc->pc = uc->uc_mcontext->__ss.__rip;
	rc->fp = uc->uc_mcontext->__ss.__rbp;
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
	rc->pc = uc->uc_mcontext.mc_rip;
	rc->fp = uc->uc_mcontext.mc_rbp;
#elif defined(__NetBSD__)
	rc->pc = uc->uc_mcontext.__gregs[_REG_RIP];
	rc->fp = uc->uc_mcontext.__gregs[_REG_RBP];
#elif defined(__OpenBSD__)
	rc->pc = uc->sc_rip;
	rc->fp = uc->sc_rbp;
#else
	rc->pc = uc->uc_mcontext.gregs[REG_RIP];
	rc->fp = uc->uc_mcontext.gregs[REG_RBP];
#endif
#elif defined(__arm__) && defined(__NetBSD__)
	rc->pc = uc->uc_mcontext.__gregs[_REG_PC];
	rc->fp = uc->uc_mcontext.__gregs[_REG_FP];
#elif defined(__arm__) && defined(__OpenBSD__)
	rc->pc = uc->sc_pc;
	rc->fp = uc->sc_r11;
#elif defined(__arm__) && defined(__FreeBSD__)
	rc->pc = uc->uc_mcontext.__gregs[_REG_PC];
	rc->fp = uc->uc_mcontext.__gregs[_REG_FP];
#elif defined(__arm__)
	rc->pc = uc->uc_mcontext.arm_pc;
	rc->fp = uc->uc_mcontext.arm_fp;
#elif defined(__aarch64__) && defined(__APPLE__)
	rc->pc = uc->uc_mcontext->__ss.__pc;
	rc->fp = uc->uc_mcontext->__ss.__fp;
#elif defined(__aarch64__) && defined(__FreeBSD__)
	rc->pc = uc->uc_mcontext.mc_gpregs.gp_elr;
	rc->fp = uc->uc_mcontext.mc_gpregs.gp_x[29];
#elif defined(__aarch64__) && defined(__NetBSD__)
	rc->pc = uc->uc_mcontext.__gregs[_REG_PC];
	rc->fp = uc->uc_mcontext.__gregs[_REG_FP];
#elif defined(__aarch64__) && defined(__OpenBSD__)
	rc->pc = uc->sc_elr;
	rc->fp = uc->sc_x[29];
#elif defined(__aarch64__)
	rc->pc = uc->uc_mcontext.pc;
	rc->fp = uc->uc_mcontext.regs[29];
#elif defined(__riscv) && defined(__OpenBSD__)
	rc->pc = uc->sc_sepc;
	rc->fp = uc->sc_s[0];
#elif defined(__riscv)
	rc->pc = uc->uc_mcontext.__gregs[REG_PC];
	rc->fp = uc->uc_mcontext.__gregs[REG_S0];
#endif
	return 0;
}

static host_fault_fn host_fault_cb;

#ifndef _WIN32

static void host_sig_handler(int signum, siginfo_t *siginf, void *puc) {
	HostFaultRegs r;
	int code;

	host_fault_regs(puc, &r);
	switch (signum) {
	case SIGFPE:
		switch (siginf->si_code) {
		case FPE_INTDIV:
		case FPE_FLTDIV:
			code = HOST_FAULT_DIVZERO;
			break;
		default:
			code = HOST_FAULT_FPE;
			break;
		}
		break;
	case SIGBUS:
	case SIGSEGV:
		code = HOST_FAULT_MEM;
		break;
	case SIGILL:
		code = HOST_FAULT_ILL;
		break;
	case SIGABRT:
		code = HOST_FAULT_ABORT;
		break;
	default:
		code = HOST_FAULT_OTHER;
		break;
	}
	host_fault_cb(code, signum, &r);
}

#ifndef SA_SIGINFO
#define SA_SIGINFO 0x00000004u
#endif

ST_FUNC void host_fault_install(host_fault_fn fn) {
	struct sigaction sigact;

	host_fault_cb = fn;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = host_sig_handler;
	sigaction(SIGFPE, &sigact, NULL);
	sigaction(SIGILL, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL);
	sigaction(SIGBUS, &sigact, NULL);
	sigaction(SIGABRT, &sigact, NULL);
}

ST_FUNC void host_fault_unblock(unsigned signum) {
	sigset_t s;
	sigemptyset(&s);
	sigaddset(&s, (int)signum);
	sigprocmask(SIG_UNBLOCK, &s, NULL);
}

#else

static long __stdcall host_seh_handler(EXCEPTION_POINTERS *ex_info) {
	HostFaultRegs r;
	unsigned code = ex_info->ExceptionRecord->ExceptionCode;
	int fc;

	host_fault_regs(ex_info->ContextRecord, &r);
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION:
		fc = HOST_FAULT_MEM;
		break;
	case EXCEPTION_STACK_OVERFLOW:
		fc = HOST_FAULT_STACK;
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		fc = HOST_FAULT_DIVZERO;
		break;
	case EXCEPTION_BREAKPOINT:
	case EXCEPTION_SINGLE_STEP:
		fc = HOST_FAULT_TRAP;
		break;
	default:
		fc = HOST_FAULT_OTHER;
		break;
	}
	if (host_fault_cb(fc, code, &r))
		return EXCEPTION_CONTINUE_SEARCH;
	return EXCEPTION_EXECUTE_HANDLER;
}

ST_FUNC void host_fault_install(host_fault_fn fn) {
	host_fault_cb = fn;
#ifdef _WIN64
	AddVectoredExceptionHandler(1, host_seh_handler);
#else
	SetUnhandledExceptionFilter(host_seh_handler);
#endif
}

ST_FUNC void host_fault_unblock(unsigned detail) {
	(void)detail;
}

#endif

#endif
