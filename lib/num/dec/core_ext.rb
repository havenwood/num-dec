require_relative "../dec"

class Integer
  def to_dec = Num::Dec.from(self)
end

class Float
  def to_dec = Num::Dec.from(self)
end

class Rational
  def to_dec = Num::Dec.from(self)
end

class String
  def to_dec = Num::Dec.from(self)
end

module Kernel
  module_function

  def Dec(input, exception: true) = Num::Dec.from(input, exception:)
end
