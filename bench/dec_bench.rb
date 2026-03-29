require "benchmark/ips"
require "num/dec"

D = Num::Dec
a = D.from("19.99")
b = D.from("1.50")

puts "Ruby #{RUBY_VERSION}"
puts "=" * 60
puts

Benchmark.ips do
  it.config(warmup: 1, time: 3)

  it.report("from(Integer)") { D.from(42) }
  it.report("from(String)") { D.from("19.99") }
  it.report("addition") { a + b }
  it.report("subtraction") { a - b }
  it.report("multiplication") { a * b }
  it.report("division") { a / b }
  it.report("negation") { -a }
  it.report("<=>") { a <=> b }
  it.report("zero?") { a.zero? }
  it.report("abs") { a.abs }
  it.report("floor") { a.floor }
  it.report("round(2)") { a.round(2) }
  it.report("to_s") { a.to_s }
  it.report("to_f") { a.to_f }

  it.compare!
end

puts
puts "--- sum 1000 values ---"
puts

decs = Array.new(1000, a)
rats = Array.new(1000, Rational(1999, 100))
floats = Array.new(1000, 19.99)

Benchmark.ips do
  it.config(warmup: 1, time: 3)

  it.report("Dec sum") { decs.sum }
  it.report("Rational sum") { rats.sum }
  it.report("Float sum") { floats.sum }

  it.compare!
end
