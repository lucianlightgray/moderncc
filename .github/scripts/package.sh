#!/usr/bin/env bash
# Package the installed mcc tree (in ./stage) into per-artifact tarballs.
#   $1 = git ref name (e.g. v1.2.3)   $2 = platform slug (e.g. linux-x86_64)
# Produces, under ./out:
#   mcc-<ver>-<plat>.tar.gz        static+stripped mcc + native runtime
#   libmcc-<ver>-<plat>.tar.gz     libmcc.a + headers + cmake package + runtime
#   mcc-cross-<ver>-<plat>.tar.gz  <arch>-mcc cross compilers + <arch>-libmcc1.a
#   checksums-<plat>.txt           sha256 of the three tarballs
set -euo pipefail

ver=${1#v}
plat=$2
stage=$PWD/stage
pkg=$PWD/pkg
out=$PWD/out
rm -rf "$pkg" "$out"
mkdir -p "$pkg" "$out"

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }

# GNUInstallDirs picks lib or lib64 depending on the distro (Ubuntu: lib,
# Gentoo/Fedora: lib64). Resolve whichever the install actually used.
if [ -d "$stage/lib64" ]; then libdir=lib64; else libdir=lib; fi

# 1) mcc: the native compiler + its runtime (libmcc1.a + bundled headers).
d="mcc-$ver-$plat"
mkdir -p "$pkg/$d/bin" "$pkg/$d/lib"
cp "$stage/bin/mcc" "$pkg/$d/bin/"
cp -R "$stage/$libdir/mcc" "$pkg/$d/lib/"
tar czf "$out/$d.tar.gz" -C "$pkg" "$d"

# 2) libmcc: the embeddable static library, its header, cmake package, and the
#    runtime it needs at link time.
d="libmcc-$ver-$plat"
mkdir -p "$pkg/$d/lib"
cp -R "$stage/include" "$pkg/$d/"
cp "$stage/$libdir/libmcc.a" "$pkg/$d/lib/"
[ -d "$stage/$libdir/cmake" ] && cp -R "$stage/$libdir/cmake" "$pkg/$d/lib/"
cp -R "$stage/$libdir/mcc" "$pkg/$d/lib/"
tar czf "$out/$d.tar.gz" -C "$pkg" "$d"

# 3) cross: the <arch>-mcc cross compilers and their libmcc1 runtime archives.
d="mcc-cross-$ver-$plat"
mkdir -p "$pkg/$d/bin" "$pkg/$d/lib/mcc"
found=0
for f in "$stage"/bin/*-mcc; do
  [ -e "$f" ] || continue
  cp "$f" "$pkg/$d/bin/"; found=1
done
find "$stage/$libdir/mcc" -name '*-libmcc1.a' -exec cp {} "$pkg/$d/lib/mcc/" \; 2>/dev/null || true
if [ "$found" = 1 ]; then
  tar czf "$out/$d.tar.gz" -C "$pkg" "$d"
else
  echo "note: no cross compilers found in stage/bin; skipping cross bundle"
fi

( cd "$out" && sha256 ./*.tar.gz > "checksums-$plat.txt" )
echo "== packaged =="
ls -l "$out"
