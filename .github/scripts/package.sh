#!/usr/bin/env bash







set -euo pipefail

ver=${1#v}
plat=$2
stage=$PWD/stage
pkg=$PWD/pkg
out=$PWD/out
rm -rf "$pkg" "$out"
mkdir -p "$pkg" "$out"

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }



if [ -d "$stage/lib64" ]; then libdir=lib64; else libdir=lib; fi


d="mcc-$ver-$plat"
mkdir -p "$pkg/$d/bin" "$pkg/$d/lib"
cp "$stage/bin/mcc" "$pkg/$d/bin/"
cp -R "$stage/$libdir/mcc" "$pkg/$d/lib/"
tar czf "$out/$d.tar.gz" -C "$pkg" "$d"



d="libmcc-$ver-$plat"
mkdir -p "$pkg/$d/lib"
cp -R "$stage/include" "$pkg/$d/"
cp "$stage/$libdir/libmcc.a" "$pkg/$d/lib/"
[ -d "$stage/$libdir/cmake" ] && cp -R "$stage/$libdir/cmake" "$pkg/$d/lib/"
cp -R "$stage/$libdir/mcc" "$pkg/$d/lib/"
tar czf "$out/$d.tar.gz" -C "$pkg" "$d"


d="mcc-cross-$ver-$plat"
mkdir -p "$pkg/$d/bin" "$pkg/$d/lib/mcc"
found=0
for f in "$stage"/bin/mcc-*; do
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
