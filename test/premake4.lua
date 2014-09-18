project "unit"
  language "C++"
  kind "ConsoleApp"
  configuration "Debug"
    targetdir "../bin/tests/debug"
    objdir "../obj/tests/debug"

  configuration "Release"
    targetdir "../bin/tests/release"
    objdir "../obj/tests/release"

  configuration "*"
    includedirs {
      "../../utils/gtest/",
      "../../src/libponyc/",
      "../../src/libponyrt/",
      "../../inc/"
    }
  libdirs "../bin/utils/"

  if not os.is("windows") then
    buildoptions "-std=gnu++11"
  end

  files { "unit/**.cc", "unit/**.h" }

  links { "gtest" }

  link_libponyc()
