typedef struct {
	unsigned long long low;
	unsigned long long high;
} mcc_int128;

#define MCC_INT128_SIGN_BIT 0x8000000000000000ULL
#define MCC_INT128_ALL_ONES 0xFFFFFFFFFFFFFFFFULL
#define MCC_INT128_HALF_BITS 64

static const double mcc_two_to_the_64 = 18446744073709551616.0;
static const long double mcc_two_to_the_64_long = 18446744073709551616.0L;

void abort(void);

static void mcc_int128_overflow(void) {
	abort();
}

static mcc_int128 int128_from_halves(unsigned long long high, unsigned long long low) {
	mcc_int128 value;
	value.low = low;
	value.high = high;
	return value;
}

static int int128_is_negative(mcc_int128 value) {
	return (value.high & MCC_INT128_SIGN_BIT) != 0ULL;
}

static mcc_int128 int128_negate(mcc_int128 value) {
	mcc_int128 result;
	result.low = ~value.low + 1ULL;
	result.high = ~value.high + (result.low == 0ULL ? 1ULL : 0ULL);
	return result;
}

static mcc_int128 int128_add(mcc_int128 a, mcc_int128 b) {
	mcc_int128 result;
	result.low = a.low + b.low;
	result.high = a.high + b.high + (result.low < a.low ? 1ULL : 0ULL);
	return result;
}

static mcc_int128 int128_subtract(mcc_int128 a, mcc_int128 b) {
	mcc_int128 result;
	result.low = a.low - b.low;
	result.high = a.high - b.high - (a.low < b.low ? 1ULL : 0ULL);
	return result;
}

static int int128_is_below(mcc_int128 a, mcc_int128 b) {
	if (a.high != b.high)
		return a.high < b.high;
	return a.low < b.low;
}

static int int128_is_equal(mcc_int128 a, mcc_int128 b) {
	return a.low == b.low && a.high == b.high;
}

static int int128_is_zero(mcc_int128 value) {
	return value.low == 0ULL && value.high == 0ULL;
}

static int int128_is_all_ones(mcc_int128 value) {
	return value.low == MCC_INT128_ALL_ONES && value.high == MCC_INT128_ALL_ONES;
}

static int int128_is_minimum(mcc_int128 value) {
	return value.low == 0ULL && value.high == MCC_INT128_SIGN_BIT;
}

static mcc_int128 int128_double(mcc_int128 value) {
	mcc_int128 result;
	result.high = (value.high << 1) | (value.low >> 63);
	result.low = value.low << 1;
	return result;
}

static unsigned long long int128_bit_at(mcc_int128 value, int index) {
	if (index >= MCC_INT128_HALF_BITS)
		return (value.high >> (index - MCC_INT128_HALF_BITS)) & 1ULL;
	return (value.low >> index) & 1ULL;
}

static int half_leading_zeros(unsigned long long half) {
	int count = 0;
	if (half == 0ULL)
		return MCC_INT128_HALF_BITS;
	if ((half >> 32) == 0ULL) {
		count += 32;
		half <<= 32;
	}
	if ((half >> 48) == 0ULL) {
		count += 16;
		half <<= 16;
	}
	if ((half >> 56) == 0ULL) {
		count += 8;
		half <<= 8;
	}
	if ((half >> 60) == 0ULL) {
		count += 4;
		half <<= 4;
	}
	if ((half >> 62) == 0ULL) {
		count += 2;
		half <<= 2;
	}
	if ((half >> 63) == 0ULL)
		count += 1;
	return count;
}

static unsigned long long half_divide_wide(unsigned long long high, unsigned long long low,
																					 unsigned long long divisor,
																					 unsigned long long *remainder) {
	unsigned long long limb_base = 1ULL << 32;
	int shift = half_leading_zeros(divisor);
	unsigned long long divisor_upper, divisor_lower;
	unsigned long long shifted_high, shifted_low, low_upper, low_lower;
	unsigned long long quotient_upper, quotient_lower, estimate_rest, partial;
	divisor <<= shift;
	divisor_upper = divisor >> 32;
	divisor_lower = divisor & 0xFFFFFFFFULL;
	if (shift == 0) {
		shifted_high = high;
		shifted_low = low;
	} else {
		shifted_high = (high << shift) | (low >> (MCC_INT128_HALF_BITS - shift));
		shifted_low = low << shift;
	}
	low_upper = shifted_low >> 32;
	low_lower = shifted_low & 0xFFFFFFFFULL;
	quotient_upper = shifted_high / divisor_upper;
	estimate_rest = shifted_high % divisor_upper;
	while (quotient_upper >= limb_base ||
				 quotient_upper * divisor_lower > (estimate_rest << 32) + low_upper) {
		quotient_upper--;
		estimate_rest += divisor_upper;
		if (estimate_rest >= limb_base)
			break;
	}
	partial = shifted_high * limb_base + low_upper - quotient_upper * divisor;
	quotient_lower = partial / divisor_upper;
	estimate_rest = partial % divisor_upper;
	while (quotient_lower >= limb_base ||
				 quotient_lower * divisor_lower > (estimate_rest << 32) + low_lower) {
		quotient_lower--;
		estimate_rest += divisor_upper;
		if (estimate_rest >= limb_base)
			break;
	}
	*remainder = (partial * limb_base + low_lower - quotient_lower * divisor) >> shift;
	return quotient_upper * limb_base + quotient_lower;
}

static void int128_unsigned_divide(mcc_int128 numerator, mcc_int128 denominator,
																	 mcc_int128 *quotient, mcc_int128 *remainder) {
	mcc_int128 partial_quotient = int128_from_halves(0ULL, 0ULL);
	mcc_int128 partial_remainder = int128_from_halves(0ULL, 0ULL);
	int index;
	if (denominator.high == 0ULL && denominator.low != 0ULL) {
		unsigned long long rest;
		if (numerator.high == 0ULL) {
			*quotient = int128_from_halves(0ULL, numerator.low / denominator.low);
			*remainder = int128_from_halves(0ULL, numerator.low % denominator.low);
			return;
		}
		if (numerator.high < denominator.low) {
			*quotient = int128_from_halves(
					0ULL, half_divide_wide(numerator.high, numerator.low, denominator.low, &rest));
		} else {
			unsigned long long upper = numerator.high / denominator.low;
			unsigned long long upper_rest = numerator.high % denominator.low;
			*quotient = int128_from_halves(
					upper, half_divide_wide(upper_rest, numerator.low, denominator.low, &rest));
		}
		*remainder = int128_from_halves(0ULL, rest);
		return;
	}
	for (index = 127; index >= 0; index--) {
		partial_remainder = int128_double(partial_remainder);
		partial_remainder.low |= int128_bit_at(numerator, index);
		if (!int128_is_below(partial_remainder, denominator)) {
			partial_remainder = int128_subtract(partial_remainder, denominator);
			if (index >= MCC_INT128_HALF_BITS)
				partial_quotient.high |= 1ULL << (index - MCC_INT128_HALF_BITS);
			else
				partial_quotient.low |= 1ULL << index;
		}
	}
	*quotient = partial_quotient;
	*remainder = partial_remainder;
}

static mcc_int128 int128_multiply_halves(unsigned long long a, unsigned long long b) {
	unsigned long long a_low = a & 0xFFFFFFFFULL;
	unsigned long long a_high = a >> 32;
	unsigned long long b_low = b & 0xFFFFFFFFULL;
	unsigned long long b_high = b >> 32;
	unsigned long long low_by_low = a_low * b_low;
	unsigned long long high_by_low = a_high * b_low;
	unsigned long long low_by_high = a_low * b_high;
	unsigned long long middle = (low_by_low >> 32) + (high_by_low & 0xFFFFFFFFULL) +
															(low_by_high & 0xFFFFFFFFULL);
	mcc_int128 result;
	result.low = (middle << 32) | (low_by_low & 0xFFFFFFFFULL);
	result.high = a_high * b_high + (high_by_low >> 32) + (low_by_high >> 32) + (middle >> 32);
	return result;
}

static unsigned long long half_sign_fill(unsigned long long half) {
	return (half & MCC_INT128_SIGN_BIT) != 0ULL ? MCC_INT128_ALL_ONES : 0ULL;
}

static unsigned long long half_shift_right_signed(unsigned long long half, int count) {
	if (count == 0)
		return half;
	return (half >> count) | (half_sign_fill(half) << (MCC_INT128_HALF_BITS - count));
}

static int half_trailing_zeros(unsigned long long half) {
	int count = 0;
	if (half == 0ULL)
		return MCC_INT128_HALF_BITS;
	if ((half & 0xFFFFFFFFULL) == 0ULL) {
		count += 32;
		half >>= 32;
	}
	if ((half & 0xFFFFULL) == 0ULL) {
		count += 16;
		half >>= 16;
	}
	if ((half & 0xFFULL) == 0ULL) {
		count += 8;
		half >>= 8;
	}
	if ((half & 0xFULL) == 0ULL) {
		count += 4;
		half >>= 4;
	}
	if ((half & 0x3ULL) == 0ULL) {
		count += 2;
		half >>= 2;
	}
	if ((half & 0x1ULL) == 0ULL)
		count += 1;
	return count;
}

static int half_population_count(unsigned long long half) {
	half = half - ((half >> 1) & 0x5555555555555555ULL);
	half = (half & 0x3333333333333333ULL) + ((half >> 2) & 0x3333333333333333ULL);
	half = (half + (half >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
	return (int)((half * 0x0101010101010101ULL) >> 56);
}

static unsigned long long half_byte_swap(unsigned long long half) {
	half = ((half & 0x00FF00FF00FF00FFULL) << 8) | ((half >> 8) & 0x00FF00FF00FF00FFULL);
	half = ((half & 0x0000FFFF0000FFFFULL) << 16) | ((half >> 16) & 0x0000FFFF0000FFFFULL);
	return (half << 32) | (half >> 32);
}

static int int128_leading_zeros(mcc_int128 value) {
	if (value.high != 0ULL)
		return half_leading_zeros(value.high);
	return MCC_INT128_HALF_BITS + half_leading_zeros(value.low);
}

static int int128_trailing_zeros(mcc_int128 value) {
	if (value.low != 0ULL)
		return half_trailing_zeros(value.low);
	return MCC_INT128_HALF_BITS + half_trailing_zeros(value.high);
}

static unsigned long long int128_top_half_after_shift(mcc_int128 value, int shift) {
	if (shift == 0)
		return value.low;
	if (shift >= MCC_INT128_HALF_BITS)
		return value.high >> (shift - MCC_INT128_HALF_BITS);
	return (value.low >> shift) | (value.high << (MCC_INT128_HALF_BITS - shift));
}

static int int128_has_bits_below(mcc_int128 value, int shift) {
	if (shift <= 0)
		return 0;
	if (shift >= MCC_INT128_HALF_BITS) {
		if (value.low != 0ULL)
			return 1;
		if (shift == MCC_INT128_HALF_BITS)
			return 0;
		return (value.high & ((1ULL << (shift - MCC_INT128_HALF_BITS)) - 1ULL)) != 0ULL;
	}
	return (value.low & ((1ULL << shift) - 1ULL)) != 0ULL;
}

static unsigned long long int128_significand_with_sticky(mcc_int128 value, int *scale_exponent) {
	int significant_bits = 128 - int128_leading_zeros(value);
	int shift = significant_bits - MCC_INT128_HALF_BITS;
	unsigned long long significand;
	if (shift <= 0) {
		*scale_exponent = 0;
		return value.low;
	}
	significand = int128_top_half_after_shift(value, shift);
	if (int128_has_bits_below(value, shift))
		significand |= 1ULL;
	*scale_exponent = shift;
	return significand;
}

static unsigned long long int128_significand_rounded(mcc_int128 value, int *scale_exponent) {
	int significant_bits = 128 - int128_leading_zeros(value);
	int shift = significant_bits - MCC_INT128_HALF_BITS;
	unsigned long long significand;
	int round_up;
	if (shift <= 0) {
		*scale_exponent = 0;
		return value.low;
	}
	significand = int128_top_half_after_shift(value, shift);
	round_up = int128_bit_at(value, shift - 1) != 0ULL &&
						 (int128_has_bits_below(value, shift - 1) || (significand & 1ULL) != 0ULL);
	if (round_up) {
		significand++;
		if (significand == 0ULL) {
			significand = MCC_INT128_SIGN_BIT;
			shift++;
		}
	}
	*scale_exponent = shift;
	return significand;
}

mcc_int128 __negti2(mcc_int128 value) {
	return int128_negate(value);
}

mcc_int128 __multi3(mcc_int128 a, mcc_int128 b) {
	mcc_int128 result = int128_multiply_halves(a.low, b.low);
	result.high += a.low * b.high + a.high * b.low;
	return result;
}

mcc_int128 __udivmodti4(mcc_int128 a, mcc_int128 b, mcc_int128 *remainder) {
	mcc_int128 quotient, rest;
	int128_unsigned_divide(a, b, &quotient, &rest);
	if (remainder != 0)
		*remainder = rest;
	return quotient;
}

mcc_int128 __udivti3(mcc_int128 a, mcc_int128 b) {
	mcc_int128 quotient, remainder;
	int128_unsigned_divide(a, b, &quotient, &remainder);
	return quotient;
}

mcc_int128 __umodti3(mcc_int128 a, mcc_int128 b) {
	mcc_int128 quotient, remainder;
	int128_unsigned_divide(a, b, &quotient, &remainder);
	return remainder;
}

mcc_int128 __divti3(mcc_int128 a, mcc_int128 b) {
	int result_is_negative = int128_is_negative(a) ^ int128_is_negative(b);
	mcc_int128 quotient, remainder;
	if (int128_is_negative(a))
		a = int128_negate(a);
	if (int128_is_negative(b))
		b = int128_negate(b);
	int128_unsigned_divide(a, b, &quotient, &remainder);
	return result_is_negative ? int128_negate(quotient) : quotient;
}

mcc_int128 __modti3(mcc_int128 a, mcc_int128 b) {
	int result_is_negative = int128_is_negative(a);
	mcc_int128 quotient, remainder;
	if (result_is_negative)
		a = int128_negate(a);
	if (int128_is_negative(b))
		b = int128_negate(b);
	int128_unsigned_divide(a, b, &quotient, &remainder);
	return result_is_negative ? int128_negate(remainder) : remainder;
}

mcc_int128 __ashlti3(mcc_int128 value, int count) {
	mcc_int128 result;
	count &= 127;
	if (count == 0)
		return value;
	if (count >= MCC_INT128_HALF_BITS) {
		result.high = value.low << (count - MCC_INT128_HALF_BITS);
		result.low = 0ULL;
	} else {
		result.high = (value.high << count) | (value.low >> (MCC_INT128_HALF_BITS - count));
		result.low = value.low << count;
	}
	return result;
}

mcc_int128 __lshrti3(mcc_int128 value, int count) {
	mcc_int128 result;
	count &= 127;
	if (count == 0)
		return value;
	if (count >= MCC_INT128_HALF_BITS) {
		result.low = value.high >> (count - MCC_INT128_HALF_BITS);
		result.high = 0ULL;
	} else {
		result.low = (value.low >> count) | (value.high << (MCC_INT128_HALF_BITS - count));
		result.high = value.high >> count;
	}
	return result;
}

mcc_int128 __ashrti3(mcc_int128 value, int count) {
	mcc_int128 result;
	count &= 127;
	if (count == 0)
		return value;
	if (count >= MCC_INT128_HALF_BITS) {
		result.low = half_shift_right_signed(value.high, count - MCC_INT128_HALF_BITS);
		result.high = half_sign_fill(value.high);
	} else {
		result.low = (value.low >> count) | (value.high << (MCC_INT128_HALF_BITS - count));
		result.high = half_shift_right_signed(value.high, count);
	}
	return result;
}

int __cmpti2(mcc_int128 a, mcc_int128 b) {
	unsigned long long a_key = a.high ^ MCC_INT128_SIGN_BIT;
	unsigned long long b_key = b.high ^ MCC_INT128_SIGN_BIT;
	if (a_key != b_key)
		return a_key < b_key ? 0 : 2;
	if (a.low != b.low)
		return a.low < b.low ? 0 : 2;
	return 1;
}

int __ucmpti2(mcc_int128 a, mcc_int128 b) {
	if (a.high != b.high)
		return a.high < b.high ? 0 : 2;
	if (a.low != b.low)
		return a.low < b.low ? 0 : 2;
	return 1;
}

int __clzti2(mcc_int128 value) {
	return int128_leading_zeros(value);
}

int __ctzti2(mcc_int128 value) {
	return int128_trailing_zeros(value);
}

int __ffsti2(mcc_int128 value) {
	if (value.low == 0ULL && value.high == 0ULL)
		return 0;
	return int128_trailing_zeros(value) + 1;
}

int __popcountti2(mcc_int128 value) {
	return half_population_count(value.low) + half_population_count(value.high);
}

int __parityti2(mcc_int128 value) {
	return (half_population_count(value.low) + half_population_count(value.high)) & 1;
}

mcc_int128 __bswapti2(mcc_int128 value) {
	return int128_from_halves(half_byte_swap(value.low), half_byte_swap(value.high));
}

double __floatuntidf(mcc_int128 value) {
	int scale_exponent;
	unsigned long long significand = int128_significand_with_sticky(value, &scale_exponent);
	double scale = 1.0;
	while (scale_exponent-- > 0)
		scale *= 2.0;
	return (double)significand * scale;
}

double __floattidf(mcc_int128 value) {
	if (int128_is_negative(value))
		return -__floatuntidf(int128_negate(value));
	return __floatuntidf(value);
}

float __floatuntisf(mcc_int128 value) {
	int scale_exponent;
	unsigned long long significand = int128_significand_with_sticky(value, &scale_exponent);
	float scale = 1.0f;
	while (scale_exponent-- > 0)
		scale *= 2.0f;
	return (float)significand * scale;
}

float __floattisf(mcc_int128 value) {
	if (int128_is_negative(value))
		return -__floatuntisf(int128_negate(value));
	return __floatuntisf(value);
}

mcc_int128 __fixunsdfti(double value) {
	mcc_int128 result = int128_from_halves(0ULL, 0ULL);
	if (!(value >= 1.0))
		return result;
	if (value >= mcc_two_to_the_64) {
		unsigned long long high = (unsigned long long)(value * (1.0 / mcc_two_to_the_64));
		result.high = high;
		result.low = (unsigned long long)(value - (double)high * mcc_two_to_the_64);
	} else {
		result.low = (unsigned long long)value;
	}
	return result;
}

mcc_int128 __fixdfti(double value) {
	if (value < 0.0)
		return int128_negate(__fixunsdfti(-value));
	return __fixunsdfti(value);
}

mcc_int128 __fixunssfti(float value) {
	return __fixunsdfti((double)value);
}

mcc_int128 __fixsfti(float value) {
	return __fixdfti((double)value);
}

long double __floatuntixf(mcc_int128 value) {
	int scale_exponent;
	unsigned long long significand = int128_significand_rounded(value, &scale_exponent);
	long double scale = 1.0L;
	while (scale_exponent-- > 0)
		scale *= 2.0L;
	return (long double)significand * scale;
}

long double __floattixf(mcc_int128 value) {
	if (int128_is_negative(value))
		return -__floatuntixf(int128_negate(value));
	return __floatuntixf(value);
}

mcc_int128 __fixunsxfti(long double value) {
	mcc_int128 result = int128_from_halves(0ULL, 0ULL);
	if (!(value >= 1.0L))
		return result;
	if (value >= mcc_two_to_the_64_long) {
		unsigned long long high =
				(unsigned long long)(value * (1.0L / mcc_two_to_the_64_long));
		result.high = high;
		result.low = (unsigned long long)(value - (long double)high * mcc_two_to_the_64_long);
	} else {
		result.low = (unsigned long long)value;
	}
	return result;
}

mcc_int128 __fixxfti(long double value) {
	if (value < 0.0L)
		return int128_negate(__fixunsxfti(-value));
	return __fixunsxfti(value);
}

static int __mcc_addo_ti_impl(mcc_int128 a, mcc_int128 b, mcc_int128 *r) {
	mcc_int128 sum = int128_add(a, b);
	*r = sum;
	return (~(a.high ^ b.high) & (a.high ^ sum.high) & MCC_INT128_SIGN_BIT) != 0ULL;
}

static int __mcc_subo_ti_impl(mcc_int128 a, mcc_int128 b, mcc_int128 *r) {
	mcc_int128 difference = int128_subtract(a, b);
	*r = difference;
	return ((a.high ^ b.high) & (a.high ^ difference.high) & MCC_INT128_SIGN_BIT) != 0ULL;
}

static int __mcc_mulo_ti_impl(mcc_int128 a, mcc_int128 b, mcc_int128 *r) {
	mcc_int128 product = __multi3(a, b);
	*r = product;
	if (int128_is_zero(a) || int128_is_zero(b))
		return 0;
	if (int128_is_all_ones(a))
		return int128_is_minimum(b);
	if (int128_is_all_ones(b))
		return int128_is_minimum(a);
	return !int128_is_equal(__divti3(product, a), b);
}

mcc_int128 __divmodti4(mcc_int128 a, mcc_int128 b, mcc_int128 *remainder) {
	int quotient_is_negative = int128_is_negative(a) ^ int128_is_negative(b);
	int remainder_is_negative = int128_is_negative(a);
	mcc_int128 quotient, rest;
	if (int128_is_negative(a))
		a = int128_negate(a);
	if (int128_is_negative(b))
		b = int128_negate(b);
	int128_unsigned_divide(a, b, &quotient, &rest);
	if (remainder != 0)
		*remainder = remainder_is_negative ? int128_negate(rest) : rest;
	return quotient_is_negative ? int128_negate(quotient) : quotient;
}

mcc_int128 __negvti2(mcc_int128 value) {
	if (int128_is_minimum(value))
		mcc_int128_overflow();
	return int128_negate(value);
}

mcc_int128 __absvti2(mcc_int128 value) {
	if (int128_is_negative(value))
		return __negvti2(value);
	return value;
}

mcc_int128 __addvti3(mcc_int128 a, mcc_int128 b) {
	mcc_int128 result;
	if (__mcc_addo_ti_impl(a, b, &result))
		mcc_int128_overflow();
	return result;
}

mcc_int128 __subvti3(mcc_int128 a, mcc_int128 b) {
	mcc_int128 result;
	if (__mcc_subo_ti_impl(a, b, &result))
		mcc_int128_overflow();
	return result;
}

mcc_int128 __mulvti3(mcc_int128 a, mcc_int128 b) {
	mcc_int128 result;
	if (__mcc_mulo_ti_impl(a, b, &result))
		mcc_int128_overflow();
	return result;
}
