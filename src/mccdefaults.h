#ifndef MCC_DEFAULTS_H
#define MCC_DEFAULTS_H

#if defined MCC_CONFIG_SYSROOT && defined MCC_CONFIG_CROSSPREFIX
#define MCC_SYSROOTED_CROSS 1
#elif defined MCC_CONFIG_SYSROOT
#define MCC_SYSROOTED_NATIVE 1
#endif

#if MCC_CONFIG_PIE
#define MCC_CONFIG_PIC 1
#endif

#ifndef MCC_CONFIG_SYSROOT
#define MCC_CONFIG_SYSROOT ""
#endif

#ifdef MCC_CONFIG_TRIPLET
#define USE_TRIPLET(s) s "/" MCC_CONFIG_TRIPLET
#define ALSO_TRIPLET(s) USE_TRIPLET(s) HOST_PATHSEP s
#else
#define USE_TRIPLET(s) s
#define ALSO_TRIPLET(s) s
#endif

#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64
#define MCC_LIBSUF64 "64"
#define MCC_ELF_LIBDIRS                                                                    \
	USE_TRIPLET("{R}/usr/lib")                                                               \
	HOST_PATHSEP                                                                             \
	"{R}/usr/lib64" HOST_PATHSEP "{R}/lib64" HOST_PATHSEP "{R}/usr/local/lib64" HOST_PATHSEP \
	"{R}/usr/lib" HOST_PATHSEP "{R}/lib" HOST_PATHSEP "{R}/usr/local/lib"
#else
#define MCC_LIBSUF64 ""
#define MCC_ELF_LIBDIRS                                 \
	USE_TRIPLET("{R}/usr/lib")                            \
	HOST_PATHSEP                                          \
	"{R}/usr/lib32" HOST_PATHSEP "{R}/lib32" HOST_PATHSEP \
	"{R}/usr/lib" HOST_PATHSEP "{R}/lib" HOST_PATHSEP "{R}/usr/local/lib"
#endif

#ifndef MCC_CONFIG_SYSINCLUDEPATHS
#if defined MCC_TARGET_PE || MCC_HOST_WIN32
#define MCC_CONFIG_SYSINCLUDEPATHS \
	"{B}/include" HOST_PATHSEP "{B}/include/winapi"
#elif defined MCC_TARGETOS_ANDROID && defined MCC_CONFIG_TRIPLET
#define MCC_CONFIG_SYSINCLUDEPATHS \
	"{B}/include" HOST_PATHSEP "{R}/include" HOST_PATHSEP "{R}/include/" MCC_CONFIG_TRIPLET
#elif defined MCC_TARGETOS_ANDROID || MCC_SYSROOTED_CROSS || (MCC_CONFIG_MUSL && MCC_SYSROOTED_NATIVE)
#define MCC_CONFIG_SYSINCLUDEPATHS \
	"{B}/include" HOST_PATHSEP "{R}/include"
#else
#define MCC_CONFIG_SYSINCLUDEPATHS \
	"{B}/include" HOST_PATHSEP ALSO_TRIPLET("{R}/usr/include")
#endif
#endif

#ifndef MCC_CONFIG_LIBPATHS
#if defined MCC_TARGET_PE || MCC_HOST_WIN32
#define MCC_CONFIG_LIBPATHS \
	"{B}/lib"
#elif defined MCC_TARGETOS_ANDROID
#define MCC_CONFIG_LIBPATHS \
	"{B}" HOST_PATHSEP "{R}/lib" HOST_PATHSEP "/system/lib" MCC_LIBSUF64
#elif MCC_SYSROOTED_CROSS
#define MCC_CONFIG_LIBPATHS \
	"{R}/lib" HOST_PATHSEP "{B}"
#elif MCC_CONFIG_MUSL && MCC_SYSROOTED_NATIVE
#define MCC_CONFIG_LIBPATHS \
	"{B}" HOST_PATHSEP "{R}/lib"
#else
#define MCC_CONFIG_LIBPATHS \
	"{B}" HOST_PATHSEP MCC_ELF_LIBDIRS
#endif
#endif

#ifndef MCC_CONFIG_CRTPREFIX
#if defined MCC_TARGETOS_ANDROID || MCC_SYSROOTED_CROSS || (MCC_CONFIG_MUSL && MCC_SYSROOTED_NATIVE)
#define MCC_CONFIG_CRTPREFIX \
	"{R}/lib"
#else
#define MCC_CONFIG_CRTPREFIX \
	MCC_ELF_LIBDIRS
#endif
#endif

#if MCC_CONFIG_MUSL && MCC_SYSROOTED_NATIVE
#define MCC_MUSL_LDSO(name) MCC_CONFIG_SYSROOT "/lib/" name
#else
#define MCC_MUSL_LDSO(name) "/lib/" name
#endif

#ifndef MCC_CONFIG_ELFINTERP
#if MCC_HOST_HURD
#define MCC_CONFIG_ELFINTERP "/lib/ld.so"
#elif defined MCC_TARGET_PE
#define MCC_CONFIG_ELFINTERP "-"
#elif defined MCC_TARGETOS_ANDROID
#define MCC_CONFIG_ELFINTERP "/system/bin/linker" MCC_LIBSUF64
#elif defined TARGETOS_FreeBSD || defined MCC_TARGETOS_FreeBSD
#define MCC_CONFIG_ELFINTERP "/libexec/ld-elf.so.1"
#elif defined TARGETOS_DragonFly || defined MCC_TARGETOS_DragonFly
#define MCC_CONFIG_ELFINTERP "/usr/libexec/ld-elf.so.2"
#elif defined TARGETOS_NetBSD || defined MCC_TARGETOS_NetBSD
#define MCC_CONFIG_ELFINTERP "/usr/libexec/ld.elf_so"
#elif defined TARGETOS_OpenBSD || defined MCC_TARGETOS_OpenBSD
#define MCC_CONFIG_ELFINTERP "/usr/libexec/ld.so"
#elif MCC_CONFIG_MUSL
#if defined MCC_TARGET_ARM64
#define MCC_CONFIG_ELFINTERP MCC_MUSL_LDSO("ld-musl-aarch64.so.1")
#elif defined MCC_TARGET_X86_64
#define MCC_CONFIG_ELFINTERP MCC_MUSL_LDSO("ld-musl-x86_64.so.1")
#elif defined MCC_TARGET_RISCV64
#define MCC_CONFIG_ELFINTERP MCC_MUSL_LDSO("ld-musl-riscv64.so.1")
#elif defined MCC_TARGET_ARM && defined MCC_ARM_HARDFLOAT
#define MCC_CONFIG_ELFINTERP MCC_MUSL_LDSO("ld-musl-armhf.so.1")
#elif defined MCC_TARGET_ARM
#define MCC_CONFIG_ELFINTERP MCC_MUSL_LDSO("ld-musl-arm.so.1")
#else
#define MCC_CONFIG_ELFINTERP MCC_MUSL_LDSO("ld-musl-i386.so.1")
#endif
#elif defined MCC_TARGET_ARM64
#define MCC_CONFIG_ELFINTERP "/lib/ld-linux-aarch64.so.1"
#elif defined MCC_TARGET_X86_64
#define MCC_CONFIG_ELFINTERP "/lib64/ld-linux-x86-64.so.2"
#elif defined MCC_TARGET_RISCV64
#define MCC_CONFIG_ELFINTERP "/lib/ld-linux-riscv64-lp64d.so.1"
#elif defined MCC_TARGET_ARM
#define MCC_CONFIG_ELFINTERP "/lib/ld-linux.so.3"
#define MCC_CONFIG_ELFINTERP_ARMHF "/lib/ld-linux-armhf.so.3"
#else
#define MCC_CONFIG_ELFINTERP "/lib/ld-linux.so.2"
#endif
#endif

#if !defined MCC_CONFIG_SWITCHES && defined MCC_TARGETOS_ANDROID
#define MCC_CONFIG_SWITCHES "-Wl,-rpath=" MCC_CONFIG_SYSROOT "/lib"
#endif

#ifndef MCC_MCCRT
#define MCC_MCCRT "libmccrt.a"
#endif

#ifndef MCC_CONFIG_CROSSPREFIX
#define MCC_CONFIG_CROSSPREFIX ""
#endif

typedef struct MCCTargetDefaults {
	const char *sysincludepaths;
	const char *libpaths;
	const char *crtprefix;
	const char *elfinterp;
	const char *elfinterp_armhf;
	const char *switches;
	unsigned char pie;
	unsigned char pic;
} MCCTargetDefaults;

static const MCCTargetDefaults mcc_target_defaults = {
		MCC_CONFIG_SYSINCLUDEPATHS,
		MCC_CONFIG_LIBPATHS,
		MCC_CONFIG_CRTPREFIX,
		MCC_CONFIG_ELFINTERP,
#ifdef MCC_CONFIG_ELFINTERP_ARMHF
		MCC_CONFIG_ELFINTERP_ARMHF,
#else
		NULL,
#endif
#ifdef MCC_CONFIG_SWITCHES
		MCC_CONFIG_SWITCHES,
#else
		NULL,
#endif
#if MCC_CONFIG_PIE
		1,
#else
		0,
#endif
#if MCC_CONFIG_PIC
		2,
#else
		0,
#endif
};

#endif
