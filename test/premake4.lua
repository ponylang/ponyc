function unittest()
  configuration "Debug"
    targetdir "../bin/tests/debug"
    objdir "../obj/tests/debug"

  configuration "Release"
    targetdir "../bin/tests/release"
    objdir "../obj/tests/release"

  configuration "*"
    includedirs {
      "../utils/gtest/",
      "../src/",
      "../inc/"
    }
    libdirs "../bin/utils/"
    if not os.is("windows") then
      buildoptions "-std=gnu++11"
    end
    language "C++"
    kind "ConsoleApp"
    links "gtest"
    link_libponyc()
end

function testutil()
  configuration "Debug"
    targetdir "../bin/tests/debug"
    objdir "../obj/tests/debug"

  configuration "Release"
    targetdir "../bin/tests/release"
    objdir "../obj/tests/release"

  configuration "*"
    includedirs {
      "../src/",
      "../inc/"
    } 
    if not os.is("windows") then
      buildoptions "-std=gnu11"
    end
    language "C++"
    kind "ConsoleApp"
    link_libponyc()
end

project "unit"
  unittest()
  files { "unit/*.cc", "unit/*.h" }

project "bnf"
  testutil()
  files { "bnf/*.c", "bnf/*.h" }
