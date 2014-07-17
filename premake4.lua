solution "ponyc"
  configurations {
    "Debug",
    "Release",
    "Profile"
    }

  buildoptions {
    "-march=native",
    "-pthread",
    }

  linkoptions {
    "-pthread",
  }

  flags {
    "ExtraWarnings",
    "FatalWarnings",
    "Symbols",
    }

  configuration "macosx"
    buildoptions "-Qunused-arguments"
    linkoptions "-Qunused-arguments"

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

  project "libponyc"
    targetname "ponyc"
    kind "StaticLib"
    language "C"
    buildoptions {
      "-std=gnu11",
      "`llvm-config-3.4 --cflags`",
      }
    files "src/libponyc/**.c"

  project "ponyc"
    kind "ConsoleApp"
    language "C++"
    buildoptions "-std=gnu11"
    linkoptions {
      "`llvm-config-3.4 --ldflags --libs all`"
      }
    files "src/ponyc/**.c"
    links "libponyc"

  include "utils/"
  include "test/"
