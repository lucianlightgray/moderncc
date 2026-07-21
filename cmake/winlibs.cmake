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
