require "bundler/gem_tasks"
require "minitest/test_task"
require "standard/rake"

task default: %i[test standard]

Minitest::TestTask.create

desc "Run benchmarks"
task :bench do
  Dir["bench/*_bench.rb"].each { ruby "-Ilib", it }
end
