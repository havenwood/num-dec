require "mkmf"

unless have_type("__int128")
  File.write("Makefile", "all install clean:\n\t@:\n")
  exit
end

create_makefile("num/dec/ext")
