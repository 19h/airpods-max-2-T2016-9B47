#include "rkof/cpu.h"
#include "rkof/libc.h"

/* @provenance RVA 0xd23c sub_D23C */
uint64_t sub_D23C(uint64_t val, uint32_t shift)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32u);
    uint32_t amount = shift & 0xFFu;

    if (amount == 0u) {
        return val;
    }
    if (amount < 32u) {
        hi = (hi << amount) | (lo >> (32u - amount));
        lo <<= amount;
    } else if (amount < 64u) {
        hi = lo << (amount - 32u);
        lo = 0u;
    } else {
        return 0ULL;
    }
    return ((uint64_t)hi << 32u) | (uint64_t)lo;
}

/* @provenance RVA 0xd9f8 sub_D9F8 */
uint64_t sub_D9F8(uint64_t val, uint32_t shift)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32u);
    uint32_t amount = shift & 0xFFu;

    if (amount == 0u) {
        return val;
    }
    if (amount < 32u) {
        lo = (lo >> amount) | (hi << (32u - amount));
        hi >>= amount;
    } else if (amount < 64u) {
        lo = hi >> (amount - 32u);
        hi = 0u;
    } else {
        return 0ULL;
    }
    return ((uint64_t)hi << 32u) | (uint64_t)lo;
}

/* @provenance RVA 0xd210 sub_D210 */
int32_t sub_D210(uint64_t *mantissa_io)
{
    uint64_t raw = *mantissa_io;
    uint32_t lo = (uint32_t)raw;
    uint32_t hi = (uint32_t)(raw >> 32u);
    uint32_t lz = (hi != 0u) ? cpu_clz32(hi) : (cpu_clz32(lo) + 32u);
    *mantissa_io = sub_D23C(raw, lz - 11u);
    return 12 - (int32_t)lz;
}

/* @provenance RVA 0xd5a4 sub_D5A4 */
int32_t sub_D5A4(uint64_t *mantissa_io)
{
    uint64_t raw = *mantissa_io;
    uint32_t lo = (uint32_t)raw;
    uint32_t hi = (uint32_t)(raw >> 32u);
    uint32_t lz = (hi != 0u) ? cpu_clz32(hi) : (cpu_clz32(lo) + 32u);
    *mantissa_io = sub_D23C(raw, lz - 11u);
    return 12 - (int32_t)lz;
}

/* @provenance RVA 0xdd76 sub_DD76 */
uint64_t sub_DD76(uint64_t num, uint64_t den, uint64_t *rem_out)
{
    if (den == 0u) {
        if (rem_out != NULL) {
            *rem_out = 0u;
        }
        return 0u;
    }

    if (den > num) {
        if (rem_out != NULL) {
            *rem_out = num;
        }
        return 0u;
    }

    uint32_t den_hi = (uint32_t)(den >> 32u);
    uint32_t den_lo = (uint32_t)den;
    uint32_t num_hi = (uint32_t)(num >> 32u);
    uint32_t num_lo = (uint32_t)num;

    uint32_t den_lz = (den_hi != 0u) ? cpu_clz32(den_hi) : (cpu_clz32(den_lo) + 32u);
    uint32_t num_lz = (num_hi != 0u) ? cpu_clz32(num_hi) : (cpu_clz32(num_lo) + 32u);
    uint32_t shift = den_lz - num_lz;

    uint64_t cur_den = sub_D23C(den, shift);
    uint64_t cur_bit = sub_D23C(1ULL, shift);
    uint64_t quot = 0u;
    uint64_t rem = num;

    for (uint32_t step = 0u; step <= shift; step++) {
        if (rem >= cur_den) {
            rem -= cur_den;
            quot |= cur_bit;
        }
        cur_den = sub_D9F8(cur_den, 1u);
        cur_bit = sub_D9F8(cur_bit, 1u);
    }

    if (rem_out != NULL) {
        *rem_out = rem;
    }
    return quot;
}

/* @provenance RVA 0xdd68 sub_DD68 */
uint64_t sub_DD68(uint64_t num, uint64_t den)
{
    return sub_DD76(num, den, NULL);
}

/* @provenance RVA 0xd5d0 sub_D5D0 */
int64_t sub_D5D0(int64_t num, int64_t den)
{
    uint32_t num_sign = (uint32_t)((uint64_t)num >> 63u);
    uint32_t den_sign = (uint32_t)((uint64_t)den >> 63u);

    uint64_t abs_num = num_sign ? (0ULL - (uint64_t)num) : (uint64_t)num;
    uint64_t abs_den = den_sign ? (0ULL - (uint64_t)den) : (uint64_t)den;

    uint64_t uquot = sub_DD76(abs_num, abs_den, NULL);
    if ((num_sign ^ den_sign) != 0u) {
        return (int64_t)(0ULL - uquot);
    }
    return (int64_t)uquot;
}

static inline uint32_t f32_to_bits(float val)
{
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = val;
    return conv.u;
}

static inline float bits_to_f32(uint32_t bits)
{
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = bits;
    return conv.f;
}

static inline uint64_t f64_to_bits(double val)
{
    union {
        double d;
        uint64_t u;
    } conv;
    conv.d = val;
    return conv.u;
}

static inline double bits_to_f64(uint64_t bits)
{
    union {
        uint64_t u;
        double d;
    } conv;
    conv.u = bits;
    return conv.d;
}

/* @provenance RVA 0xd610 sub_D610 */
double sub_D610(float val)
{
    uint32_t raw = f32_to_bits(val);
    uint32_t abs_bits = raw & 0x7FFFFFFFu;
    uint32_t sign_hi = raw & 0x80000000u;
    uint32_t out_lo = 0u;
    uint32_t out_hi = 0u;

    if (((abs_bits - 0x00800000u) >> 24u) <= 0x7Eu) {
        out_hi = 0x38000000u + (abs_bits >> 3u);
        out_lo = abs_bits << 29u;
    } else if (abs_bits >= 0x7F800000u) {
        uint32_t qnan_bit = raw & 0x00400000u;
        out_hi = 0x7FF00000u | (qnan_bit >> 3u);
        out_lo = raw & 0x003FFFFFu;
    } else if (abs_bits != 0u) {
        uint32_t lz = cpu_clz32(abs_bits);
        uint64_t shifted = sub_D23C((uint64_t)abs_bits, lz + 21u);
        out_lo = (uint32_t)shifted;
        out_hi = ((uint32_t)(shifted >> 32u) ^ 0x00100000u) | ((0x389u - lz) << 20u);
    }

    out_hi |= sign_hi;
    return bits_to_f64(((uint64_t)out_hi << 32u) | (uint64_t)out_lo);
}

/* @provenance RVA 0xd680 sub_D680 */
int32_t sub_D680(double val)
{
    uint64_t bits = f64_to_bits(val);
    uint32_t lo = (uint32_t)bits;
    uint32_t hi = (uint32_t)(bits >> 32u);
    uint32_t exp = (hi >> 20u) & 0x7FFu;
    int32_t sign_mul = ((hi & 0x80000000u) != 0u) ? -1 : 1;

    uint32_t mag;
    if ((exp - 0x3FFu) <= 0x33u) {
        uint32_t mant_hi = (hi & 0x000FFFFFu) | 0x00100000u;
        uint64_t mant = ((uint64_t)mant_hi << 32u) | (uint64_t)lo;
        mag = (uint32_t)sub_D9F8(mant, 0x433u - exp);
    } else {
        if (exp < 0x3FFu) {
            return 0;
        }
        uint64_t raw_pair = ((uint64_t)0x3FFu << 32u) | (uint64_t)lo;
        mag = (uint32_t)sub_D23C(raw_pair, exp - 0x433u);
    }

    return (int32_t)(mag * (uint32_t)sign_mul);
}

/* @provenance RVA 0xd6c2 sub_D6C2 */
int64_t sub_D6C2(float val)
{
    uint32_t bits = f32_to_bits(val);
    uint32_t exp = (bits >> 23u) & 0xFFu;
    if (exp < 0x7Fu) {
        return 0;
    }

    uint32_t neg = (bits >> 31u) & 1u;
    uint32_t mant = (bits & 0x007FFFFFu) | 0x00800000u;
    uint64_t uval = (exp >= 0x97u)
        ? sub_D23C((uint64_t)mant, exp - 0x96u)
        : sub_D9F8((uint64_t)mant, 0x96u - exp);

    return neg ? (int64_t)(0ULL - uval) : (int64_t)uval;
}

/* @provenance RVA 0xd704 sub_D704 */
uint32_t sub_D704(double val)
{
    uint64_t bits = f64_to_bits(val);
    uint32_t lo = (uint32_t)bits;
    uint32_t hi = (uint32_t)(bits >> 32u);
    if ((hi & 0x80000000u) != 0u) {
        return 0u;
    }
    uint32_t exp = (hi >> 20u) & 0x7FFu;
    if (exp < 0x3FFu) {
        return 0u;
    }
    uint32_t mant32 = (hi << 11u) | (lo >> 21u) | 0x80000000u;
    uint32_t rshift = (0x41Eu - exp) & 0xFFu;
    return (rshift >= 32u) ? 0u : (mant32 >> rshift);
}

/* @provenance RVA 0xd736 sub_D736 */
uint64_t sub_D736(float val)
{
    uint32_t bits = f32_to_bits(val);
    if ((bits & 0x80000000u) != 0u) {
        return 0u;
    }
    uint32_t exp = (bits >> 23u) & 0xFFu;
    if (exp < 0x7Fu) {
        return 0u;
    }
    uint32_t mant = (bits & 0x007FFFFFu) | 0x00800000u;
    if (exp >= 0x97u) {
        return sub_D23C((uint64_t)mant, exp - 0x96u);
    }
    return sub_D9F8((uint64_t)mant, 0x96u - exp);
}

/* @provenance RVA 0xd7bc sub_D7BC */
float sub_D7BC(int64_t val)
{
    if (val == 0) {
        return bits_to_f32(0u);
    }

    uint32_t sign_bit = (uint32_t)((uint64_t)val >> 63u) << 31u;
    uint64_t abs_val = (sign_bit != 0u) ? (0ULL - (uint64_t)val) : (uint64_t)val;
    uint32_t lo = (uint32_t)abs_val;
    uint32_t hi = (uint32_t)(abs_val >> 32u);
    uint32_t lz = (hi != 0u) ? cpu_clz32(hi) : (cpu_clz32(lo) + 32u);
    uint32_t exp_off = lz ^ 0x3Fu;
    uint32_t mant24;

    if (lz > 0x27u) {
        mant24 = (uint32_t)sub_D23C((uint64_t)lo, lz - 0x28u);
    } else {
        if (lz == 0x27u) {
            hi = (hi << 1u) | (lo >> 31u);
            lo <<= 1u;
        } else if (lz != 0x26u) {
            uint64_t tail = sub_D23C(abs_val, lz + 0x1Au);
            uint32_t sticky = (tail != 0u) ? 1u : 0u;
            uint64_t shifted = sub_D9F8(abs_val, 0x26u - lz);
            lo = (uint32_t)shifted | sticky;
            hi = (uint32_t)(shifted >> 32u);
        }

        uint32_t round_guard = ((lo >> 2u) & 1u) | lo;
        uint64_t rounded = (((uint64_t)hi << 32u) | (uint64_t)round_guard) + 1ULL;
        uint32_t r_lo = (uint32_t)rounded;
        uint32_t r_hi = (uint32_t)(rounded >> 32u);
        if (((r_lo << 5u) & 0x80000000u) == 0u) {
            mant24 = (r_lo >> 2u) | (r_hi << 30u);
        } else {
            exp_off = 0x40u - lz;
            mant24 = (r_lo >> 3u) | (r_hi << 29u);
        }
    }

    uint32_t fbits = (mant24 & 0x007FFFFFu) + (sign_bit | (exp_off << 23u)) + 0x3F800000u;
    return bits_to_f32(fbits);
}

/* Freestanding ARM EABI compiler-rt memory lowering adapters */
void __aeabi_memcpy(void *dst, const void *src, size_t len);
void __aeabi_memcpy4(void *dst, const void *src, size_t len);
void __aeabi_memcpy8(void *dst, const void *src, size_t len);
void __aeabi_memset(void *dst, size_t len, int byte_val);
void __aeabi_memset4(void *dst, size_t len, int byte_val);
void __aeabi_memset8(void *dst, size_t len, int byte_val);
void __aeabi_memclr(void *dst, size_t len);
void __aeabi_memclr4(void *dst, size_t len);
void __aeabi_memclr8(void *dst, size_t len);

void __aeabi_memcpy(void *dst, const void *src, size_t len)  { (void)rkof_memcpy(dst, src, len); }
void __aeabi_memcpy4(void *dst, const void *src, size_t len) { (void)rkof_memcpy(dst, src, len); }
void __aeabi_memcpy8(void *dst, const void *src, size_t len) { (void)rkof_memcpy(dst, src, len); }
void __aeabi_memset(void *dst, size_t len, int byte_val)     { (void)rkof_memset(dst, byte_val, len); }
void __aeabi_memset4(void *dst, size_t len, int byte_val)    { (void)rkof_memset(dst, byte_val, len); }
void __aeabi_memset8(void *dst, size_t len, int byte_val)    { (void)rkof_memset(dst, byte_val, len); }
void __aeabi_memclr(void *dst, size_t len)                   { (void)rkof_memset(dst, 0, len); }
void __aeabi_memclr4(void *dst, size_t len)                  { (void)rkof_memset(dst, 0, len); }
void __aeabi_memclr8(void *dst, size_t len)                  { (void)rkof_memset(dst, 0, len); }
