solution "ponyc"
  configurations { "Debug", "Release" }

  project "ponyc"
    kind "ConsoleApp"
    language "C++"
    files { "**.h", "**.c", "**.cpp" }
    flags {
      "ExtraWarnings",
      "FatalWarnings",
      }

    configuration "Debug"
      flags { "Symbols" }

    configuration "Release"
      defines { "NDEBUG" }
      flags { "Optimize" }

