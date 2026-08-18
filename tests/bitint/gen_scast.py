import sys
out=sys.argv[1]
mask64=(1<<64)-1
# widths spanning (128,256) and (256,512), plus the shift-skipped exact-width cases
Ns=[129,130,160,192,200,255,257,300,320,384,400,448,500,511,256,512]
cases=[]
for N in Ns:
    # value with sign bit (N-1) set and some low entropy -> signed is negative
    uval=(1<<(N-1)) | 0x0123456789abcdef
    sval=uval-(1<<N)
    q=-((-sval)//7) if sval<0 else sval//7
    r=sval-q*7
    cases.append((N,uval,sval,q,r))

L=[]
L.append('/* Regression for T-mac-30063: unsigned _BitInt(N) -> signed _BitInt(N)')
L.append(' * cast must sign-extend bit N-1 (arithmetic fill), so the value reads')
L.append(' * negative and signed div/mod/compare are correct. Oracle: Python bignum')
L.append(' * (tests/bitint/gen_scast.py). Self-checking: main returns the number of')
L.append(' * failed checks (0 == pass) and prints a summary line the ctest matches. */')
L.append('#include <stdio.h>')
L.append('')
L.append('static int fails = 0;')
L.append('static int checks = 0;')
L.append('#define CK(cond, tag) do { checks++; if (!(cond)) { fails++; \\')
L.append('    printf("FAIL %s\\n", tag); } } while (0)')
L.append('')
L.append('/* MCC_SCAST_MUTATE: corrupt one expected value so a *correct* compiler')
L.append(' * reports a failure -- proves the harness is non-vacuous (T-lin-10003). */')
L.append('#ifdef MCC_SCAST_MUTATE')
L.append('#define WANT_SIGN 0')
L.append('#else')
L.append('#define WANT_SIGN 1')
L.append('#endif')
L.append('')
L.append('int main(void) {')
for (N,uval,sval,q,r) in cases:
    lo=uval & mask64
    hi=(uval>>64) & mask64
    qlo=q & mask64
    rlo=r & mask64
    L.append('  {')
    L.append(f'    unsigned _BitInt({N}) u = (unsigned _BitInt({N}))0x{lo:x}ULL;')
    if hi:
        L.append(f'    u |= (unsigned _BitInt({N}))0x{hi:x}ULL << 64;')
    L.append(f'    u |= (unsigned _BitInt({N}))1 << {N-1};')
    L.append(f'    signed _BitInt({N}) s = (signed _BitInt({N}))u;')
    L.append(f'    signed _BitInt({N}) q = s / (signed _BitInt({N}))7;')
    L.append(f'    signed _BitInt({N}) r = s % (signed _BitInt({N}))7;')
    L.append(f'    CK((s < 0) == WANT_SIGN, "N{N} sign");')
    L.append(f'    CK((unsigned long long)q == 0x{qlo:x}ULL, "N{N} qlo");')
    L.append(f'    CK((long long)r == {r}LL, "N{N} rmod");')
    L.append(f'    CK(s < (signed _BitInt({N}))500, "N{N} cmp");')
    # unsigned >> must stay logical: top bit becomes 0, so as-signed non-negative
    L.append(f'    unsigned _BitInt({N}) v = u >> 1;')
    L.append(f'    CK((signed _BitInt({N}))v >= 0, "N{N} ushr-logical");')
    L.append('  }')
L.append('  printf("bitint-scast checks=%d fails=%d\\n", checks, fails);')
L.append('  return fails;')
L.append('}')
open(out,'w').write('\n'.join(L)+'\n')
# report expected check count
print("checks_expected", len(cases)*5)
