/* Optional C extension for Num::Dec.  Replaces arithmetic, comparison,
   rounding and predicate methods with TypedData-backed i128 operations.
   Pure Ruby fallback in lib/num/dec.rb provides the same interface.
   */

#include "ruby.h"

typedef __int128 i128;
typedef unsigned __int128 u128;

#define MASK64 ((u128)0xFFFFFFFFFFFFFFFFULL)

#ifndef RBOOL
#define RBOOL(v) ((v) ? Qtrue : Qfalse)
#endif

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

typedef struct {
    i128 value;
} dec_t;

/* Returns 0 because the struct is embedded in the Ruby object slot. */
static size_t
dec_memsize(const void *ptr)
{
    (void)ptr;
    return 0;
}

static const rb_data_type_t dec_type = {
    .wrap_struct_name = "Num::Dec",
    .function = {
        .dmark = NULL,
        .dfree = RUBY_TYPED_DEFAULT_FREE,
        .dsize = dec_memsize,
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED |
             RUBY_TYPED_EMBEDDABLE | RUBY_TYPED_FROZEN_SHAREABLE
};

static const i128 SCALE = (i128)1000000000000000000LL;
static VALUE rb_cDec;
static VALUE dec_zero; /* cached Dec(0) to avoid allocation */
static ID id_to_r, id_cmp, id_neg, id_floor, id_ceil, id_truncate,
         id_lt, id_le, id_gt, id_ge;

__attribute__((cold, noreturn)) static void
dec_overflow(void)
{
    rb_raise(rb_eRangeError, "Dec overflow");
}

static inline i128
dec_get(VALUE self)
{
    dec_t *d;
    TypedData_Get_Struct(self, dec_t, &dec_type, d);
    return d->value;
}

static inline VALUE
dec_wrap(i128 val)
{
    if (val == 0) return dec_zero;
    dec_t *d;
    VALUE obj = TypedData_Make_Struct(rb_cDec, dec_t, &dec_type, d);
    d->value = val;
    OBJ_FREEZE(obj);
    return obj;
}

static VALUE
i128_to_ruby(i128 val)
{
    if (val >= LONG_MIN && val <= LONG_MAX)
        return LONG2NUM((long)val);

    u128 v = (u128)val;
    uint64_t words[2] = { (uint64_t)(v & MASK64), (uint64_t)(v >> 64) };
    return rb_integer_unpack(words, 2, 8, 0,
                             INTEGER_PACK_LITTLE_ENDIAN | INTEGER_PACK_2COMP);
}

static void
wide_mul(u128 a, u128 b, u128 *lo, u128 *hi)
{
    u128 al = a & MASK64, ah = a >> 64;
    u128 bl = b & MASK64, bh = b >> 64;
    u128 ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;

    u128 mid = lh + hl;
    int carry_mid = mid < lh;

    *lo = ll + (mid << 64);
    int carry_lo = *lo < ll;

    *hi = hh + (mid >> 64) + ((u128)carry_mid << 64) + (u128)carry_lo;
}

static int
wide_div(u128 hi, u128 lo, u128 d, u128 *result)
{
    if (d == 0) return -1;
    if (hi == 0) { *result = lo / d; return 0; }
    if (hi >= d) return -1;

    if (d <= MASK64) {
        u128 r = hi;
        u128 mid = (r << 64) | (lo >> 64);
        u128 q1 = mid / d;
        u128 r1 = mid % d;
        u128 tail = (r1 << 64) | (lo & MASK64);
        *result = (q1 << 64) | (tail / d);
        return 0;
    }

    u128 r = hi, q = 0;
    for (int i = 127; i >= 0; i--) {
        r = (r << 1) | ((lo >> i) & 1);
        if (r >= d) { r -= d; q |= (u128)1 << i; }
    }
    *result = q;
    return 0;
}

/* |a| * factor / divisor, signed by a ^ b.  Sets *overflow on failure. */
static i128
i128_wide_op(i128 a, i128 b, u128 factor, u128 divisor, int *overflow)
{
    int neg = (a < 0) ^ (b < 0);
    u128 aa = a < 0 ? -(u128)a : (u128)a;
    u128 lo, hi, q;
    wide_mul(aa, factor, &lo, &hi);
    if (wide_div(hi, lo, divisor, &q) != 0) { *overflow = 1; return 0; }
    u128 limit = neg ? ((u128)1 << 127) : (((u128)1 << 127) - 1);
    if (q > limit) { *overflow = 1; return 0; }
    return neg ? -(i128)q : (i128)q;
}

/* Scaled multiply staying in i128.
   Fast path when both operands fit in u64: product fits in u128,
   avoids 256-bit wide_mul + wide_div entirely. */
static i128
i128_mul_scaled(i128 a, i128 b, int *overflow)
{
    if (a == 0 || b == 0) return 0;
    if (a == SCALE) return b;
    if (b == SCALE) return a;
    int neg = (a < 0) ^ (b < 0);
    u128 aa = a < 0 ? -(u128)a : (u128)a;
    u128 bb = b < 0 ? -(u128)b : (u128)b;
    if (aa <= UINT64_MAX && bb <= UINT64_MAX) {
        u128 product = (u128)(uint64_t)aa * (u128)(uint64_t)bb;
        u128 q = product / (u128)SCALE;
        u128 limit = neg ? ((u128)1 << 127) : (((u128)1 << 127) - 1);
        if (UNLIKELY(q > limit)) { *overflow = 1; return 0; }
        return neg ? -(i128)q : (i128)q;
    }
    return i128_wide_op(a, b, bb, (u128)SCALE, overflow);
}

/* Scaled divide staying in i128.
   Fast path when |a| fits in u64: |a| * SCALE fits in u128,
   avoids 256-bit widening. */
static i128
i128_div_scaled(i128 a, i128 b, int *overflow)
{
    if (b == 0) { *overflow = 1; return 0; }
    if (a == 0) return 0;
    int neg = (a < 0) ^ (b < 0);
    u128 aa = a < 0 ? -(u128)a : (u128)a;
    u128 bb = b < 0 ? -(u128)b : (u128)b;
    if (aa <= UINT64_MAX) {
        u128 numerator = (u128)(uint64_t)aa * (u128)SCALE;
        u128 q = numerator / bb;
        u128 limit = neg ? ((u128)1 << 127) : (((u128)1 << 127) - 1);
        if (UNLIKELY(q > limit)) { *overflow = 1; return 0; }
        return neg ? -(i128)q : (i128)q;
    }
    return i128_wide_op(a, b, (u128)SCALE, bb, overflow);
}

static i128
ruby_int_to_i128(VALUE integer)
{
    if (FIXNUM_P(integer))
        return (i128)FIX2LONG(integer);
    uint64_t words[2];
    int ret = rb_integer_pack(integer, words, 2, 8, 0,
                              INTEGER_PACK_LITTLE_ENDIAN | INTEGER_PACK_2COMP);
    if (UNLIKELY(ret == 2 || ret == -2))
        dec_overflow();
    return (i128)((u128)words[1] << 64 | words[0]);
}

static VALUE
dec_raw(VALUE self)
{
    return i128_to_ruby(dec_get(self));
}

static VALUE
dec_s_wrap(VALUE klass, VALUE raw_int)
{
    (void)klass;
    return dec_wrap(ruby_int_to_i128(raw_int));
}

static VALUE
dec_from_integer(VALUE klass, VALUE integer)
{
    (void)klass;
    i128 val = ruby_int_to_i128(integer), result;
    if (UNLIKELY(__builtin_mul_overflow(val, SCALE, &result)))
        dec_overflow();
    return dec_wrap(result);
}

static VALUE
dec_dump(VALUE self, VALUE level)
{
    (void)level;
    return rb_funcall(i128_to_ruby(dec_get(self)), rb_intern_const("to_s"), 0);
}

static VALUE
dec_s_load(VALUE klass, VALUE str)
{
    (void)klass;
    VALUE raw = rb_funcall(str, rb_intern_const("to_i"), 0);
    return dec_wrap(ruby_int_to_i128(raw));
}

static VALUE
dec_add(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other), r;
    if (UNLIKELY(__builtin_add_overflow(a, b, &r)))
        dec_overflow();
    return dec_wrap(r);
}

static VALUE
dec_sub(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other), r;
    if (UNLIKELY(__builtin_sub_overflow(a, b, &r)))
        dec_overflow();
    return dec_wrap(r);
}

static VALUE
dec_neg(VALUE self)
{
    i128 a = dec_get(self), r;
    if (UNLIKELY(__builtin_sub_overflow((i128)0, a, &r)))
        dec_overflow();
    return dec_wrap(r);
}

static VALUE
dec_mul(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other);
    if (a == 0 || b == 0) return dec_wrap(0);
    if (a == SCALE) return other;
    if (b == SCALE) return self;
    int overflow = 0;
    i128 r = i128_mul_scaled(a, b, &overflow);
    if (UNLIKELY(overflow)) dec_overflow();
    return dec_wrap(r);
}

static VALUE
dec_div_op(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other);
    if (UNLIKELY(b == 0))
        rb_raise(rb_eZeroDivError, "Dec division by zero");
    if (a == 0) return dec_wrap(0);
    if (b == SCALE) return self;
    int overflow = 0;
    i128 r = i128_div_scaled(a, b, &overflow);
    if (UNLIKELY(overflow)) dec_overflow();
    return dec_wrap(r);
}

static VALUE
dec_mod(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other);
    if (UNLIKELY(b == 0))
        rb_raise(rb_eZeroDivError, "Dec modulo by zero");
    if (b == 1 || b == -1) return dec_wrap(0);
    i128 r = a % b;
    if (r != 0 && ((r ^ b) < 0)) r += b;
    return dec_wrap(r);
}

static VALUE
dec_divmod(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other);
    if (UNLIKELY(b == 0))
        rb_raise(rb_eZeroDivError, "Dec divmod by zero");
    if (b == -1) {
        VALUE q = rb_funcall(i128_to_ruby(a), id_neg, 0);
        VALUE pair[2] = { q, dec_wrap(0) };
        return rb_ary_new_from_values(2, pair);
    }
    i128 q = a / b, r = a % b;
    if (r != 0 && ((r ^ b) < 0)) { q--; r += b; }
    VALUE pair[2] = { i128_to_ruby(q), dec_wrap(r) };
    return rb_ary_new_from_values(2, pair);
}

static VALUE
dec_div_int(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other);
    if (UNLIKELY(b == 0))
        rb_raise(rb_eZeroDivError, "Dec div by zero");
    if (b == -1)
        return rb_funcall(i128_to_ruby(a), id_neg, 0);
    i128 q = a / b, r = a % b;
    if (r != 0 && ((r ^ b) < 0)) q--;
    return i128_to_ruby(q);
}

/* Exponentiation by squaring, entirely in i128 until the final wrap. */
static VALUE
dec_pow(VALUE self, VALUE exp_val)
{
    if (UNLIKELY(!FIXNUM_P(exp_val) && !RB_TYPE_P(exp_val, T_BIGNUM)))
        rb_raise(rb_eTypeError, "Num::Dec#** supports only Integer exponents");

    long exp = NUM2LONG(exp_val);
    int neg = exp < 0;
    if (neg) {
        if (UNLIKELY(exp == LONG_MIN)) dec_overflow();
        exp = -exp;
    }
    if (exp == 0) return dec_wrap(SCALE);
    if (!neg && exp == 1) return self;

    int overflow = 0;
    i128 base = dec_get(self), result = SCALE;
    while (exp > 0) {
        if (exp & 1) {
            result = i128_mul_scaled(result, base, &overflow);
            if (UNLIKELY(overflow)) dec_overflow();
        }
        exp >>= 1;
        if (exp > 0) {
            base = i128_mul_scaled(base, base, &overflow);
            if (UNLIKELY(overflow)) dec_overflow();
        }
    }
    if (neg) {
        if (UNLIKELY(result == 0))
            rb_raise(rb_eZeroDivError, "Dec division by zero");
        result = i128_div_scaled(SCALE, result, &overflow);
        if (UNLIKELY(overflow)) dec_overflow();
    }
    return dec_wrap(result);
}

static VALUE
dec_cmp(VALUE self, VALUE other)
{
    if (rb_obj_is_kind_of(other, rb_cDec)) {
        i128 a = dec_get(self), b = dec_get(other);
        if (a < b) return INT2FIX(-1);
        if (a > b) return INT2FIX(1);
        return INT2FIX(0);
    }
    if (rb_obj_is_kind_of(other, rb_cNumeric)) {
        VALUE self_r = rb_funcall(self, id_to_r, 0);
        return rb_funcall(self_r, id_cmp, 1, other);
    }
    return Qnil;
}

static VALUE
dec_lt(VALUE self, VALUE other)
{
    if (UNLIKELY(!rb_obj_is_kind_of(other, rb_cDec)))
        return rb_num_coerce_relop(self, other, id_lt);
    return RBOOL(dec_get(self) < dec_get(other));
}

static VALUE
dec_le(VALUE self, VALUE other)
{
    if (UNLIKELY(!rb_obj_is_kind_of(other, rb_cDec)))
        return rb_num_coerce_relop(self, other, id_le);
    return RBOOL(dec_get(self) <= dec_get(other));
}

static VALUE
dec_gt(VALUE self, VALUE other)
{
    if (UNLIKELY(!rb_obj_is_kind_of(other, rb_cDec)))
        return rb_num_coerce_relop(self, other, id_gt);
    return RBOOL(dec_get(self) > dec_get(other));
}

static VALUE
dec_ge(VALUE self, VALUE other)
{
    if (UNLIKELY(!rb_obj_is_kind_of(other, rb_cDec)))
        return rb_num_coerce_relop(self, other, id_ge);
    return RBOOL(dec_get(self) >= dec_get(other));
}

static VALUE
dec_hash(VALUE self)
{
    i128 v = dec_get(self);
    st_index_t h = rb_hash_start((st_index_t)((u128)v & MASK64));
    h = rb_hash_uint(h, (st_index_t)((u128)v >> 64));
    h = rb_hash_end(h);
    return ST2FIX(h);
}

static VALUE
dec_eql(VALUE self, VALUE other)
{
    if (!rb_obj_is_kind_of(other, rb_cDec)) return Qfalse;
    return RBOOL(dec_get(self) == dec_get(other));
}

static VALUE
dec_zero_p(VALUE self)
{
    return RBOOL(dec_get(self) == 0);
}

static VALUE
dec_positive_p(VALUE self)
{
    return RBOOL(dec_get(self) > 0);
}

static VALUE
dec_negative_p(VALUE self)
{
    return RBOOL(dec_get(self) < 0);
}

static VALUE
dec_abs(VALUE self)
{
    i128 a = dec_get(self), r;
    if (a >= 0) return self;
    if (UNLIKELY(__builtin_sub_overflow((i128)0, a, &r)))
        dec_overflow();
    return dec_wrap(r);
}

static VALUE
dec_abs_diff(VALUE self, VALUE other)
{
    i128 a = dec_get(self), b = dec_get(other), r;
    if (UNLIKELY(__builtin_sub_overflow(a, b, &r)))
        dec_overflow();
    if (r >= 0) return dec_wrap(r);
    i128 neg;
    if (UNLIKELY(__builtin_sub_overflow((i128)0, r, &neg)))
        dec_overflow();
    return dec_wrap(neg);
}

static VALUE
dec_fix(VALUE self)
{
    i128 v = dec_get(self);
    return dec_wrap((v / SCALE) * SCALE);
}

static VALUE
dec_frac(VALUE self)
{
    return dec_wrap(dec_get(self) % SCALE);
}

static VALUE
dec_integer_p(VALUE self)
{
    return RBOOL(dec_get(self) % SCALE == 0);
}

/* Sum an Array of Dec values in i128, wrapping only the final result. */
static VALUE
dec_s_sum(int argc, VALUE *argv, VALUE klass)
{
    (void)klass;
    VALUE ary;
    rb_scan_args(argc, argv, "1", &ary);
    ary = rb_check_array_type(ary);
    if (NIL_P(ary))
        rb_raise(rb_eTypeError, "not an array");

    long len = RARRAY_LEN(ary);
    i128 acc = 0;
    for (long i = 0; i < len; i++) {
        i128 r;
        if (UNLIKELY(__builtin_add_overflow(acc, dec_get(RARRAY_AREF(ary, i)), &r)))
            dec_overflow();
        acc = r;
    }
    return dec_wrap(acc);
}

static VALUE
dec_to_f(VALUE self)
{
    return DBL2NUM((double)dec_get(self) / (double)SCALE);
}

static VALUE
dec_to_i(VALUE self)
{
    return i128_to_ruby(dec_get(self) / SCALE);
}

static VALUE
dec_to_r(VALUE self)
{
    return rb_Rational(i128_to_ruby(dec_get(self)), i128_to_ruby(SCALE));
}

static VALUE
dec_to_s(VALUE self)
{
    i128 v = dec_get(self);
    int neg = v < 0;
    u128 av = neg ? -(u128)v : (u128)v;
    u128 whole = av / (u128)SCALE;
    u128 frac = av % (u128)SCALE;

    /* Format whole part. */
    char buf[80]; /* sign + 39 whole digits + '.' + 18 frac digits + '\0' */
    int pos = 0;
    if (neg) buf[pos++] = '-';

    /* Write whole part digits in reverse, then flip. */
    int start = pos;
    if (whole == 0) {
        buf[pos++] = '0';
    }
    else {
        while (whole > 0) {
            buf[pos++] = '0' + (int)(whole % 10);
            whole /= 10;
        }
        for (int l = start, r = pos - 1; l < r; l++, r--) {
            char tmp = buf[l]; buf[l] = buf[r]; buf[r] = tmp;
        }
    }

    buf[pos++] = '.';

    if (frac == 0) {
        buf[pos++] = '0';
    }
    else {
        /* Write 18 fractional digits, then strip trailing zeros. */
        char frac_buf[18];
        for (int i = 17; i >= 0; i--) {
            frac_buf[i] = '0' + (int)(frac % 10);
            frac /= 10;
        }
        int last = 17;
        while (last > 0 && frac_buf[last] == '0') last--;
        for (int i = 0; i <= last; i++) buf[pos++] = frac_buf[i];
    }

    return rb_str_new(buf, pos);
}

static VALUE
dec_inspect(VALUE self)
{
    VALUE str = dec_to_s(self);
    rb_str_cat_cstr(str, "dec");
    return str;
}

static const int64_t POW10[] = {
    1LL, 10LL, 100LL, 1000LL, 10000LL, 100000LL, 1000000LL,
    10000000LL, 100000000LL, 1000000000LL, 10000000000LL,
    100000000000LL, 1000000000000LL, 10000000000000LL,
    100000000000000LL, 1000000000000000LL, 10000000000000000LL,
    100000000000000000LL, 1000000000000000000LL
};

/* Reciprocal lookup tables for division-free x / 10^n (u64 inputs).
   div_pow10(x, n) = (uint64_t)((u128)x * RECIP10[n] >> 64) >> SHIFT10[n] */
static const uint64_t RECIP10[] = {
    UINT64_C(0),
    UINT64_C(0xCCCCCCCCCCCCCCCD),
    UINT64_C(0xA3D70A3D70A3D70B),
    UINT64_C(0x624DD2F1A9FBE77),
    UINT64_C(0xD1B71758E219652C),
    UINT64_C(0xA7C5AC471B478424),
    UINT64_C(0x8637BD05AF6C69B6),
    UINT64_C(0xD6BF94D5E57A42BD),
    UINT64_C(0xABCC77118461CEFD),
    UINT64_C(0x89705F4136B4A598),
    UINT64_C(0xDBE6FECEBDEDD5BF),
    UINT64_C(0xAFEBFF0BCB24AAFE),
    UINT64_C(0x8CBCCC096F5088CC),
    UINT64_C(0xE12E13424BB40E14),
    UINT64_C(0xB424DC35095CD810),
    UINT64_C(0x901D7CF73AB0ACD9),
    UINT64_C(0xE69594BEC44DE15C),
    UINT64_C(0xB877AA3236A4B44A),
    UINT64_C(0x9392EE8E921D5D08)
};

static const uint8_t SHIFT10[] = {
    0, 3, 6, 9, 13, 16, 19, 23, 26, 29, 33, 36, 39, 43, 46, 49, 53, 56, 59
};

static inline uint64_t
div_pow10(uint64_t x, int n)
{
    if (n == 0) return x;
    return (uint64_t)((u128)x * RECIP10[n] >> 64) >> SHIFT10[n];
}

static VALUE
dec_floor(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 0, 1);
    int ndigits = argc > 0 ? NUM2INT(argv[0]) : 0;
    i128 v = dec_get(self);

    if (ndigits >= 18) return self;

    if (ndigits <= 0) {
        i128 q = v / SCALE;
        if (v < 0 && v % SCALE != 0) q--;
        VALUE result = i128_to_ruby(q);
        if (ndigits == 0) return result;
        return rb_funcall(result, id_floor, 1, INT2FIX(ndigits));
    }

    int scale = 18 - ndigits;
    u128 av = v < 0 ? -(u128)v : (u128)v;
    if (av <= UINT64_MAX) {
        uint64_t uv = (uint64_t)av;
        uint64_t q = div_pow10(uv, scale);
        uint64_t rem = uv - q * (uint64_t)POW10[scale];
        if (v < 0 && rem != 0) q++;
        u128 result = (u128)q * (u128)POW10[scale];
        return dec_wrap(v < 0 ? -(i128)result : (i128)result);
    }

    i128 factor = (i128)POW10[scale];
    i128 q = v / factor;
    if (v < 0 && v % factor != 0) q--;
    i128 result;
    if (UNLIKELY(__builtin_mul_overflow(q, factor, &result)))
        dec_overflow();
    return dec_wrap(result);
}

static VALUE
dec_ceil(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 0, 1);
    int ndigits = argc > 0 ? NUM2INT(argv[0]) : 0;
    i128 v = dec_get(self);

    if (ndigits >= 18) return self;

    if (ndigits <= 0) {
        i128 q = v / SCALE;
        i128 r = v % SCALE;
        if (r < 0) { q--; r += SCALE; }
        if (r != 0) q++;
        VALUE result = i128_to_ruby(q);
        if (ndigits == 0) return result;
        return rb_funcall(result, id_ceil, 1, INT2FIX(ndigits));
    }

    int scale = 18 - ndigits;
    u128 av = v < 0 ? -(u128)v : (u128)v;
    if (av <= UINT64_MAX) {
        uint64_t uv = (uint64_t)av;
        uint64_t q = div_pow10(uv, scale);
        uint64_t rem = uv - q * (uint64_t)POW10[scale];
        if (rem == 0) return self;
        if (v >= 0) q++;
        u128 result = (u128)q * (u128)POW10[scale];
        return dec_wrap(v < 0 ? -(i128)result : (i128)result);
    }

    i128 factor = (i128)POW10[scale];
    i128 q = v / factor;
    i128 r = v % factor;
    if (r < 0) { q--; r += factor; }
    if (r == 0) return self;
    i128 result;
    if (UNLIKELY(__builtin_mul_overflow(q + 1, factor, &result)))
        dec_overflow();
    return dec_wrap(result);
}

static VALUE
dec_truncate(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 0, 1);
    int ndigits = argc > 0 ? NUM2INT(argv[0]) : 0;
    i128 v = dec_get(self);

    if (ndigits >= 18) return self;

    if (ndigits <= 0) {
        i128 q = v / SCALE;
        VALUE result = i128_to_ruby(q);
        if (ndigits == 0) return result;
        return rb_funcall(result, id_truncate, 1, INT2FIX(ndigits));
    }

    int scale = 18 - ndigits;
    u128 av = v < 0 ? -(u128)v : (u128)v;
    if (av <= UINT64_MAX) {
        uint64_t uv = (uint64_t)av;
        uint64_t q = div_pow10(uv, scale);
        u128 result = (u128)q * (u128)POW10[scale];
        return dec_wrap(v < 0 ? -(i128)result : (i128)result);
    }

    i128 factor = (i128)POW10[scale];
    return dec_wrap((v / factor) * factor);
}

void
Init_ext(void)
{
    VALUE rb_mNum = rb_const_get(rb_cObject, rb_intern_const("Num"));
    rb_cDec = rb_const_get(rb_mNum, rb_intern_const("Dec"));
    VALUE singleton = rb_singleton_class(rb_cDec);

    id_to_r     = rb_intern_const("to_r");
    id_cmp      = rb_intern_const("<=>");
    id_neg      = rb_intern_const("-@");
    id_floor    = rb_intern_const("floor");
    id_ceil     = rb_intern_const("ceil");
    id_truncate = rb_intern_const("truncate");
    id_lt       = rb_intern_const("<");
    id_le       = rb_intern_const("<=");
    id_gt       = rb_intern_const(">");
    id_ge       = rb_intern_const(">=");

    /* Cached zero avoids allocation for the common zero result. */
    dec_t *zd;
    dec_zero = TypedData_Make_Struct(rb_cDec, dec_t, &dec_type, zd);
    zd->value = 0;
    OBJ_FREEZE(dec_zero);
    rb_gc_register_mark_object(dec_zero);

    /* Disable public construction. */
    rb_undef_alloc_func(rb_cDec);
    rb_undef_method(singleton, "new");

    rb_define_private_method(singleton, "_wrap", dec_s_wrap, 1);
    rb_define_protected_method(rb_cDec, "raw", dec_raw, 0);
    rb_define_private_method(singleton, "from_integer", dec_from_integer, 1);

    rb_define_method(rb_cDec, "_dump", dec_dump, 1);
    rb_define_singleton_method(rb_cDec, "_load", dec_s_load, 1);

    rb_define_method(rb_cDec, "+",      dec_add, 1);
    rb_define_method(rb_cDec, "-",      dec_sub, 1);
    rb_define_method(rb_cDec, "-@",     dec_neg, 0);
    rb_define_method(rb_cDec, "*",      dec_mul, 1);
    rb_define_method(rb_cDec, "/",      dec_div_op, 1);
    rb_define_method(rb_cDec, "quo",    dec_div_op, 1);
    rb_define_method(rb_cDec, "%",      dec_mod, 1);
    rb_define_method(rb_cDec, "modulo", dec_mod, 1);
    rb_define_method(rb_cDec, "divmod", dec_divmod, 1);
    rb_define_method(rb_cDec, "div",    dec_div_int, 1);
    rb_define_method(rb_cDec, "**",     dec_pow, 1);

    rb_define_method(rb_cDec, "<=>",  dec_cmp, 1);
    rb_define_method(rb_cDec, "<",    dec_lt, 1);
    rb_define_method(rb_cDec, "<=",   dec_le, 1);
    rb_define_method(rb_cDec, ">",    dec_gt, 1);
    rb_define_method(rb_cDec, ">=",   dec_ge, 1);
    rb_define_method(rb_cDec, "hash", dec_hash, 0);
    rb_define_method(rb_cDec, "eql?", dec_eql, 1);

    rb_define_method(rb_cDec, "integer?",  dec_integer_p, 0);
    rb_define_method(rb_cDec, "zero?",     dec_zero_p, 0);
    rb_define_method(rb_cDec, "positive?", dec_positive_p, 0);
    rb_define_method(rb_cDec, "negative?", dec_negative_p, 0);
    rb_define_method(rb_cDec, "abs",       dec_abs, 0);
    rb_define_method(rb_cDec, "abs_diff",  dec_abs_diff, 1);
    rb_define_method(rb_cDec, "fix",       dec_fix, 0);
    rb_define_method(rb_cDec, "frac",      dec_frac, 0);

    rb_define_method(rb_cDec, "to_f",      dec_to_f, 0);
    rb_define_method(rb_cDec, "to_i",      dec_to_i, 0);
    rb_define_method(rb_cDec, "to_r",      dec_to_r, 0);
    rb_define_method(rb_cDec, "to_s",      dec_to_s, 0);
    rb_define_method(rb_cDec, "inspect",   dec_inspect, 0);
    rb_define_method(rb_cDec, "floor",    dec_floor, -1);
    rb_define_method(rb_cDec, "ceil",     dec_ceil, -1);
    rb_define_method(rb_cDec, "truncate", dec_truncate, -1);

    rb_define_singleton_method(rb_cDec, "sum", dec_s_sum, -1);
}
