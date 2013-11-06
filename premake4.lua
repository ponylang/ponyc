solution "ponyc"
  configurations {
    "Debug",
    "Release",
    "Profile"
    }

  buildoptions {
    "-std=gnu99",
    "-march=native",
    }

  flags {
    "ExtraWarnings",
    "FatalWarnings",
    "Symbols",
    }

--  configuration "macosx"
  --  buildoptions "-Wa,-q -Wa,-Wno-trigraphs"

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
    links "m"
