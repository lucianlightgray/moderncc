/* Regression for T-mac-30063: unsigned _BitInt(N) -> signed _BitInt(N)
 * cast must sign-extend bit N-1 (arithmetic fill), so the value reads
 * negative and signed div/mod/compare are correct. Oracle: Python bignum
 * (tests/bitint/gen_scast.py). Self-checking: main returns the number of
 * failed checks (0 == pass) and prints a summary line the ctest matches. */
#include <stdio.h>

static int fails = 0;
static int checks = 0;
#define CK(cond, tag) do { checks++; if (!(cond)) { fails++; \
    printf("FAIL %s\n", tag); } } while (0)

/* MCC_SCAST_MUTATE: corrupt one expected value so a *correct* compiler
 * reports a failure -- proves the harness is non-vacuous (T-lin-10003). */
#ifdef MCC_SCAST_MUTATE
#define WANT_SIGN 0
#else
#define WANT_SIGN 1
#endif

int main(void) {
  {
    unsigned _BitInt(129) u = (unsigned _BitInt(129))0x123456789abcdefULL;
    u |= (unsigned _BitInt(129))1 << 128;
    signed _BitInt(129) s = (signed _BitInt(129))u;
    signed _BitInt(129) q = s / (signed _BitInt(129))7;
    signed _BitInt(129) r = s % (signed _BitInt(129))7;
    CK((s < 0) == WANT_SIGN, "N129 sign");
    CK((unsigned long long)q == 0xb70509ea383d1d6cULL, "N129 qlo");
    CK((long long)r == -5LL, "N129 rmod");
    CK(s < (signed _BitInt(129))500, "N129 cmp");
    unsigned _BitInt(129) v = u >> 1;
    CK((signed _BitInt(129))v >= 0, "N129 ushr-logical");
  }
  {
    unsigned _BitInt(130) u = (unsigned _BitInt(130))0x123456789abcdefULL;
    u |= (unsigned _BitInt(130))1 << 129;
    signed _BitInt(130) s = (signed _BitInt(130))u;
    signed _BitInt(130) q = s / (signed _BitInt(130))7;
    signed _BitInt(130) r = s % (signed _BitInt(130))7;
    CK((s < 0) == WANT_SIGN, "N130 sign");
    CK((unsigned long long)q == 0x6de077a113aad447ULL, "N130 qlo");
    CK((long long)r == -2LL, "N130 rmod");
    CK(s < (signed _BitInt(130))500, "N130 cmp");
    unsigned _BitInt(130) v = u >> 1;
    CK((signed _BitInt(130))v >= 0, "N130 ushr-logical");
  }
  {
    unsigned _BitInt(160) u = (unsigned _BitInt(160))0x123456789abcdefULL;
    u |= (unsigned _BitInt(160))1 << 159;
    signed _BitInt(160) s = (signed _BitInt(160))u;
    signed _BitInt(160) q = s / (signed _BitInt(160))7;
    signed _BitInt(160) r = s % (signed _BitInt(160))7;
    CK((s < 0) == WANT_SIGN, "N160 sign");
    CK((unsigned long long)q == 0x6de077a113aad447ULL, "N160 qlo");
    CK((long long)r == -2LL, "N160 rmod");
    CK(s < (signed _BitInt(160))500, "N160 cmp");
    unsigned _BitInt(160) v = u >> 1;
    CK((signed _BitInt(160))v >= 0, "N160 ushr-logical");
  }
  {
    unsigned _BitInt(192) u = (unsigned _BitInt(192))0x123456789abcdefULL;
    u |= (unsigned _BitInt(192))1 << 191;
    signed _BitInt(192) s = (signed _BitInt(192))u;
    signed _BitInt(192) q = s / (signed _BitInt(192))7;
    signed _BitInt(192) r = s % (signed _BitInt(192))7;
    CK((s < 0) == WANT_SIGN, "N192 sign");
    CK((unsigned long long)q == 0xb70509ea383d1d6cULL, "N192 qlo");
    CK((long long)r == -5LL, "N192 rmod");
    CK(s < (signed _BitInt(192))500, "N192 cmp");
    unsigned _BitInt(192) v = u >> 1;
    CK((signed _BitInt(192))v >= 0, "N192 ushr-logical");
  }
  {
    unsigned _BitInt(200) u = (unsigned _BitInt(200))0x123456789abcdefULL;
    u |= (unsigned _BitInt(200))1 << 199;
    signed _BitInt(200) s = (signed _BitInt(200))u;
    signed _BitInt(200) q = s / (signed _BitInt(200))7;
    signed _BitInt(200) r = s % (signed _BitInt(200))7;
    CK((s < 0) == WANT_SIGN, "N200 sign");
    CK((unsigned long long)q == 0xdb97530eca8641feULL, "N200 qlo");
    CK((long long)r == -3LL, "N200 rmod");
    CK(s < (signed _BitInt(200))500, "N200 cmp");
    unsigned _BitInt(200) v = u >> 1;
    CK((signed _BitInt(200))v >= 0, "N200 ushr-logical");
  }
  {
    unsigned _BitInt(255) u = (unsigned _BitInt(255))0x123456789abcdefULL;
    u |= (unsigned _BitInt(255))1 << 254;
    signed _BitInt(255) s = (signed _BitInt(255))u;
    signed _BitInt(255) q = s / (signed _BitInt(255))7;
    signed _BitInt(255) r = s % (signed _BitInt(255))7;
    CK((s < 0) == WANT_SIGN, "N255 sign");
    CK((unsigned long long)q == 0xb70509ea383d1d6cULL, "N255 qlo");
    CK((long long)r == -5LL, "N255 rmod");
    CK(s < (signed _BitInt(255))500, "N255 cmp");
    unsigned _BitInt(255) v = u >> 1;
    CK((signed _BitInt(255))v >= 0, "N255 ushr-logical");
  }
  {
    unsigned _BitInt(257) u = (unsigned _BitInt(257))0x123456789abcdefULL;
    u |= (unsigned _BitInt(257))1 << 256;
    signed _BitInt(257) s = (signed _BitInt(257))u;
    signed _BitInt(257) q = s / (signed _BitInt(257))7;
    signed _BitInt(257) r = s % (signed _BitInt(257))7;
    CK((s < 0) == WANT_SIGN, "N257 sign");
    CK((unsigned long long)q == 0xdb97530eca8641feULL, "N257 qlo");
    CK((long long)r == -3LL, "N257 rmod");
    CK(s < (signed _BitInt(257))500, "N257 cmp");
    unsigned _BitInt(257) v = u >> 1;
    CK((signed _BitInt(257))v >= 0, "N257 ushr-logical");
  }
  {
    unsigned _BitInt(300) u = (unsigned _BitInt(300))0x123456789abcdefULL;
    u |= (unsigned _BitInt(300))1 << 299;
    signed _BitInt(300) s = (signed _BitInt(300))u;
    signed _BitInt(300) q = s / (signed _BitInt(300))7;
    signed _BitInt(300) r = s % (signed _BitInt(300))7;
    CK((s < 0) == WANT_SIGN, "N300 sign");
    CK((unsigned long long)q == 0xb70509ea383d1d6cULL, "N300 qlo");
    CK((long long)r == -5LL, "N300 rmod");
    CK(s < (signed _BitInt(300))500, "N300 cmp");
    unsigned _BitInt(300) v = u >> 1;
    CK((signed _BitInt(300))v >= 0, "N300 ushr-logical");
  }
  {
    unsigned _BitInt(320) u = (unsigned _BitInt(320))0x123456789abcdefULL;
    u |= (unsigned _BitInt(320))1 << 319;
    signed _BitInt(320) s = (signed _BitInt(320))u;
    signed _BitInt(320) q = s / (signed _BitInt(320))7;
    signed _BitInt(320) r = s % (signed _BitInt(320))7;
    CK((s < 0) == WANT_SIGN, "N320 sign");
    CK((unsigned long long)q == 0xdb97530eca8641feULL, "N320 qlo");
    CK((long long)r == -3LL, "N320 rmod");
    CK(s < (signed _BitInt(320))500, "N320 cmp");
    unsigned _BitInt(320) v = u >> 1;
    CK((signed _BitInt(320))v >= 0, "N320 ushr-logical");
  }
  {
    unsigned _BitInt(384) u = (unsigned _BitInt(384))0x123456789abcdefULL;
    u |= (unsigned _BitInt(384))1 << 383;
    signed _BitInt(384) s = (signed _BitInt(384))u;
    signed _BitInt(384) q = s / (signed _BitInt(384))7;
    signed _BitInt(384) r = s % (signed _BitInt(384))7;
    CK((s < 0) == WANT_SIGN, "N384 sign");
    CK((unsigned long long)q == 0xb70509ea383d1d6cULL, "N384 qlo");
    CK((long long)r == -5LL, "N384 rmod");
    CK(s < (signed _BitInt(384))500, "N384 cmp");
    unsigned _BitInt(384) v = u >> 1;
    CK((signed _BitInt(384))v >= 0, "N384 ushr-logical");
  }
  {
    unsigned _BitInt(400) u = (unsigned _BitInt(400))0x123456789abcdefULL;
    u |= (unsigned _BitInt(400))1 << 399;
    signed _BitInt(400) s = (signed _BitInt(400))u;
    signed _BitInt(400) q = s / (signed _BitInt(400))7;
    signed _BitInt(400) r = s % (signed _BitInt(400))7;
    CK((s < 0) == WANT_SIGN, "N400 sign");
    CK((unsigned long long)q == 0x6de077a113aad447ULL, "N400 qlo");
    CK((long long)r == -2LL, "N400 rmod");
    CK(s < (signed _BitInt(400))500, "N400 cmp");
    unsigned _BitInt(400) v = u >> 1;
    CK((signed _BitInt(400))v >= 0, "N400 ushr-logical");
  }
  {
    unsigned _BitInt(448) u = (unsigned _BitInt(448))0x123456789abcdefULL;
    u |= (unsigned _BitInt(448))1 << 447;
    signed _BitInt(448) s = (signed _BitInt(448))u;
    signed _BitInt(448) q = s / (signed _BitInt(448))7;
    signed _BitInt(448) r = s % (signed _BitInt(448))7;
    CK((s < 0) == WANT_SIGN, "N448 sign");
    CK((unsigned long long)q == 0x6de077a113aad447ULL, "N448 qlo");
    CK((long long)r == -2LL, "N448 rmod");
    CK(s < (signed _BitInt(448))500, "N448 cmp");
    unsigned _BitInt(448) v = u >> 1;
    CK((signed _BitInt(448))v >= 0, "N448 ushr-logical");
  }
  {
    unsigned _BitInt(500) u = (unsigned _BitInt(500))0x123456789abcdefULL;
    u |= (unsigned _BitInt(500))1 << 499;
    signed _BitInt(500) s = (signed _BitInt(500))u;
    signed _BitInt(500) q = s / (signed _BitInt(500))7;
    signed _BitInt(500) r = s % (signed _BitInt(500))7;
    CK((s < 0) == WANT_SIGN, "N500 sign");
    CK((unsigned long long)q == 0xdb97530eca8641feULL, "N500 qlo");
    CK((long long)r == -3LL, "N500 rmod");
    CK(s < (signed _BitInt(500))500, "N500 cmp");
    unsigned _BitInt(500) v = u >> 1;
    CK((signed _BitInt(500))v >= 0, "N500 ushr-logical");
  }
  {
    unsigned _BitInt(511) u = (unsigned _BitInt(511))0x123456789abcdefULL;
    u |= (unsigned _BitInt(511))1 << 510;
    signed _BitInt(511) s = (signed _BitInt(511))u;
    signed _BitInt(511) q = s / (signed _BitInt(511))7;
    signed _BitInt(511) r = s % (signed _BitInt(511))7;
    CK((s < 0) == WANT_SIGN, "N511 sign");
    CK((unsigned long long)q == 0x6de077a113aad447ULL, "N511 qlo");
    CK((long long)r == -2LL, "N511 rmod");
    CK(s < (signed _BitInt(511))500, "N511 cmp");
    unsigned _BitInt(511) v = u >> 1;
    CK((signed _BitInt(511))v >= 0, "N511 ushr-logical");
  }
  {
    unsigned _BitInt(256) u = (unsigned _BitInt(256))0x123456789abcdefULL;
    u |= (unsigned _BitInt(256))1 << 255;
    signed _BitInt(256) s = (signed _BitInt(256))u;
    signed _BitInt(256) q = s / (signed _BitInt(256))7;
    signed _BitInt(256) r = s % (signed _BitInt(256))7;
    CK((s < 0) == WANT_SIGN, "N256 sign");
    CK((unsigned long long)q == 0x6de077a113aad447ULL, "N256 qlo");
    CK((long long)r == -2LL, "N256 rmod");
    CK(s < (signed _BitInt(256))500, "N256 cmp");
    unsigned _BitInt(256) v = u >> 1;
    CK((signed _BitInt(256))v >= 0, "N256 ushr-logical");
  }
  {
    unsigned _BitInt(512) u = (unsigned _BitInt(512))0x123456789abcdefULL;
    u |= (unsigned _BitInt(512))1 << 511;
    signed _BitInt(512) s = (signed _BitInt(512))u;
    signed _BitInt(512) q = s / (signed _BitInt(512))7;
    signed _BitInt(512) r = s % (signed _BitInt(512))7;
    CK((s < 0) == WANT_SIGN, "N512 sign");
    CK((unsigned long long)q == 0xdb97530eca8641feULL, "N512 qlo");
    CK((long long)r == -3LL, "N512 rmod");
    CK(s < (signed _BitInt(512))500, "N512 cmp");
    unsigned _BitInt(512) v = u >> 1;
    CK((signed _BitInt(512))v >= 0, "N512 ushr-logical");
  }
  printf("bitint-scast checks=%d fails=%d\n", checks, fails);
  return fails;
}
