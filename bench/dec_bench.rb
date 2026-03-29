require "benchmark/ips"
require "num/dec"

puts "Ruby #{RUBY_VERSION}"
puts "=" * 60

d1 = Num::Dec.from("19.99")
d2 = Num::Dec.from("1.50")

r1 = Rational(1999, 100)
r2 = Rational(150, 100)

f1 = 19.99
f2 = 1.50

Benchmark.ips do
  it.config(warmup: 1, time: 3)

  it.report("Dec  +") { d1 + d2 }
  it.report("Rational +") { r1 + r2 }
  it.report("Float +") { f1 + f2 }

  it.compare!
end

puts

Benchmark.ips do
  it.config(warmup: 1, time: 3)

  it.report("Dec  *") { d1 * d2 }
  it.report("Rational *") { r1 * r2 }
  it.report("Float *") { f1 * f2 }

  it.compare!
end

puts

Benchmark.ips do
  it.config(warmup: 1, time: 3)

  it.report("Dec  /") { d1 / d2 }
  it.report("Rational /") { r1 / r2 }
  it.report("Float /") { f1 / f2 }

  it.compare!
end

puts

decs = Array.new(1000, d1)
rats = Array.new(1000, r1)
ints = Array.new(1000, 1999)

Benchmark.ips do
  it.config(warmup: 1, time: 3)

  it.report("Dec.sum(1000)") { Num::Dec.sum(decs) }
  it.report("Integer sum 1000") { ints.sum }
  it.report("Rational sum 1000") { rats.sum }

  it.compare!
end
