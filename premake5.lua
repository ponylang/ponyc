  local force_cpp = { }
    function cppforce(inFiles)
      for _, val in ipairs(inFiles) do
        for _, fname in ipairs(os.matchfiles(val)) do
          table.insert(force_cpp, path.getabsolute(fname))
        end
      end
  end

-- gmake
  premake.override(path, "iscfile", function(base, fname)
    if table.contains(force_cpp, fname) then
      return false
    else
      return base(fname)
    end
  end)

-- Visual Studio
  premake.override(premake.vstudio.vc2010, "additionalCompileOptions", function(base, cfg, condition)
    if cfg.abspath then
      if table.contains(force_cpp, cfg.abspath) then
        _p(3,'<CompileAs %s>CompileAsCpp</CompileAs>', condition)
      end
    end
    return base(cfg, condition)
  end)

  function link_libponyc()
    configuration "gmake"
    linkoptions {
      llvm_config("--ldflags")
    }
    configuration "vs*"
    libdirs {
      llvm_config("--libdir")
    }
    configuration "*"
    links { "libponyc", "libponyrt" }
    local output = llvm_config("--libs")
    for lib in string.gmatch(output, "-l(%S+)") do
      links { lib }
    end
  end

  function llvm_config(opt)
    --expect symlink of llvm-config to your config version (e.g llvm-config-3.4)
    local stream = assert(io.popen("llvm-config " .. opt))
    local output = ""
    --llvm-config contains '\n'
    while true do
      local curr = stream:read("*l")
      if curr == nil then
        break
      end
      output = output .. curr
    end
    stream:close()
    return output
  end

  function testsuite()
    kind "ConsoleApp"
    language "C++"
    links "gtest"
    configuration "gmake"
    buildoptions { "-std=gnu++11" }
    configuration "*"
    if (_OPTIONS["run-tests"]) then
      configuration "gmake"
      postbuildcommands { "$(TARGET)" }
      configuration "vs*"
      postbuildcommands { "\"$(TargetPath)\"" }
      configuration "*"
    end
  end

  solution "ponyc"
    configurations { "Debug", "Release", "Profile" }
    location( _OPTIONS["to"] )

    flags {
      "FatalWarnings",
      "MultiProcessorCompile",
      "ReleaseRuntime" --for all configs
    }

    configuration "Debug"
      targetdir "build/debug"
      objdir "build/debug/obj"
      defines "DEBUG"
      flags { "Symbols" }

    configuration "Release"
      targetdir "build/release"
      objdir "build/release/obj"

    configuration "Profile"
      targetdir "build/profile"
      objdir "build/profile/obj"

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
      local version, exitcode = os.outputof("git describe --tags --always")
      
      if exitcode ~= 0 then
        version = os.outputof("type VERSION")
      end
      
      debugdir "."
      defines {
        -- disables warnings for vsnprintf
        "_CRT_SECURE_NO_WARNINGS",
        "PONY_VERSION=\"" .. string.gsub(version, "\n", "") .. "\"",
        "PLATFORM_TOOLS_VERSION=$(PlatformToolsetVersion)"
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
      llvm_config("--includedir"),
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
      "lib/gtest/gtest-all.cc",
      "lib/gtest/gtest_main.cc"
    }

  project "testc"
    targetname "testc"
    testsuite()
    includedirs {
      "lib/gtest",
      "src/common",
      "src/libponyc"
    }
    files {
      "test/libponyc/**.h",
      "test/libponyc/**.cc"
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
      "lib/gtest",
      "src/common",
      "src/libponyrt"
    }
    files { "test/libponyrt/**.cc" }
end

  if _ACTION == "clean" then
    os.rmdir("build")
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
