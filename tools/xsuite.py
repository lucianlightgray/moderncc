#!/usr/bin/env python3
"""Run the gcc and llvm C test suites through an mcc binary.

Honors the suites' own directives: DejaGnu (`dg-do`, `dg-error`, `dg-options`,
`dg-require-effective-target`, target selectors) for the gcc tree, and lit
(`RUN:`, `REQUIRES:`, `UNSUPPORTED:`, `-verify`/`expected-error`) for the llvm
tree. A test whose directives ask for something this host/compiler cannot
express is skipped with a reason rather than counted as a failure.

Usage:
  tools/xsuite.py --mcc cmake-release/mcc-o3 --out cmake-release/xsuite \
                  --gcc /home/llg/Projects/gcc --llvm /home/llg/Projects/llvm-project \
                  --opt=-O0 --opt=-O3 [--jobs 32] [--limit N] [--suite gcc.dg ...]

The `--opt` value must be attached with `=`; argparse reads a bare `--opt -O0`
as a missing argument followed by an unknown flag.
"""
import argparse, fnmatch, json, os, re, resource, shlex, subprocess, sys, threading
from concurrent.futures import ThreadPoolExecutor

TRIPLE = "x86_64-pc-linux-gnu"

ET_TRUE = {
    "lp64", "int32plus", "int32", "size32plus", "ptr32plus", "stdint_types",
    "alloca", "c99_runtime", "fenv", "fenv_exceptions", "trampolines",
    "nonpic", "pie_enabled", "elf", "static", "gld", "unwrapped",
    "large_double", "double64", "double64plus", "long_double_64plus",
    "inttypes_types", "wchar_t_char32_t_compatible", "signed_char",
    "run_expensive_tests", "scheduling", "label_values", "indirect_jumps",
    "global_constructor", "init_priority", "weak_undefined", "cxa_atexit",
    "freorder", "gc_sections", "section_anchors", "string_merging",
    "tls_native", "tls_runtime", "pthread", "posix_memalign",
}
ET_FALSE = {
    "ilp32", "int16", "avr_tiny", "keeps_null_pointer_checks", "lto",
    "offload_nvptx", "offload_gcn", "vect_int", "vect_float", "vect_double",
    "arm_neon", "aarch64_sve", "powerpc_altivec_ok", "s390_vx", "riscv_v",
    "sync_int_128", "sync_int_128_runtime", "int128", "__int128",
    "untyped_assembly", "gas", "named_sections", "profiling",
    "fpic", "fopenmp", "fopenacc", "vla_in_struct", "sigsetjmp",
    "ucn", "ucn_nocache", "wchar", "c11_atomics", "openacc",
}

BAD_OPT_RE = re.compile(
    r"^-(m(?!s-bitfields|no-sse)|march=|mtune=|flto|fopenmp|fopenacc|fprofile|"
    r"fanalyzer|fsanitize|fgraphite|ftree-parallelize|fcf-protection|fpatchable|"
    r"fstack-clash|fharden|fipa-|fdevirtualize|fira-|fsched|fmodulo|funroll|"
    r"fpeel|ftracer|fvpt|fauto-profile|fcompare-debug|fdiagnostics-format|"
    r"std=(c\+\+|gnu\+\+))")
KEEP_OPT_RE = re.compile(r"^-(std=|D|I|U|O|include$|w$|pedantic|ansi$|"
                         r"W(?![lpa],)|"
                         r"f(no-)?(signed-char|unsigned-char|common|wrapv|builtin|"
                         r"strict-aliasing|gnu89-inline|short-enums|pic|PIC|pie|PIE|"
                         r"gnu-tm))")

ANSI_OK = False
GIMPLE_RE = re.compile(r"__GIMPLE|__RTL")

LIT_BAD_RE = re.compile(
    r"(-fopenmp|-fopenacc|-fcuda|-fhlsl|-fobjc|-x\s+(c\+\+|objective)|"
    r"-fsanitize|-fmodules|-fcoverage|-fprofile|-flto|-emit-pch|-emit-module|"
    r"-analyze|-ast-dump|-ast-print|-print-stats|-plugin|-target-feature|"
    r"-mllvm|-Xclang\s+-mllvm|--target=(?!x86_64)|-triple[= ](?!x86_64|%itanium|i386|i686))")

LIT_REQ_TRUE = {"x86-registered-target", "linux", "system-linux", "native",
                "shell", "target={{.*}}", "asserts", "x86_64-target-arch",
                "x86_64-linux", "clang", "c99", "c11", "c17"}


def read(path):
    try:
        with open(path, "r", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def sel_ok(sel):
    toks = [t for t in re.split(r"[\s{}]+", sel) if t and t not in ("target", "xfail")]
    if not toks:
        return True, ""
    pos, neg = [], []
    for t in toks:
        (neg if t.startswith("!") else pos).append(t.lstrip("!"))
    for t in neg:
        if tok_true(t) is True:
            return False, f"target-selector({t})"
    if not pos:
        return True, ""
    for t in pos:
        if tok_true(t) is True:
            return True, ""
    unknown = [t for t in pos if tok_true(t) is None]
    if unknown:
        return False, f"effective-target({unknown[0]})"
    return False, f"target-selector({pos[0]})"


def tok_true(t):
    if "-" in t and "*" in t:
        return fnmatch.fnmatch(TRIPLE, t.replace("?", "?"))
    if t in ET_TRUE:
        return True
    if t in ET_FALSE:
        return False
    if t.startswith(("vect_", "arm_", "aarch64_", "powerpc_", "s390_", "riscv_",
                     "avx", "sse", "mips_", "sparc_", "amdgcn", "nvptx")):
        return False
    return None


DRIVER_DIRS = ("/asan/", "/hwasan/", "/ubsan/", "/tsan/", "/msan/", "/dfsan/",
               "/analyzer/", "/plugin/", "/gcov/", "/lto/", "/gomp/", "/vect/",
               "/graphite/", "/dfp/", "/simulate-thread/", "/pch/", "/compat/",
               "/scudo/", "/safestack/", "/profile/", "/cfi/", "/memprof/",
               "/BlocksRuntime/", "/orc/", "/fuzzer/", "/xray/", "/tysan/",
               "/nsan/", "/gwp_asan/", "/interception/", "/sanitizer_common/")


SUITE_SKIP_DIRS = (
    ("/gcc.c-torture/execute/builtins/",
     "multi-file-test(builtins/lib half is never linked; every file fails on "
     "unresolved 'main')"),
    ("/gcc.dg/vmx/",
     "wrong-target-suite(vmx.exp returns unless powerpc*-*-* && "
     "powerpc_altivec_ok; needs altivec.h)"),
    ("/gcc.dg/goacc/",
     "openacc-suite(goacc.exp returns unless effective-target fopenacc; "
     "whole suite runs with -fopenacc)"),
    ("/gcc.dg/goacc-gomp/",
     "openacc-suite(goacc-gomp.exp returns unless fopenacc && fopenmp)"),
    ("/c-c++-common/goacc/",
     "openacc-suite(driven by gcc.dg/goacc/goacc.exp with -fopenacc; "
     "effective-target fopenacc is false)"),
    ("/c-c++-common/goacc-gomp/",
     "openacc-suite(driven by gcc.dg/goacc-gomp/goacc-gomp.exp with "
     "-fopenacc -fopenmp)"),
    ("/clang/test/Driver/",
     "driver-option-test(the option under test is not forwarded from the RUN "
     "line and the FileCheck assertions are never evaluated)"),
)


def suite_skip(path):
    p = path.replace(os.sep, "/")
    return next((why for d, why in SUITE_SKIP_DIRS if d in p), None)


def driver_dir(path):
    p = path.replace(os.sep, "/")
    return next((d.strip("/") for d in DRIVER_DIRS if d in p), None)


def gcc_plan(path, text, default_mode):
    flags, mode, expect = [], default_mode, "ok"
    why = suite_skip(path)
    if why:
        return None, None, None, why
    d = driver_dir(path)
    if d:
        return None, None, None, f"suite-driver-flags({d})"
    p = path.replace(os.sep, "/")
    if "/gcc.dg/special/" in p and not re.match(r".*[1-9]$",
                                                os.path.splitext(os.path.basename(p))[0]):
        return None, None, None, ("multi-file-aux(special.exp globs *[1-9].c only; "
                                  "this is an additional-source half, not a test)")
    if GIMPLE_RE.search(text):
        return None, None, None, ("gcc-ir-frontend(__GIMPLE/__RTL is GCC's internal-IR "
                                  "frontend, not C)")

    m = re.search(r"\{\s*dg-do\s+([a-z-]+)([^}]*)", text)
    if m:
        mode = m.group(1)
        ok, why = sel_ok(m.group(2))
        if not ok:
            return None, None, None, why
    if mode in ("preprocess",):
        mode = "preprocess"
    elif mode in ("compile", "assemble"):
        mode = "compile"
    elif mode == "link":
        mode = "link"
    elif mode == "run":
        mode = "run"
    else:
        return None, None, None, f"dg-do({mode})"

    if re.search(r"\{\s*dg-(additional-sources|extra-ld-options|lto-do|"
                 r"begin-multiline-output|regexp)", text):
        return None, None, None, "multi-file/unsupported-directive"

    for m in re.finditer(r"\{\s*dg-require-([a-z0-9_-]+)\s+([^}]*)", text):
        name, rest = m.group(1), m.group(2)
        if name == "effective-target":
            t = (rest.split() or ["?"])[0].strip('"')
            v = tok_true(t)
            if v is not True:
                return None, None, None, f"dg-require-effective-target({t})"
        elif name not in ("weak", "alias"):
            return None, None, None, f"dg-require-{name}"

    for m in re.finditer(r"\{\s*dg-skip-if\s+[^{]*\{([^}]*)\}", text):
        ok, _ = sel_ok(m.group(1))
        if ok:
            return None, None, None, "dg-skip-if"

    for m in re.finditer(r"\{\s*dg-(?:additional-)?options\s+\"([^\"]*)\"([^}]*)", text):
        sel = m.group(2)
        if sel.strip():
            ok, _ = sel_ok(sel)
            if not ok:
                continue
        try:
            args = shlex.split(m.group(1))
        except ValueError:
            continue
        for a in args:
            if BAD_OPT_RE.match(a):
                return None, None, None, f"dg-options({a})"
            if a == "-ansi" and not ANSI_OK:
                return None, None, None, "unsupported-option(-ansi)"
            if KEEP_OPT_RE.match(a):
                flags.append(a)

    for m in re.finditer(r"\{\s*dg-add-options\s+([a-z0-9_]+)", text):
        return None, None, None, f"dg-add-options({m.group(1)})"

    if re.search(r"\{\s*dg-error\b", text):
        expect = "reject"
        if mode == "run":
            mode = "compile"
    elif re.search(r"\{\s*dg-shouldfail\b", text) and mode == "run":
        expect = "runfail"
    if re.search(r"\{\s*dg-output\b", text):
        pass
    return mode, expect, flags, None


def lit_plan(path, text):
    why = suite_skip(path)
    if why:
        return None, None, None, why
    d = driver_dir(path)
    if d:
        return None, None, None, f"runtime-lib-suite({d})"
    runs = re.findall(r"^[^\n]*?(?://|#|/\*)\s*RUN:\s*(.*)$", text, re.M)
    if not runs:
        return None, None, None, "no-RUN-line"
    joined = " ".join(runs)
    if re.search(r"^[^\n]*?(?://|#)\s*(REQUIRES|UNSUPPORTED|XFAIL):\s*(.*)$", text, re.M):
        for m in re.finditer(r"^[^\n]*?(?://|#)\s*(REQUIRES|UNSUPPORTED|XFAIL):\s*(.*)$",
                             text, re.M):
            kind, body = m.group(1), m.group(2).strip()
            feats = [f.strip() for f in re.split(r"[,&|]", body) if f.strip()]
            if kind == "REQUIRES":
                if not all(f in LIT_REQ_TRUE for f in feats):
                    return None, None, None, f"REQUIRES({feats[0] if feats else '?'})"
            else:
                return None, None, None, f"{kind}({feats[0] if feats else '?'})"
    if LIT_BAD_RE.search(joined):
        m = LIT_BAD_RE.search(joined)
        return None, None, None, f"lit-flag({m.group(0).strip()[:24]})"
    if "%clang" not in joined and "%cc" not in joined:
        return None, None, None, "non-compiler-RUN"
    if "%S/Inputs" in joined or "%p/Inputs" in joined or "-include " in joined:
        pass

    flags = []
    for m in re.finditer(r"-std=([a-z0-9+]+)", joined):
        flags.append("-std=" + m.group(1))
        break
    for m in re.finditer(r"\s-D\s*([A-Za-z_][^\s\"']*)", joined):
        flags.append("-D" + m.group(1))

    expect = "reject" if ("-verify" in joined and "expected-error" in text) or \
                         re.search(r"\bnot\s+%clang", joined) else "ok"
    if "expected-no-diagnostics" in text:
        expect = "ok"

    mode = "compile"
    if re.search(r"%run\s+%t|&&\s*%t\b|&&\s*\./%t", joined):
        mode = "run"
    elif " -E" in joined and "-emit" not in joined:
        mode = "preprocess"
    return mode, expect, flags, None


def limits_compile():
    resource.setrlimit(resource.RLIMIT_AS, (8 << 30, 8 << 30))
    resource.setrlimit(resource.RLIMIT_FSIZE, (1 << 30, 1 << 30))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def limits_run():
    resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
    resource.setrlimit(resource.RLIMIT_FSIZE, (1 << 30, 1 << 30))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


ICE_RE = re.compile(r"internal compiler error", re.I)
ECHO_RE = re.compile(r"^\s*(?:\d+\s*)?\|")


def is_ice(rc, msg):
    if rc < 0 or rc >= 128:
        return True
    body = "\n".join(l for l in (msg or "").splitlines() if not ECHO_RE.match(l))
    return bool(ICE_RE.search(body))


def err_raw(msg):
    for line in (msg or "").splitlines():
        if "error:" in line:
            return line.strip()[:200]
    return ""


def err_key(msg):
    lines = [l.strip() for l in (msg or "").splitlines() if l.strip()]
    cand = [l for l in lines if "error:" in l] or \
           [l for l in lines if "fatal" in l.lower() or "unsupported" in l.lower()] or lines
    for line in cand:
        i = line.find("error:")
        if i >= 0:
            line = line[i:]
        line = re.sub(r"^[^\s:]*[/\\][^\s:]*:\d+:(\d+:)?\s*", "", line)
        line = re.sub(r"'[^']*'", "'X'", line)
        line = re.sub(r"\"[^\"]*\"", '"X"', line)
        line = re.sub(r"\b\d+\b", "N", line)
        if line.lower().startswith(("in file included", "warning:")):
            continue
        return line[:110]
    return ""


class Runner:
    def __init__(self, mcc, out, jobs, ctimeout, rtimeout, force_opt=False, ref=""):
        self.mcc, self.out, self.ref = mcc, out, ref
        self.ctimeout, self.rtimeout = ctimeout, rtimeout
        self.force_opt = force_opt
        self.lock = threading.Lock()
        self.fh = open(os.path.join(out, "results.jsonl"), "w")
        self.n = 0
        self.jobs = jobs

    def emit(self, rec, base=None):
        for ext in (".o", ".x") if base else ():
            try:
                os.unlink(base + ext)
            except OSError:
                pass
        with self.lock:
            self.fh.write(json.dumps(rec) + "\n")
            self.n += 1
            if self.n % 2000 == 0:
                print(f"  ... {self.n} results", flush=True)

    def ref_verdict(self, cmd, base, work, mode):
        if not self.ref:
            return None
        rcmd = [self.ref] + cmd[1:]
        if REF_STD and not any(a.startswith("-std=") or a == "-ansi" for a in rcmd):
            rcmd.insert(1, REF_STD)
        try:
            os.unlink(base + ".ref.o")
        except OSError:
            pass
        rcmd = [a if a != base + ".o" else base + ".ref.o" for a in rcmd]
        rcmd = [a if a != base + ".x" else base + ".ref.x" for a in rcmd]
        try:
            p = subprocess.run(rcmd, capture_output=True, text=True, errors="replace",
                               timeout=self.ctimeout, preexec_fn=limits_compile, cwd=work)
        except (OSError, subprocess.SubprocessError):
            return "error"
        err = (p.stderr or "")
        if REF_BADFLAG_RE.search(err):
            return "badflag"
        for ext in (".ref.o", ".ref.x"):
            try:
                os.unlink(base + ext)
            except OSError:
                pass
        return "ok" if p.returncode == 0 else "reject"

    def one(self, t, opt, idx):
        work = os.path.join(self.out, "w%02d" % (idx % self.jobs))
        os.makedirs(work, exist_ok=True)
        src, mode, expect = t["file"], t["mode"], t["expect"]
        base = os.path.join(work, "t%d" % idx)
        rec = {"suite": t["suite"], "file": src, "opt": opt, "mode": mode,
               "expect": expect}
        inc = ["-I" + os.path.dirname(src)] + ["-I" + d for d in t.get("inc", [])]
        flags = [a for a in t["flags"] if not (self.force_opt and a.startswith("-O"))]
        cmd = [self.mcc] + ([] if self.force_opt else [opt]) + flags + inc
        if self.force_opt:
            cmd.append(opt)
        if mode == "preprocess":
            cmd += ["-E", src]
        elif mode == "compile":
            cmd += ["-c", src, "-o", base + ".o"]
        else:
            cmd += [src, "-o", base + ".x", "-lm"]
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, errors="replace",
                               timeout=self.ctimeout, preexec_fn=limits_compile,
                               cwd=work)
            crc, cerr = p.returncode, (p.stderr or "") + (p.stdout or "" if mode != "preprocess" else "")
        except subprocess.TimeoutExpired:
            rec.update(status="TIMEOUT", stage="compile")
            return self.emit(rec, base)

        if expect == "reject":
            rec.update(status="PASS" if crc != 0 else "XPASS", stage="compile", rc=crc)
            if crc == 0:
                rec["err"] = ""
                v = self.ref_verdict(cmd, base, work, mode)
                if v:
                    rec["ref"] = v
                if v == "ok":
                    rec["status"] = "XPASS_REFOK"
            return self.emit(rec, base)
        if crc != 0:
            rec.update(status="ICE" if is_ice(crc, cerr) else "FAIL",
                       stage="compile", rc=crc, err=err_key(cerr),
                       err_raw=err_raw(cerr))
            v = self.ref_verdict(cmd, base, work, mode)
            if v:
                rec["ref"] = v
            if v == "reject":
                rec["status"] = "REFFAIL"
            return self.emit(rec, base)
        if mode != "run":
            rec.update(status="PASS", stage="compile", rc=0)
            return self.emit(rec, base)
        try:
            q = subprocess.run([base + ".x"], capture_output=True, text=True,
                               errors="replace", timeout=self.rtimeout,
                               preexec_fn=limits_run, cwd=work)
        except subprocess.TimeoutExpired:
            rec.update(status="TIMEOUT", stage="run")
            return self.emit(rec, base)
        want_fail = expect == "runfail"
        if (q.returncode != 0) != want_fail:
            rec.update(status="FAILEXE", stage="run", rc=q.returncode,
                       err=err_key(q.stderr))
        else:
            rec.update(status="PASS", stage="run", rc=q.returncode)
        return self.emit(rec, base)


DG_OPTIONS_RE = re.compile(r"\{\s*dg-(?:additional-)?options\b")


def suite_default_flags(path, text, G):
    if DG_OPTIONS_RE.search(text):
        return []
    p = path.replace(os.sep, "/")
    G = G.replace(os.sep, "/").rstrip("/") + "/"
    for sub, extra in (("gcc.dg/", ["-ansi", "-pedantic-errors"]),
                       ("c-c++-common/", ["-Wc++-compat"])):
        if p.startswith(G + sub) and "/" not in p[len(G + sub):]:
            return extra
    return []


REF_STD = ""


def probe_ref_std(mcc):
    try:
        p = subprocess.run([mcc, "-E", "-dM", "-"], input="", capture_output=True,
                           text=True, errors="replace", timeout=30)
    except (OSError, subprocess.SubprocessError):
        return ""
    m = re.search(r"__STDC_VERSION__\s+(\d+)L", p.stdout or "")
    if not m:
        return ""
    gnu = "__STRICT_ANSI__" not in (p.stdout or "")
    ver = {"202311": "23", "201710": "17", "201112": "11",
           "199901": "99"}.get(m.group(1))
    if not ver:
        return ""
    return "-std=" + ("gnu" if gnu else "c") + ver


REF_BADFLAG_RE = re.compile(
    r"unknown argument|unsupported option|unrecognized command|"
    r"no such file or directory: '-|argument unused|not supported")


def probe_opt(mcc, opt):
    try:
        p = subprocess.run([mcc, opt, "-E", "-"], input="", capture_output=True,
                           text=True, errors="replace", timeout=30)
        return p.returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def collect(args):
    tests, skips = [], []
    dflt_flags = getattr(args, "suite_default_flags", False)
    G = os.path.join(args.gcc, "gcc", "testsuite")
    gsuites = [("gcc.c-torture/execute", "run"), ("gcc.c-torture/compile", "compile"),
               ("gcc.c-torture/unsorted", "compile"), ("gcc.dg", "compile"),
               ("gcc.misc-tests", "compile"), ("c-c++-common", "compile"),
               ("gcc.target/i386", "compile"), ("gcc.target/x86_64", "compile")]
    if args.gcc and os.path.isdir(G):
        for sub, dflt in gsuites:
            root = os.path.join(G, sub)
            if not os.path.isdir(root):
                continue
            for dirpath, dirnames, files in os.walk(root):
                dirnames[:] = [d for d in dirnames if d not in ("compat",)]
                for f in sorted(files):
                    if not f.endswith(".c"):
                        continue
                    p = os.path.join(dirpath, f)
                    txt = read(p)
                    mode, expect, flags, why = gcc_plan(p, txt, dflt)
                    name = "gcc:" + sub.split("/")[0] if not sub.startswith("gcc.c-torture") \
                        else "gcc:c-torture/" + sub.split("/")[-1]
                    if why:
                        skips.append({"suite": name, "file": p, "status": "SKIP",
                                      "reason": why})
                        continue
                    pre = ["-w"] if sub.startswith("gcc.c-torture") else []
                    if dflt_flags:
                        pre = pre + suite_default_flags(p, txt, G)
                    tests.append({"suite": name, "file": p, "mode": mode,
                                  "expect": expect, "flags": pre + flags,
                                  "inc": [G, os.path.join(G, "gcc.dg")]})
    L = args.llvm
    if L and os.path.isdir(L):
        lsuites = [("clang/test", "llvm:clang"), ("compiler-rt/test", "llvm:compiler-rt"),
                   ("llvm/test", "llvm:llvm")]
        for sub, name in lsuites:
            root = os.path.join(L, sub)
            if not os.path.isdir(root):
                continue
            for dirpath, dirnames, files in os.walk(root):
                dirnames[:] = [d for d in dirnames if d != "Inputs"]
                for f in sorted(files):
                    if not f.endswith(".c"):
                        continue
                    p = os.path.join(dirpath, f)
                    txt = read(p)
                    mode, expect, flags, why = lit_plan(p, txt)
                    if why:
                        skips.append({"suite": name, "file": p, "status": "SKIP",
                                      "reason": why})
                        continue
                    tests.append({"suite": name, "file": p, "mode": mode,
                                  "expect": expect, "flags": flags,
                                  "inc": [os.path.join(dirpath, "Inputs"),
                                          os.path.join(L, "compiler-rt/lib/builtins"),
                                          os.path.join(L, "compiler-rt/test/builtins/Unit")]})
    return tests, skips


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mcc", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--gcc", default="")
    ap.add_argument("--llvm", default="")
    ap.add_argument("--opt", action="append", default=[])
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--suite", action="append", default=[])
    ap.add_argument("--ctimeout", type=float, default=25.0)
    ap.add_argument("--rtimeout", type=float, default=15.0)
    ap.add_argument("--force-opt", action="store_true")
    ap.add_argument("--files", default="")
    ap.add_argument("--ref", default="")
    ap.add_argument("--suite-default-flags", action="store_true")
    args = ap.parse_args()
    opts = args.opt or ["-O0"]
    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    global ANSI_OK, REF_STD
    ANSI_OK = probe_opt(os.path.abspath(args.mcc), "-ansi")
    print(f"xsuite: mcc accepts -ansi: {ANSI_OK}", flush=True)
    if args.ref:
        REF_STD = probe_ref_std(os.path.abspath(args.mcc))
        print(f"xsuite: reference {args.ref}, default std forwarded as "
              f"{REF_STD or '(none detected)'}", flush=True)
    if args.suite_default_flags and not ANSI_OK:
        sys.exit("xsuite: --suite-default-flags needs an mcc that accepts -ansi")

    tests, skips = collect(args)
    if args.files:
        want = {l.strip() for l in open(args.files) if l.strip()}
        tests = [t for t in tests if t["file"] in want]
        skips = [s for s in skips if s["file"] in want]
    if args.suite:
        keep = lambda s: any(x in s for x in args.suite)
        tests = [t for t in tests if keep(t["suite"])]
        skips = [s for s in skips if keep(s["suite"])]
    if args.limit:
        tests = tests[:args.limit]
    print(f"xsuite: {len(tests)} runnable tests, {len(skips)} skipped by directives, "
          f"opts={opts}, jobs={args.jobs}", flush=True)

    r = Runner(os.path.abspath(args.mcc), args.out, args.jobs, args.ctimeout,
               args.rtimeout, args.force_opt, args.ref)
    for s in skips:
        r.fh.write(json.dumps(s) + "\n")
    work = [(t, o) for t in tests for o in opts]
    slots = {}
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        def job(i, t, o):
            r.one(t, o, i)
        list(ex.map(lambda a: job(*a), [(i, t, o) for i, (t, o) in enumerate(work)]))
    r.fh.close()
    print(f"xsuite: wrote {os.path.join(args.out, 'results.jsonl')}")


if __name__ == "__main__":
    main()
