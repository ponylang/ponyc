function cpp_lib()
  language "C++"
  kind "StaticLib"
  if not os.is("windows") then
    buildoptions "-std=gnu++11"
  end
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
