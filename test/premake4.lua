function unittest()
  configuration "Debug"
    targetdir "../bin/tests/debug"
    objdir "../obj/tests/debug"

  configuration "Release"
    targetdir "../bin/tests/release"
    objdir "../obj/tests/release"

  configuration "macosx"
    links { "c++" }

  configuration "*"
    includedirs {
      "../utils/gtest/",
      "../src/",
      "../inc/"
    }
    libdirs "../bin/utils/"
    buildoptions "-std=gnu++11"
    language "C++"
    kind "ConsoleApp"
    links { "gtest", "libponyc", "pthread" }
end

project "unit"
  unittest()
  files "unit/*.cc"
