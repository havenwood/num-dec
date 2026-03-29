# Num::Dec

Exact decimal arithmetic for Ruby. A 128-bit fixed-point number with 18 decimal places, inspired by [Roc's `Dec`](https://www.roc-lang.org/builtins/Num#Dec).

```ruby
require "num/dec"

a = Num::Dec.from("19.99")
b = Num::Dec.from("1.50")
a + b  #=> 21.49dec
a * b  #=> 29.985dec
```

No floating-point surprises:

```ruby
Num::Dec.from("0.1") + Num::Dec.from("0.2") == Num::Dec.from("0.3")  #=> true
```

## Why not Float, Rational or BigDecimal?

**Float** is fast but inexact in base 10. `0.1 + 0.2 != 0.3`.

**Rational** is exact but slow (allocates two bignums per value) and prints as fractions (`3/10` not `0.3`).

**BigDecimal** is exact and arbitrary-precision but heavy. It allocates a variable-length digit array on the heap, requires `require "bigdecimal"` (bundled gem in Ruby 4.0) and its API differs from core numerics.

**Num::Dec** is exact, fixed-precision (18 decimal places) and lightweight. One 128-bit integer per value. Full `Numeric` subclass with operators, coercion, rounding and `Comparable`. About 1.8x faster than Rational with an optional C extension that brings it within 2x of Integer.

## Usage

```ruby
prices = [Num::Dec.from("9.99"), Num::Dec.from("4.50"), Num::Dec.from("12.00")]
prices.sort   #=> [4.5dec, 9.99dec, 12.0dec]
prices.min    #=> 4.5dec
prices.sum                #=> 26.49dec
Num::Dec.sum(prices)      #=> 26.49dec  (41x faster with C ext)
```

### Construction

`Num::Dec.from` and `Num::Dec[]` accept Integer, Float, String, Rational or Dec:

```ruby
Num::Dec.from(42)              #=> 42.0dec
Num::Dec["3.14"]               #=> 3.14dec
Num::Dec.from(0.25)            #=> 0.25dec
Num::Dec.from(Rational(1, 3))  #=> 0.333333333333333333dec
```

Passing an existing Dec returns it unchanged. Use `exception: false` to get nil on invalid input:

```ruby
Num::Dec.from("bad", exception: false)  #=> nil
```

### Converter

Include `Num` for a `Dec()` converter (like `Integer()` or `Rational()`):

```ruby
include Num

Dec(42)       #=> 42.0dec
Dec("3.14")   #=> 3.14dec
```

Or load core extensions for `to_dec` and `Kernel#Dec`:

```ruby
require "num/dec/core_ext"

42.to_dec     #=> 42.0dec
"3.14".to_dec #=> 3.14dec
Dec(42)       #=> 42.0dec
```

### Arithmetic

```ruby
a = Num::Dec.from("10")
b = Num::Dec.from("3")

a + b   #=> 13.0dec
a - b   #=> 7.0dec
a * b   #=> 30.0dec
a / b   #=> 3.333333333333333333dec
a % b   #=> 1.0dec
a ** 3  #=> 1000.0dec
-a      #=> -10.0dec
```

Integer and Rational coerce automatically:

```ruby
5 + Num::Dec.from(2)                #=> 7.0dec
Rational(1, 4) + Num::Dec.from(1)   #=> 1.25dec
```

### Rounding

All rounding methods accept an optional `ndigits`. `round` also accepts `half: :up | :down | :even`:

```ruby
d = Num::Dec.from("3.14")
d.floor          #=> 3
d.ceil           #=> 4
d.round          #=> 3
d.round(1)       #=> 3.1dec
d.truncate       #=> 3
d.fix            #=> 3.0dec
d.frac           #=> 0.14dec

Num::Dec.from("2.5").round(half: :even)  #=> 2
Num::Dec.from("3.5").round(half: :even)  #=> 4
```

### Range and Overflow

Over 170 quintillion whole units with 18 decimal places:

```ruby
Num::Dec::MAX  #=> 170141183460469231731.687303715884105727dec
Num::Dec::MIN  #=> -170141183460469231731.687303715884105728dec
```

Overflow raises `RangeError`. Checked variants return tuples:

```ruby
Num::Dec::MAX.add_checked(Num::Dec.from(1))    #=> [:err, :overflow]
Num::Dec.from(1).div_checked(Num::Dec.from(0))  #=> [:err, :div_by_zero]
```

### Pattern Matching

Dec supports `deconstruct` and `deconstruct_keys` for pattern matching:

```ruby
case Num::Dec.from("3.14")
in {whole: 0..9, frac:}
  puts "single digit, fractional part: #{frac}"
end

case Num::Dec.from("3.14")
in [3, frac]
  puts "three and #{frac}"
end
```

### Conversion

```ruby
d = Num::Dec.from("3.14")
d.to_f      #=> 3.14
d.to_i      #=> 3
d.to_r      #=> (157/50)
d.to_s      #=> "3.14"
d.integer?  #=> false
```

## Performance

An optional C extension embeds a native `__int128` directly in the Ruby object, bypassing bignum allocation entirely. With the C extension:

- Arithmetic is **3-15x faster** than pure Ruby (biggest wins in `*`, `/` and `**`)
- About **2x faster** than Rational for bulk arithmetic
- Within **2-3x of Integer** for addition
- `Dec.sum` loops in native i128 — about **130x faster** than `Array#sum`

Without the C extension the gem uses pure Ruby bignum arithmetic. Slower but correct on all platforms.

Run `ruby -Ilib bench/dec_bench.rb` to benchmark on your hardware.

## Installation

```ruby
gem "num-dec"
```

The C extension compiles on install if the platform supports `__int128` (GCC and Clang on 64-bit). Otherwise the gem falls back to pure Ruby. Requires Ruby >= 4.0.

## About

The `Dec` type originates from [Roc](https://www.roc-lang.org/), a lovely functional language. Roc stores `Dec` as a signed 128-bit integer with a fixed 18-digit fractional part, giving exact decimal arithmetic without floating-point representation error. This gem brings the same semantics to Ruby.

See Roc's [numeric types tutorial](https://www.roc-lang.org/tutorial#numeric-types) and the [`Dec` builtins reference](https://www.roc-lang.org/builtins/Num#Dec) for background on the design.
