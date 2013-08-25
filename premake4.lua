solution "ponyc"
  configurations {
    "Debug",
    "Release",
    "Profile"
    }

  buildoptions {
    "-std=gnu99",
    "-march=native",
    "-falign-loops=16",
    }

  flags {
    "ExtraWarnings",
    "FatalWarnings",
    "Symbols",
    }

  configuration "macosx"
    buildoptions "-mno-avx"

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
      "-flto=jobserver",
      "-ffunction-sections",
      "-fdata-sections",
      "-freorder-blocks-and-partition",
      }
    linkoptions {
      "-flto=jobserver",
      "-fuse-ld=gold",
      "-fwhole-program",
      }

  configuration {
    "Release or Profile",
    "not macosx",
    }
    linkoptions {
      "-fuse-linker-plugin",
      }

  project "ponyc"
    kind "ConsoleApp"
    language "C"
    files "src/*.c"
