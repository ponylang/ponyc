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

  links "libponyc"

  local output = llvm_config("--libs")

  for lib in string.gmatch(output, "-l(%S+)") do
    links { lib }
  end
end
