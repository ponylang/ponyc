-- os.outputof is broken in premake4, hence this workaround
function llvm_config(opt)
  local stream = assert(io.popen("llvm-config-3.4 " .. opt))
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
  links "libponyc"
  local output = llvm_config("--libs")
  for lib in string.gmatch(output, "-l(%S+)") do
    links { lib }
  end
  configuration("not macosx")
  links {
    "tinfo",
    "dl"
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
    buildoptions {
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
    includedirs {
      llvm_config("--includedir")
    }
    buildoptions {
      "-std=gnu11"
    }
    defines {
      "_DEBUG",
      "_GNU_SOURCE",
      "__STDC_CONSTANT_MACROS",
      "__STDC_FORMAT_MACROS",
      "__STDC_LIMIT_MACROS",
    }
    files { "src/libponyc/**.c", "src/libponyc/**.h" }

  project "ponyc"
    kind "ConsoleApp"
    language "C++"
    buildoptions "-std=gnu11"
    files { "src/ponyc/**.c", "src/ponyc/**.h" }
    link_libponyc()

  include "utils/"
  include "test/"
