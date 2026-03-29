Gem::Specification.new do |spec|
  spec.name = "num-dec"
  spec.version = File.read(File.join(__dir__, "lib/num/dec/version.rb"))[/VERSION = "(.+)"/, 1]
  spec.authors = ["Shannon Skipper"]
  spec.email = ["shannonskipper@gmail.com"]

  spec.summary = "Exact decimal arithmetic with 18 digits of precision"
  spec.description = "A 128-bit decimal fixed-point number for Ruby inspired by Roc's Dec type with an optional C extension for native i128 performance"
  spec.homepage = "https://github.com/havenwood/num-dec"
  spec.license = "MIT"
  spec.required_ruby_version = ">= 4.0"

  spec.metadata["source_code_uri"] = spec.homepage
  spec.metadata["rubygems_mfa_required"] = "true"

  spec.files = %w[LICENSE.txt Rakefile README.md] + Dir["lib/**/*.rb"] + Dir["ext/**/*.{c,rb}"]
  spec.extensions = ["ext/num/dec/extconf.rb"]
end
