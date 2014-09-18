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
