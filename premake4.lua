solution "ponyc"
  configurations { "Debug", "Release" }

  project "ponyc"
    kind "ConsoleApp"
    language "C"
    files { "**.h", "**.c" }
    flags {
      "ExtraWarnings",
      "FatalWarnings",
      }

    buildoptions { "-std=gnu99" }
    links( "m" )

    configuration "Debug"
      flags { "Symbols" }

    configuration "Release"
      defines { "NDEBUG" }
      flags { "Optimize" }
