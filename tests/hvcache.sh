#!/bin/sh
MCCHV=$1
BDIR=$2
CDIR=$BDIR/hvcache-roundtrip
rm -rf "$CDIR"
mkdir -p "$CDIR" || exit 1

run() {
	MCCHV_CACHE_DIR=$CDIR "$MCCHV" "-B$BDIR" --seed "$1" --passes 2 \
		--workers 2 --seconds 0.6
}

out1=$(run 42)
rc=$?
[ $rc -eq 77 ] && exit 77
[ $rc -eq 0 ] || { echo "run1 failed rc=$rc"; exit 1; }
echo "$out1" | grep -q "hv: cache miss (cold)" || {
	echo "expected a cold miss on the first run"; exit 1; }
echo "$out1" | grep -q "hv:   cache file" || {
	echo "expected a cache write on the first run"; exit 1; }

out2=$(run 42)
[ $? -eq 0 ] || { echo "run2 failed"; exit 1; }
echo "$out2" | grep -q "hv: cache hit (warm-start)" || {
	echo "expected a warm hit on the second run"; exit 1; }
echo "$out2" | grep -q "hv: cache resume:" || {
	echo "expected resumed search state on the second run"; exit 1; }

key1=$(echo "$out1" | sed -n 's/.*intention \(0x[0-9a-f]*\).*/\1/p')
key2=$(echo "$out2" | sed -n 's/.*intention \(0x[0-9a-f]*\).*/\1/p')
[ -n "$key1" ] && [ "$key1" = "$key2" ] || {
	echo "intention key not stable across runs ($key1 vs $key2)"; exit 1; }

out3=$(run 43)
[ $? -eq 0 ] || { echo "run3 failed"; exit 1; }
echo "$out3" | grep -q "hv: cache miss (cold)" || {
	echo "expected a miss for the edited intention"; exit 1; }
key3=$(echo "$out3" | sed -n 's/.*intention \(0x[0-9a-f]*\).*/\1/p')
[ "$key1" != "$key3" ] || {
	echo "edited intention did not change the key"; exit 1; }

out4=$(env MCCHV_CACHE_DIR= XDG_CACHE_HOME= HOME= "$MCCHV" "-B$BDIR" \
	--seed 42 --passes 2 --workers 2 --seconds 0.6)
[ $? -eq 0 ] || { echo "run without a cache dir failed"; exit 1; }
echo "$out4" | grep -q "hv: cache disabled" || {
	echo "expected the cache to be disabled with no resolvable dir"; exit 1; }

echo OK
