function cpp_lib()
  language "C++"
  kind "StaticLib"
  buildoptions "-std=gnu++11"
end

project "gtest"
  cpp_lib()

  targetdir "../bin/utils/"
  objdir "../obj/utils/"

  includedirs { "gtest/" }
  files {
    "gtest/gtest-all.cc",
    "gtest/gtest_main.cc"
  }
