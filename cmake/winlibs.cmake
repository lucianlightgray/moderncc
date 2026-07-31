set(MCC_MINGW_WINLIBS_VER "16.1.0-ucrt" CACHE STRING
    "WinLibs GCC version tag used in the vendor/winlibs-mingw-w64-<ver>-<arch> dir names")

set(_wl "https://github.com/brechtsanders/winlibs_mingw/releases/download/16.1.0posix-14.0.0-ucrt-r3")
set(MCC_MINGW_WINLIBS_X86_64_URL
    "${_wl}/winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip"
    CACHE STRING "WinLibs x86_64 (64-bit) toolchain zip URL")
set(MCC_MINGW_WINLIBS_X86_64_SHA256
    "4273565109cd8ab8ecef1b0dc2a56fd7f5c7ee0885840a1c011b9325160ec0c3"
    CACHE STRING "SHA256 of the WinLibs x86_64 zip")
set(MCC_MINGW_WINLIBS_I686_URL
    "${_wl}/winlibs-i686-posix-dwarf-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip"
    CACHE STRING "WinLibs i686 (native 32-bit) toolchain zip URL")
set(MCC_MINGW_WINLIBS_I686_SHA256
    "c4c7419f2820ac2e169dc86f1397a07deff20297a107c7a1ca486643d8d435be"
    CACHE STRING "SHA256 of the WinLibs i686 zip")
unset(_wl)

# llvm-mingw (mstorsjo) is the only self-contained mingw toolchain that ships an
# aarch64-w64-mingw32 compiler; WinLibs is x86_64/i686 only. MCC_MINGW_ARCH=arm64
# pulls this instead. The zip unpacks to an inner llvm-mingw-<ver>-ucrt-aarch64/
# directory (unlike WinLibs' mingw64/); the arm64 resolve path accounts for that.
# The aarch64-host bundle is what windows-11-arm runs natively.
set(MCC_LLVMMINGW_VER "20260616" CACHE STRING
    "llvm-mingw release tag for the aarch64 mingw toolchain profile")
set(MCC_LLVMMINGW_AARCH64_URL
    "https://github.com/mstorsjo/llvm-mingw/releases/download/${MCC_LLVMMINGW_VER}/llvm-mingw-${MCC_LLVMMINGW_VER}-ucrt-aarch64.zip"
    CACHE STRING "llvm-mingw aarch64-host (arm64) toolchain zip URL")
set(MCC_LLVMMINGW_AARCH64_SHA256
    "312593669435bd0bfc1a43ac3fba23c8b27e0610bade88b2738e5a01702a99ba"
    CACHE STRING "SHA256 of the llvm-mingw aarch64 zip")
