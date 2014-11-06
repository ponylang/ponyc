  solution "ponyc"
    configurations { "Debug", "Release", "Profile" }
    location( _OPTIONS["to"] )

    flags {
      "FatalWarnings",
      "MultiProcessorCompile",
      "ReleaseRuntime" --for all configs
    }

    configuration "Debug"
      targetdir "bin/debug"
      objdir "obj/debug"
      defines "DEBUG"
      flags { "Symbols" }

    configuration "Release"
      targetdir "bin/release"
      objdir "obj/release"

    configuration "Profile"
      targetdir "bin/profile"
      objdir "obj/profile"

    configuration "Release or Profile"
      defines "NDEBUG"
      optimize "Speed"
      --flags { "LinkTimeOptimization" }

      if not os.is("windows") then
        linkoptions {
          "-flto",
          "-fuse-ld=gold"
        }
      end

    configuration { "Profile", "gmake" }
      buildoptions "-pg"
      linkoptions "-pg"

    configuration { "Profile", "vs*" }
      buildoptions "/PROFILE"

    configuration "vs*"
      debugdir "."
      defines {
        -- disables warnings for vsnprintf
        "_CRT_SECURE_NO_WARNINGS"
      }

    configuration { "not windows" }
      linkoptions {
        "-pthread"
      }

    configuration { "macosx", "gmake" }
      toolset "clang"
      buildoptions "-Qunused-arguments"
      linkoptions "-Qunused-arguments"


    configuration "gmake"
      buildoptions {
        "-march=native"
      }

    configuration "vs*"
      architecture "x64"

  dofile("scripts/properties.lua")
  dofile("scripts/llvm.lua")
  dofile("scripts/helper.lua")

  project "libponyc"
    targetname "ponyc"
    kind "StaticLib"
    language "C"
    includedirs {
      llvm_config("--includedir"),
      "src/common"
    }

    files {
      "src/common/*.h",
      "src/libponyc/**.c*",
      "src/libponyc/**.h"
    }

    configuration "gmake"
      buildoptions{
        "-std=gnu11"
      }
      defines {
        "__STDC_CONSTANT_MACROS",
        "__STDC_FORMAT_MACROS",
        "__STDC_LIMIT_MACROS"
      }
      excludes { "src/libponyc/**.cc" }
    configuration "vs*"
      defines {
        "PONY_USE_BIGINT"
      }
      cppforce { "src/libponyc/**.c*" }
    configuration "*"

  project "libponyrt"
    targetname "ponyrt"
    kind "StaticLib"
    language "C"
    includedirs {
      "src/common/",
      "src/libponyrt/"
    }
    files {
      "src/libponyrt/**.c",
      "src/libponyrt/**.h"
    }
    configuration "gmake"
      buildoptions {
        "-std=gnu11"
      }
    configuration "vs*"
      cppforce { "src/libponyrt/**.c" }
    configuration "*"

  project "ponyc"
    kind "ConsoleApp"
    language "C++"
    includedirs {
      "src/common/"
    }
    files {
      "src/ponyc/**.h",
      "src/ponyc/**.c"
    }
    configuration "gmake"
      buildoptions "-std=gnu11"
    configuration "vs*"
      cppforce { "src/ponyc/**.c" }
    configuration "*"
      link_libponyc()

if ( _OPTIONS["with-tests"] or _OPTIONS["run-tests"] ) then
  project "gtest"
    targetname "gtest"
    language "C++"
    kind "StaticLib"

    configuration "gmake"
      buildoptions {
        "-std=gnu++11"
      }
    configuration "*"

    includedirs {
      "utils/gtest"
    }
    files {
      "utils/gtest/gtest-all.cc",
      "utils/gtest/gtest_main.cc"
    }

  project "testc"
    targetname "testc"
    testsuite()
    includedirs {
      "utils/gtest",
      "src/common",
      "src/libponyc"
    }
    files {
      "test/unit/ponyc/**.h",
      "test/unit/ponyc/**.cc"
    }
    link_libponyc()
    configuration "vs*"
      defines { "PONY_USE_BIGINT" }
    configuration "*"

  project "testrt"
    targetname "testrt"
    testsuite()
    links "libponyrt"
    includedirs {
      "utils/gtest",
      "src/common",
      "src/libponyrt"
    }
    files { "test/unit/ponyrt/**.cc" }
end

  if _ACTION == "clean" then
    --os.rmdir("bin") os.rmdir clears out the link targets of symbolic links...
    --os.rmdir("obj")
  end

  -- Allow for out-of-source builds.
  newoption {
    trigger     = "to",
    value       = "path",
    description = "Set output location for generated files."
  }

  newoption {
    trigger     = "with-tests",
    description = "Compile test suite for every build."
  }

  newoption {
    trigger = "run-tests",
    description = "Run the test suite on every successful build."
  }

  newoption {
    trigger     = "use-docsgen",
    description = "Select a tool for generating the API documentation",
    allowed = {
      { "sphinx", "Chooses Sphinx as documentation tool. (Default)" },
      { "doxygen", "Chooses Doxygen as documentation tool." }
    }
  }

  dofile("scripts/release.lua")

  -- Package release versions of ponyc for all supported platforms.
  newaction {
    trigger     = "release",
    description = "Prepare a new ponyc release.",
    execute     = dorelease
  }

  dofile("scripts/docs.lua")

  newaction {
    trigger     = "docs",
    value       = "tool",
    description = "Produce API documentation.",
    execute     = dodocs
  }
