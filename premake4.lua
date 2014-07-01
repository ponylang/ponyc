solution "ponyc"
  configurations {
    "Debug",
    "Release",
    "Profile"
    }

  buildoptions {
    "-std=gnu11",
    "-march=native",
    }

  flags {
    "ExtraWarnings",
    "FatalWarnings",
    "Symbols",
    }

  configuration "macosx"
    buildoptions "-Qunused-arguments"

  configuration "Debug"
    targetdir "bin/debug"

  configuration "Release"
    targetdir "bin/release"

  configuration "Profile"
    targetdir "bin/profile"
    buildoptions "-pg"
    linkoptions "-pg"

  configuration "Release or Profile"
    defines "NDEBUG"
    buildoptions {
      "-O3",
      "-flto",
      }
    linkoptions {
      "-flto",
      "-fuse-ld=gold",
      }

  configuration {
    "Release or Profile",
    "not macosx",
    }
    buildoptions "-pthread"
    linkoptions "-pthread"

  project "libponyc"
    kind "StaticLib"
    language "C"
    buildoptions "-std=gnu11"
    files "src/libponyc/**.c"

  project "ponyc"
    kind "ConsoleApp"
    language "C"
    files "src/ponyc/**.c"
    links {"m", "libponyc"}

  include "utils/"
  include "test/"
