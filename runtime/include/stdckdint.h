#ifndef _STDCKDINT_H
#define _STDCKDINT_H

#define __STDC_VERSION_STDCKDINT_H__ 202311L

/* T-mac-30156: delegate to the checked-overflow builtins, which are
 * __int128-free (mcc rejects the __int128 type on some targets, e.g.
 * aarch64-apple-darwin, so the header must not use it). Per C23 7.20 each
 * ckd_* computes the mathematically-exact result of a (+|-|*) b, stores the
 * value wrapped into the type of *r, and returns whether it could not be
 * represented — which is exactly the corresponding __builtin_*_overflow. The
 * builtins evaluate a and b once and detect overflow against the type of *r,
 * covering every signed/unsigned and narrower-result combination. */
#define ckd_add(r, a, b) ((_Bool)__builtin_add_overflow((a), (b), (r)))
#define ckd_sub(r, a, b) ((_Bool)__builtin_sub_overflow((a), (b), (r)))
#define ckd_mul(r, a, b) ((_Bool)__builtin_mul_overflow((a), (b), (r)))

#endif
