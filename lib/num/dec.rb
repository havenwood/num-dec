require_relative "dec/version"

module Num
  class Dec < Numeric
    SCALE = 10**18
    MAX_RAW = (1 << 127) - 1
    MIN_RAW = -(1 << 127)

    class << self
      def [](input) = from(input)

      def sum(values) = values.reduce(_wrap(0)) { |acc, dec| acc + dec }

      def from(input, exception: true)
        raw = case input
        when Dec then return input
        when Integer then return from_integer(input)
        when String then parse(input)
        when Float then parse(input.to_s)
        when Rational then (input * SCALE).round
        else raise TypeError, "can't convert #{input.class} into Num::Dec"
        end
        _wrap(raw)
      rescue ArgumentError, TypeError, RangeError
        raise if exception
        nil
      end

      def _load(str) = _wrap(Integer(str))

      undef_method :new

      private

      def _wrap(raw)
        raise RangeError, "Dec overflow" unless raw.between?(MIN_RAW, MAX_RAW)
        obj = allocate
        obj.instance_variable_set(:@raw, raw)
        obj.freeze
        obj
      end

      def from_integer(input) = _wrap(input * SCALE)

      def parse(str)
        str = str.strip
        negative = str.start_with?("-")
        str = str.delete_prefix("-").delete_prefix("+")

        whole, frac = str.split(".", 2)
        frac ||= "0"

        raise ArgumentError, "invalid value for Dec(): #{str.inspect}" unless whole.match?(/\A\d+\z/) && frac.match?(/\A\d+\z/)

        frac = frac.ljust(18, "0")[0, 18]

        raw = whole.to_i * SCALE + frac.to_i
        negative ? -raw : raw
      end
    end

    def _dump(_level) = raw.to_s

    # Arithmetic (truncate-toward-zero for * and /)
    def +(other) = _dec(raw + other.raw)
    def -(other) = _dec(raw - other.raw)
    def -@ = _dec(-raw)

    def *(other)
      product = raw * other.raw
      _dec(product.negative? ? -(-product / SCALE) : product / SCALE)
    end

    def /(other)
      raise ZeroDivisionError, "Dec division by zero" if other.raw.zero?
      a = raw.abs * SCALE
      q = a / other.raw.abs
      _dec((raw.negative? ^ other.raw.negative?) ? -q : q)
    end

    def quo(other) = self / other

    def **(other)
      raise TypeError, "Num::Dec#** supports only Integer exponents" unless other.is_a?(Integer)
      return self.class.from(1) / (self**other.abs) if other.negative?
      return self.class.from(1) if other.zero?
      return (self**(other / 2)).then { it * it } if other.even?

      self * self**other.pred
    end

    def %(other)
      raise ZeroDivisionError, "Dec modulo by zero" if other.raw.zero?
      _dec(raw - other.raw * (raw / other.raw))
    end

    alias_method :modulo, :%

    def divmod(other)
      raise ZeroDivisionError, "Dec divmod by zero" if other.raw.zero?
      q = raw / other.raw
      [q, _dec(raw - other.raw * q)]
    end

    def remainder(other)
      mod = self % other
      return mod if mod.zero?
      return mod - other if negative? ^ other.negative?

      mod
    end

    def div(other)
      raise ZeroDivisionError, "Dec div by zero" if other.raw.zero?
      raw / other.raw
    end

    def fdiv(other) = to_f / other.to_f

    # Comparison and equality
    def <=>(other)
      return raw <=> other.raw if other.is_a?(self.class)
      to_r <=> other.to_r if other.is_a?(Numeric)
    end

    def hash = raw.hash
    def eql?(other) = other.is_a?(self.class) && raw.eql?(other.raw)

    def coerce(other)
      [self.class.from(other), self]
    rescue ArgumentError
      raise TypeError, "#{other.class} can't be coerced into #{self.class}"
    end

    # Predicates
    def abs = _dec(raw.abs)
    def abs_diff(other) = (self - other).abs
    def integer? = (raw % SCALE).zero?
    def zero? = raw.zero?
    def positive? = raw.positive?
    def negative? = raw.negative?

    # Parts
    def fix = _dec(raw.negative? ? -(-raw / SCALE) * SCALE : (raw / SCALE) * SCALE)
    def frac = self - fix

    def deconstruct = [to_i, frac]

    def deconstruct_keys(keys)
      h = {whole: to_i, frac: frac}
      keys ? h.slice(*keys) : h
    end

    # Rounding
    def floor(ndigits = 0)
      return self if ndigits >= 18

      if ndigits <= 0
        q, = raw.divmod(SCALE)
        return ndigits.zero? ? q : q.floor(ndigits)
      end

      factor = 10**(18 - ndigits)
      _dec((raw / factor) * factor)
    end

    def ceil(ndigits = 0)
      return self if ndigits >= 18

      if ndigits <= 0
        q, r = raw.divmod(SCALE)
        result = r.zero? ? q : q.succ
        return ndigits.zero? ? result : result.ceil(ndigits)
      end

      factor = 10**(18 - ndigits)
      q, r = raw.divmod(factor)
      r.zero? ? self : _dec(q.succ * factor)
    end

    def round(ndigits = 0, half: :up)
      return self if ndigits >= 18
      return round(0, half:).round(ndigits) if ndigits.negative?

      r = raw
      factor = 10**(18 - ndigits)
      abs_raw = r.abs
      q, remainder = abs_raw.divmod(factor)
      half_factor = factor / 2

      q = if remainder > half_factor || (remainder == half_factor && round_half_up?(q, half))
        q + 1
      else
        q
      end

      rounded = q * factor
      rounded = -rounded if r.negative?
      ndigits.zero? ? rounded / SCALE : _dec(rounded)
    end

    def truncate(ndigits = 0)
      return self if ndigits >= 18

      if ndigits <= 0
        result = to_i
        return ndigits.zero? ? result : result.truncate(ndigits)
      end

      factor = 10**(18 - ndigits)
      return _dec(-(-raw / factor) * factor) if raw.negative?

      _dec((raw / factor) * factor)
    end

    # Conversion
    def to_f = raw.to_f / SCALE

    def to_i
      return -(-raw / SCALE) if raw.negative?

      raw / SCALE
    end

    def to_r = Rational(raw, SCALE)

    def to_s
      r = raw
      sign = r.negative? ? "-" : ""
      abs_raw = r.abs
      whole = abs_raw / SCALE
      frac = abs_raw % SCALE
      return "#{sign}#{whole}.0" if frac.zero?

      frac_str = frac.to_s.rjust(18, "0").sub(/0+\z/, "")
      "#{sign}#{whole}.#{frac_str}"
    end

    def inspect = "#{self}dec"

    # Checked operations
    def add_checked(other) = checked { self + other }
    def sub_checked(other) = checked { self - other }
    def mul_checked(other) = checked { self * other }

    def div_checked(other)
      return [:err, :div_by_zero] if other.is_a?(self.class) && other.zero?
      checked { self / other }
    end

    protected

    attr_reader :raw

    private

    def _dec(raw) = self.class.send(:_wrap, raw)

    def checked
      [:ok, yield]
    rescue RangeError
      [:err, :overflow]
    end

    def round_half_up?(q, mode)
      case mode
      when :up then true
      when :down then false
      when :even then q.odd?
      else raise ArgumentError, "invalid rounding mode: #{mode}"
      end
    end
  end

  module_function

  def Dec(input, exception: true) = Dec.from(input, exception:)
end

begin
  verbose, $VERBOSE = $VERBOSE, nil
  require "num/dec/ext"
rescue LoadError
ensure
  $VERBOSE = verbose
end

Num::Dec::MAX = Num::Dec.send(:_wrap, Num::Dec::MAX_RAW)
Num::Dec::MIN = Num::Dec.send(:_wrap, Num::Dec::MIN_RAW)
