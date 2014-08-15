-- premake.override is a premake5 feature.
-- We use this approach to improve Visual Studio Support for Windows.
-- The goal is to force C++ compilation for non-*.cpp/cxx/cc file extensions.
-- By doing this properly, we avoid a MSVC warning about overiding /TC
-- with /TP.

if premake.override then
  local force_cpp = { }
 
  function cppforce(inFiles)
    for _, val in ipairs(inFiles) do
      for _, fname in ipairs(os.matchfiles(val)) do
        table.insert(force_cpp, path.getabsolute(fname))
      end
    end
  end
  
  premake.override(premake.vstudio.vc2010, "additionalCompileOptions", function(base, cfg, condition)
    if cfg.abspath then
      if table.contains(force_cpp, cfg.abspath) then
        _p(3,'<CompileAs %s>CompileAsCpp</CompileAs>', condition)
      end
    end
    return base(cfg, condition)
  end)
end

-- os.outputof is broken in premake4, hence this workaround
function llvm_config(opt)
    local llvm_bin = ""
    
    if os.is("windows") then
      llvm_bin = "llvm-config "
    else
      llvm_bin = "llvm-config-3.4 "
    end    

    local stream = assert(io.popen(llvm_bin .. opt))
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
  links { "libponyc" }
  
  if os.is("windows") then 
    libdirs {
     llvm_config("--libdir")
    }
  else
    linkoptions { llvm_config("--ldflags") }
  end

  local output = llvm_config("--libs")

  for lib in string.gmatch(output, "-l(%S+)") do
    links { lib }
  end

  if not ( os.is("macosx") or os.is("windows") ) then
    links {
      "tinfo",
      "dl"
    }
  end
end

solution "ponyc"
  configurations {
    "Debug",
    "Release",
    "Profile"
  }

  flags {
      "FatalWarnings",
      "MultiProcessorCompile"
    }

  links { "Shlwapi.lib" }
  
  if os.is("windows") then
    if(architecture) then
      architecture "x64"
    end
  end

  configuration "not windows"
    buildoptions {
      "-march=native",
      "-pthread"
    }
    linkoptions {
      "-pthread"
    }

  configuration "*"

  configuration "macosx"
    buildoptions "-Qunused-arguments"
    linkoptions "-Qunused-arguments"

  configuration "*"

  configuration "Debug"
    targetdir "bin/debug"
    flags { "Symbols" }

  configuration "Release"
    targetdir "bin/release"

  configuration "Profile"
    targetdir "bin/profile"
    buildoptions "-pg"
    linkoptions "-pg"

  configuration "Release or Profile"
    defines "NDEBUG"
    if os.is("windows") then
      optimize "Speed"
    else
      flags "OptimizeSpeed"
      buildoptions {
        "-flto",
      }
      linkoptions {
        "-flto",
        "-fuse-ld=gold",
      }
    end
 
  project "libponyc"
    targetname "ponyc"
    kind "StaticLib"
    language "C"
    includedirs {
      llvm_config("--includedir")
    }
    if not os.is("windows") then
      buildoptions {
        "-std=gnu11"
      }   
    end
    defines {
      "_DEBUG",
      "_GNU_SOURCE",
      "__STDC_CONSTANT_MACROS",
      "__STDC_FORMAT_MACROS",
      "__STDC_LIMIT_MACROS",
    }
    files { "src/libponyc/**.c", "src/libponyc/**.h", "src/libponyc/**.hpp" }
    cppforce { "src/libponyc/**.c" }

  project "ponyc"
    kind "ConsoleApp"
    language "C"
    if not os.is("windows") then
      buildoptions "-std=gnu11"
    end
    files { "src/ponyc/**.c", "src/ponyc/**.h" }
    cppforce { "src/ponyc/**.c" }
    link_libponyc()

  include "utils/"
  include "test/"
