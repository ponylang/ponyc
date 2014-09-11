-- os.outputof is broken in premake4, hence this workaround
function llvm_config(opt)
  local stream = assert(io.popen("llvm-config-3.5 " .. opt))
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

function link_libponyc()
  linkoptions {
    llvm_config("--ldflags")
  }
  links { "libponyc", "libponycc", "z", "curses" }
  local output = llvm_config("--libs")
  for lib in string.gmatch(output, "-l(%S+)") do
    links { lib }
  end
  configuration("not macosx")
    links {
      "tinfo",
      "dl",
    }
  configuration("*")
end

solution "ponyc"
  configurations {
    "Debug",
    "Release",
    "Profile"
  }
  buildoptions {
    "-mcx16",
    "-march=native",
    "-pthread"
  }
  linkoptions {
    "-pthread"
  }
  flags {
    "ExtraWarnings",
    "FatalWarnings",
    "Symbols"
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
    flags "OptimizeSpeed"
    linkoptions {
      "-fuse-ld=gold",
    }

  project "libponyc"
    targetname "ponyc"
    kind "StaticLib"
    language "C"
    buildoptions "-std=gnu11"
    includedirs {
      llvm_config("--includedir")
    }
    defines {
      "_DEBUG",
      "__STDC_CONSTANT_MACROS",
      "__STDC_FORMAT_MACROS",
      "__STDC_LIMIT_MACROS",
    }
    files { "src/libponyc/**.c", "src/libponyc/**.h" }
    excludes { "src/libponyc/platform/**.cc" }

  project "libponycc"
    targetname "ponycc"
    kind "StaticLib"
    language "C++"
    buildoptions "-std=gnu++11"
    includedirs {
      llvm_config("--includedir")
    }
    defines {
      "_DEBUG",
      "__STDC_CONSTANT_MACROS",
      "__STDC_FORMAT_MACROS",
      "__STDC_LIMIT_MACROS",
    }
    files { "src/libponyc/codegen/host.cc" }

  project "ponyc"
    kind "ConsoleApp"
    language "C++"
    buildoptions "-std=gnu11"
    files { "src/ponyc/**.c", "src/ponyc/**.h" }
    link_libponyc()
    postbuildcommands {
      "rm -rf $(TARGETDIR)/builtin",
      "ln -sf " .. path.getabsolute("packages/builtin") .. " $(TARGETDIR)"
    }

  include "ponyrt/src/"
  include "utils/"
  include "test/"
