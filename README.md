# Num::Dec

A decimal number for Ruby. Each value is a signed 128-bit integer scaled by 10^18, so decimal fractions are exact in memory. You get 18 decimal places of precision and whole numbers up to ~170 quintillion. Every instance is frozen and Ractor-shareable.

```ruby
"0.1".to_dec + "0.2".to_dec == "0.3".to_dec  #=> true
```

## A Ruby Numeric

Dec is a `Numeric` subclass with full operator support and coercion. It works anywhere Ruby numbers work.

```ruby
a, b = Dec(10), Dec(3)
a + b   #=> 13.0dec
a * b   #=> 30.0dec
a / b   #=> 3.333333333333333333dec
a ** 3  #=> 1000.0dec

5 + Dec(2)              #=> 7.0dec
Rational(1, 4) + Dec(1) #=> 1.25dec

%w[9.99 4.50 12.00].map(&:to_dec).sort  #=> [4.5dec, 9.99dec, 12.0dec]
```

Pattern matching, frozen and Ractor-shareable:

```ruby
case "3.14".to_dec
in {whole: 0..9, frac:}
  frac  #=> 0.14dec
end

Ractor.shareable?("3.14".to_dec)  #=> true
```

## Construction

```ruby
# With core extensions (adds to_dec and Dec())
require "num/dec/core_ext"

42.to_dec              #=> 42.0dec
"3.14".to_dec          #=> 3.14dec
0.25.to_dec            #=> 0.25dec
Rational(1, 3).to_dec  #=> 0.333333333333333333dec
Dec(42)                #=> 42.0dec
```

```ruby
# Without core extensions
require "num/dec"

Num::Dec.from(42)      #=> 42.0dec
Num::Dec["3.14"]       #=> 3.14dec
```

Passing an existing Dec returns it unchanged. Use `exception: false` to get nil on invalid input:

```ruby
Dec("bad", exception: false)  #=> nil
```

## Rounding

`floor`, `ceil`, `round` and `truncate` accept an optional `ndigits`. `round` accepts `half: :up | :down | :even`:

```ruby
d = Dec("3.14")
d.floor     #=> 3
d.ceil      #=> 4
d.round(1)  #=> 3.1dec
d.truncate  #=> 3

d.fix   #=> 3.0dec
d.frac  #=> 0.14dec

Dec("2.5").round(half: :even)  #=> 2
```

## Range and Overflow

Over 170 quintillion whole units with 18 decimal places:

```ruby
Num::Dec::MAX  #=> 170141183460469231731.687303715884105727dec
Num::Dec::MIN  #=> -170141183460469231731.687303715884105728dec
```

Overflow raises `RangeError`. Checked variants return tuples:

```ruby
Num::Dec::MAX.add_checked(Dec(1))  #=> [:err, :overflow]
Dec(1).div_checked(Dec(0))         #=> [:err, :div_by_zero]
```

## Conversion

`to_f`, `to_i`, `to_r`, `to_s` and `integer?` work as expected.

## When to use something else

**Float** is the right choice for graphics, scientific computing or anywhere you need speed over decimal precision. It uses base-2 under the hood so `0.1 + 0.2` is not exactly `0.3`, but it runs in hardware and covers a vast range.

**Rational** is exact and arbitrary-precision. Reach for it when you need fractions that can't be represented in 18 decimal places or when you want lossless arithmetic with no fixed range.

**BigDecimal** is exact with arbitrary precision and a variable-length representation. It makes sense when you need more than 18 digits or need to control precision dynamically.

**Dec** is a good default when you want exact base-10 arithmetic in a fixed, lightweight representation. One 128-bit integer per value with no heap allocation beyond the object itself.

## Performance

An optional C extension embeds a native `__int128` directly in the Ruby object, bypassing bignum allocation entirely. With the C extension:

- Arithmetic is **3-15x faster** than the pure Ruby fallback (biggest wins in `*`, `/` and `**`)
- About **2x faster** than Rational for bulk arithmetic
- Within **2-3x of Integer** for addition
- `Dec.sum` loops in native i128, about **130x faster** than `Array#sum`

Without the C extension, the gem uses pure Ruby bignum arithmetic. Slower but correct on all platforms.

Run `ruby -Ilib bench/dec_bench.rb` to benchmark on your hardware.

## Installation

```ruby
gem "num-dec"
```

The C extension compiles on install if the platform supports `__int128` (GCC and Clang on 64-bit). Otherwise, the gem falls back to pure Ruby. Requires Ruby >= 4.0.

## About

The `Dec` type originates from [Roc](https://www.roc-lang.org/), a functional language that uses Dec as its default numeric type for decimal values. Roc stores `Dec` as a signed 128-bit integer with a fixed 18-digit fractional part, giving exact decimal arithmetic without floating-point representation error. This gem brings the same semantics to Ruby as a `Numeric` subclass.

See Roc's [numeric types tutorial](https://www.roc-lang.org/tutorial#numeric-types) and the [`Dec` builtins reference](https://www.roc-lang.org/builtins/Num#Dec) for background on the design.
