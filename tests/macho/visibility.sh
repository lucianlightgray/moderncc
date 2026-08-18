#!/bin/sh
# T-mac-30157: hidden/internal visibility must become a Mach-O private_extern
# (nm -m "private external") in the linked image, not a plain "external".
set -e
MCC="$1"; SRCDIR="$2"; WORK="$3"; BFLAG="$4"
command -v nm >/dev/null 2>&1 || { echo "SKIP: no nm"; exit 77; }
mkdir -p "$WORK"
cat > "$WORK/vis.c" <<'CEOF'
__attribute__((visibility("hidden")))   int hid_fn(void){return 1;}
__attribute__((visibility("internal"))) int int_fn(void){return 4;}
__attribute__((visibility("hidden")))   int hid_var = 7;
int plain_fn(void){return 3;}
int main(void){ return hid_fn()+int_fn()+hid_var+plain_fn(); }
CEOF
"$MCC" $BFLAG "$WORK/vis.c" -o "$WORK/vis" || { echo "FAIL: compile/link"; exit 1; }
out=$(nm -m "$WORK/vis" 2>/dev/null)
check() { # name  expected-substring
    line=$(printf '%s\n' "$out" | grep -E "_$1\$") || { echo "FAIL: no symbol _$1"; exit 1; }
    case "$line" in
        *"$2"*) echo "PASS: _$1 is $2" ;;
        *) echo "FAIL: _$1 expected '$2' got: $line"; exit 1 ;;
    esac
}
check hid_fn  "private external"
check int_fn  "private external"
check hid_var "private external"
check plain_fn "external"
# and plain_fn must NOT be private external
printf '%s\n' "$out" | grep -E "_plain_fn\$" | grep -q "private external" && { echo "FAIL: plain_fn wrongly private"; exit 1; }
echo "PASS: visibility mapped to private_extern correctly"
