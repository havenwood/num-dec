require_relative "test_helper"

class DecTest < Minitest::Test
  Dec = Num::Dec

  # Construction
  def test_from_integer = assert_equal "42.0", Dec.from(42).to_s
  def test_from_string = assert_equal "3.14", Dec.from("3.14").to_s
  def test_from_string_no_fraction = assert_equal "7.0", Dec.from("7").to_s
  def test_from_string_leading_zeros = assert_equal "1.000001", Dec.from("1.000001").to_s
  def test_from_rational = assert_equal Rational(1, 4), Dec.from(Rational(1, 4)).to_r
  def test_from_float = assert_equal "42.42", Dec.from(42.42).to_s
  def test_from_float_exact = assert_equal Dec.from("0.25"), Dec.from(0.25)
  def test_from_negative_string = assert_equal "-99.5", Dec.from("-99.5").to_s

  def test_from_dec_idempotent
    d = Dec.from(42)
    assert_same d, Dec.from(d)
  end

  def test_from_exception_false = assert_nil Dec.from("bad", exception: false)
  def test_from_rejects_invalid = assert_raises(ArgumentError) { Dec.from("abc") }
  def test_from_rejects_unsupported_type = assert_raises(TypeError) { Dec.from(:foo) }
  def test_from_positive_sign_string = assert_equal "3.14", Dec.from("+3.14").to_s
  def test_from_overflow_exception_false = assert_nil Dec.from(10**40, exception: false)

  # .[] factory
  def test_bracket = assert_equal Dec.from(42), Dec[42]
  def test_bracket_string = assert_equal Dec.from("3.14"), Dec["3.14"]

  # Arithmetic
  def test_addition = assert_equal Dec.from(3), Dec.from(1) + Dec.from(2)
  def test_subtraction = assert_equal Dec.from(2), Dec.from(5) - Dec.from(3)
  def test_multiplication = assert_equal Dec.from(42), Dec.from(6) * Dec.from(7)
  def test_multiplication_decimals = assert_equal Dec.from("0.02"), Dec.from("0.1") * Dec.from("0.2")
  def test_division = assert_equal Dec.from("2.5"), Dec.from(10) / Dec.from(4)
  def test_division_by_zero = assert_raises(ZeroDivisionError) { Dec.from(1) / Dec.from(0) }
  def test_negation = assert_equal Dec.from(-5), -Dec.from(5)
  def test_quo = assert_equal Dec.from("2.5"), Dec.from(10).quo(Dec.from(4))
  def test_fdiv = assert_in_delta 2.5, Dec.from(10).fdiv(Dec.from(4))

  # Exponentiation
  def test_power = assert_equal Dec.from(8), Dec.from(2)**3
  def test_power_zero = assert_equal Dec.from(1), Dec.from(42)**0
  def test_power_negative = assert_equal Dec.from("0.5"), Dec.from(2)**-1

  def test_power_negative_base
    assert_equal Dec.from(4), Dec.from(-2)**2
    assert_equal Dec.from(-8), Dec.from(-2)**3
  end

  def test_power_zero_base_negative_exp = assert_raises(ZeroDivisionError) { Dec.from(0)**-1 }
  def test_power_non_integer = assert_raises(TypeError) { Dec.from(2)**Dec.from("0.5") }

  # Modulo and integer division
  def test_modulo = assert_equal Dec.from(2), Dec.from(5) % Dec.from(3)
  def test_modulo_negative_dividend = assert_equal Dec.from(1), Dec.from(-5) % Dec.from(3)
  def test_modulo_negative_divisor = assert_equal Dec.from("-1.0"), Dec.from(5) % Dec.from(-3)
  def test_modulo_both_negative = assert_equal Dec.from("-2.0"), Dec.from(-5) % Dec.from(-3)
  def test_modulo_method = assert_equal Dec.from(2), Dec.from(5).modulo(Dec.from(3))
  def test_modulo_by_zero = assert_raises(ZeroDivisionError) { Dec.from(1) % Dec.from(0) }

  def test_divmod = assert_equal [1, Dec.from(2)], Dec.from(5).divmod(Dec.from(3))
  def test_divmod_negative_dividend = assert_equal [-2, Dec.from(1)], Dec.from(-5).divmod(Dec.from(3))
  def test_divmod_negative_divisor = assert_equal [-2, Dec.from("-1.0")], Dec.from(5).divmod(Dec.from(-3))
  def test_divmod_both_negative = assert_equal [1, Dec.from("-2.0")], Dec.from(-5).divmod(Dec.from(-3))
  def test_divmod_by_zero = assert_raises(ZeroDivisionError) { Dec.from(1).divmod(Dec.from(0)) }

  def test_remainder = assert_equal Dec.from(2), Dec.from(5).remainder(Dec.from(3))
  def test_remainder_negative = assert_equal Dec.from(-2), Dec.from(-5).remainder(Dec.from(3))
  def test_div = assert_equal 1, Dec.from(5).div(Dec.from(3))
  def test_div_negative_divisor = assert_equal(-2, Dec.from(5).div(Dec.from(-3)))
  def test_div_both_negative = assert_equal 1, Dec.from(-5).div(Dec.from(-3))
  def test_div_by_zero = assert_raises(ZeroDivisionError) { Dec.from(1).div(Dec.from(0)) }

  # Truncate-toward-zero in * and /
  def test_mul_truncates_toward_zero
    a = Dec.new(7)
    assert_equal Dec.new(0), (-a) * Dec.new(3)
  end

  def test_div_truncates_toward_zero
    assert_equal Dec.from("-3.333333333333333333"), Dec.from(-10) / Dec.from(3)
    assert_equal Dec.from("3.333333333333333333"), Dec.from(10) / Dec.from(3)
  end

  # Comparison
  def test_compare = assert_operator Dec.from(1), :<, Dec.from(2)
  def test_compare_with_integer = assert_equal 0, Dec.from(5) <=> 5
  def test_compare_with_rational = assert_equal 0, Dec.from("0.5") <=> Rational(1, 2)
  def test_compare_with_non_numeric = assert_nil Dec.from(1) <=> "foo"
  def test_sorting = assert_equal [Dec.from(1), Dec.from(2), Dec.from(3)], [Dec.from(3), Dec.from(1), Dec.from(2)].sort

  # Coercion
  def test_coerce_integer = assert_equal Dec.from(7), 5 + Dec.from(2)
  def test_coerce_rational = assert_equal Dec.from("1.25"), Rational(1, 4) + Dec.from(1)
  def test_coerce_unsupported_type = assert_raises(TypeError) { Dec.from(1).coerce("bad") }

  # Hash and equality
  def test_hash_equality = assert_equal Dec.from(5).hash, Dec.from(5).hash
  def test_eql = assert_operator Dec.from(5), :eql?, Dec.from(5)
  def test_eql_non_dec = refute_operator Dec.from(5), :eql?, 5
  def test_hash_key = assert_equal :one, {Dec.from(1) => :one}[Dec.from(1)]

  # Conversion
  def test_to_f = assert_in_delta 2.5, Dec.from("2.5").to_f
  def test_to_i = assert_equal 9, Dec.from("9.99").to_i
  def test_to_i_negative = assert_equal(-3, Dec.from("-3.7").to_i)
  def test_to_r = assert_equal Rational(1, 2), Dec.from("0.5").to_r
  def test_to_s = assert_equal "100.0", Dec.from(100).to_s
  def test_inspect = assert_equal "1.5dec", Dec.from("1.5").inspect

  # Rounding
  def test_floor = assert_equal 3, Dec.from("3.7").floor
  def test_floor_negative = assert_equal(-4, Dec.from("-3.7").floor)
  def test_floor_ndigits = assert_equal Dec.from("3.1"), Dec.from("3.14").floor(1)
  def test_floor_negative_ndigits = assert_equal(-10, Dec.from("-3.7").floor(-1))
  def test_floor_ndigits_identity = assert_equal Dec.from("3.14"), Dec.from("3.14").floor(18)
  def test_ceil = assert_equal 4, Dec.from("3.1").ceil
  def test_ceil_negative = assert_equal(-3, Dec.from("-3.7").ceil)
  def test_ceil_ndigits = assert_equal Dec.from("3.2"), Dec.from("3.14").ceil(1)
  def test_ceil_exact = assert_equal 3, Dec.from(3).ceil
  def test_ceil_negative_ndigits = assert_equal(10, Dec.from("3.1").ceil(-1))
  def test_ceil_ndigits_identity = assert_equal Dec.from("3.14"), Dec.from("3.14").ceil(18)
  def test_round = assert_equal 4, Dec.from("3.5").round
  def test_round_half_up = assert_equal 4, Dec.from("3.5").round(half: :up)
  def test_round_half_down = assert_equal 3, Dec.from("3.5").round(half: :down)
  def test_round_half_even = assert_equal 4, Dec.from("3.5").round(half: :even)
  def test_round_half_even_2 = assert_equal 2, Dec.from("2.5").round(half: :even)
  def test_round_ndigits = assert_equal Dec.from("3.1"), Dec.from("3.14").round(1)
  def test_round_negative_ndigits = assert_equal 0, Dec.from("3.5").round(-1)
  def test_round_ndigits_identity = assert_equal Dec.from("3.14"), Dec.from("3.14").round(18)
  def test_round_invalid_half = assert_raises(ArgumentError) { Dec.from("3.5").round(half: :bad) }
  def test_truncate = assert_equal 3, Dec.from("3.7").truncate
  def test_truncate_negative = assert_equal(-3, Dec.from("-3.7").truncate)
  def test_truncate_ndigits = assert_equal Dec.from("3.1"), Dec.from("3.14").truncate(1)
  def test_truncate_negative_ndigits = assert_equal 0, Dec.from("3.7").truncate(-1)
  def test_truncate_ndigits_identity = assert_equal Dec.from("3.14"), Dec.from("3.14").truncate(18)

  # Parts
  def test_fix = assert_equal Dec.from(3), Dec.from("3.14").fix
  def test_fix_negative = assert_equal Dec.from(-3), Dec.from("-3.14").fix
  def test_frac = assert_equal Dec.from("0.14"), Dec.from("3.14").frac
  def test_frac_negative = assert_equal Dec.from("-0.14"), Dec.from("-3.14").frac
  def test_fix_plus_frac = assert_equal Dec.from("3.14"), Dec.from("3.14").fix + Dec.from("3.14").frac

  def test_deconstruct = assert_equal [3, Dec.from("0.14")], Dec.from("3.14").deconstruct

  def test_deconstruct_keys = assert_equal({whole: 3, frac: Dec.from("0.14")}, Dec.from("3.14").deconstruct_keys(nil))
  def test_pattern_match_hash = assert_pattern { Dec.from("3.14") => {whole: 3, frac: Dec} }
  def test_pattern_match_array = assert_pattern { Dec.from("3.14") => [3, Dec] }

  # Predicates
  def test_abs = assert_equal Dec.from(42), Dec.from(-42).abs
  def test_abs_diff = assert_equal Dec.from(2), Dec.from(3).abs_diff(Dec.from(5))
  def test_integer_true = assert_predicate Dec.from(3), :integer?
  def test_integer_false = refute_predicate Dec.from("3.14"), :integer?
  def test_zero = assert_predicate Dec.from(0), :zero?
  def test_positive = assert_predicate Dec.from(1), :positive?
  def test_negative = assert_predicate Dec.from(-1), :negative?
  def test_nonzero = assert_equal Dec.from(5), Dec.from(5).nonzero?
  def test_nonzero_zero = assert_nil Dec.from(0).nonzero?
  def test_frozen = assert_predicate Dec.from(1), :frozen?
  def test_shareable = assert Ractor.shareable?(Dec.from(1))
  def test_is_a_numeric = assert_kind_of Numeric, Dec.from(1)

  # Constants
  def test_max = assert_equal "170141183460469231731.687303715884105727", Dec::MAX.to_s
  def test_min = assert_equal "-170141183460469231731.687303715884105728", Dec::MIN.to_s

  # Overflow
  def test_overflow_add = assert_raises(RangeError) { Dec::MAX + Dec.from(1) }
  def test_overflow_negate_min = assert_raises(RangeError) { -Dec::MIN }

  # Checked
  def test_add_checked_ok = assert_equal [:ok, Dec.from(3)], Dec.from(1).add_checked(Dec.from(2))
  def test_add_checked_overflow = assert_equal [:err, :overflow], Dec::MAX.add_checked(Dec.from(1))
  def test_sub_checked_overflow = assert_equal [:err, :overflow], Dec::MIN.sub_checked(Dec.from(1))
  def test_mul_checked_overflow = assert_equal [:err, :overflow], Dec::MAX.mul_checked(Dec.from(2))
  def test_div_checked_ok = assert_equal [:ok, Dec.from("2.5")], Dec.from(10).div_checked(Dec.from(4))
  def test_div_checked_zero = assert_equal [:err, :div_by_zero], Dec.from(1).div_checked(Dec.from(0))

  # Parsing edge cases
  def test_parse_pads_fraction = assert_equal Dec.from("1.500000000000000000"), Dec.from("1.5")
  def test_parse_truncates_fraction = assert_equal Dec.from("1.123456789012345678"), Dec.from("1.1234567890123456789")

  # Converter
  def test_num_dec_converter = assert_equal Dec.from(42), Num.Dec(42)
  def test_num_dec_exception_false = assert_nil Num.Dec("bad", exception: false)
end
