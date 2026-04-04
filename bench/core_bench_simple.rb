yjit = " +YJIT" if RUBY_DESCRIPTION.match?(/YJIT/)
puts "Ruby #{RUBY_VERSION}#{yjit}"
puts "=" * 60

N = 5_000_000

def bench(label, n = N)
  # Warmup
  (n / 10).times { yield }
  t0 = Process.clock_gettime(Process::CLOCK_MONOTONIC)
  n.times { yield }
  elapsed = Process.clock_gettime(Process::CLOCK_MONOTONIC) - t0
  ips = n / elapsed
  ns = elapsed / n * 1_000_000_000
  printf "  %-20s %10.3fM i/s  (%5.1f ns)\n", label, ips / 1_000_000, ns
  ips
end

d1 = Decimal("29.99")
d2 = Decimal("1.50")
r1 = Rational(2999, 100)
r2 = Rational(150, 100)
f1 = 29.99
f2 = 1.50

puts "\n--- Addition ---"
bench("Decimal") { d1 + d2 }
bench("Rational") { r1 + r2 }
bench("Float") { f1 + f2 }

puts "\n--- Multiplication ---"
bench("Decimal") { d1 * d2 }
bench("Rational") { r1 * r2 }
bench("Float") { f1 * f2 }

puts "\n--- Division ---"
bench("Decimal") { d1 / d2 }
bench("Rational") { r1 / r2 }
bench("Float") { f1 / f2 }

dr = Decimal("29.985")
rr = Rational(29985, 1000)
fr = 29.985

puts "\n--- Round(2) ---"
bench("Decimal") { dr.round(2) }
bench("Rational") { rr.round(2) }
bench("Float") { fr.round(2) }

puts "\n--- Parse ---"
bench("Decimal") { Decimal("29.99") }
bench("Rational") { Rational("29.99") }
bench("Float") { Float("29.99") }

puts "\n--- to_s ---"
bench("Decimal") { d1.to_s }
bench("Rational") { r1.to_s }
bench("Float") { f1.to_s }

decs = Array.new(1000, d1)
rats = Array.new(1000, r1)
flts = Array.new(1000, f1)

puts "\n--- Sum 1000 ---"
bench("Decimal", 5_000) { decs.sum(Decimal(0)) }
bench("Rational", 5_000) { rats.sum }
bench("Float", 5_000) { flts.sum }
