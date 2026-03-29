require_relative "test_helper"
require "num/dec/core_ext"

class CoreExtTest < Minitest::Test
  Dec = Num::Dec

  def test_integer_to_dec = assert_equal Dec.from(42), 42.to_dec
  def test_integer_negative_to_dec = assert_equal Dec.from(-5), -5.to_dec

  def test_float_to_dec = assert_equal Dec.from(Rational(1, 4)), 0.25.to_dec

  def test_rational_to_dec = assert_equal Dec.from(Rational(1, 3)), Rational(1, 3).to_dec

  def test_string_to_dec = assert_equal Dec.from("3.14"), "3.14".to_dec
  def test_string_invalid = assert_raises(ArgumentError) { "bad".to_dec }

  def test_kernel_dec = assert_equal Dec.from(42), Dec(42)
  def test_kernel_dec_string = assert_equal Dec.from("3.14"), Dec("3.14")
  def test_kernel_dec_exception_false = assert_nil Dec("bad", exception: false)
end
