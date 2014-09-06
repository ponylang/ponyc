function link_libponyc()
  linkoptions {
    llvm_config("--ldflags")
  }

  links "libponyc"

  if os.is("windows") then
    links { "Shlwapi.lib" }
  end

  local output = llvm_config("--libs")

  for lib in string.gmatch(output, "-l(%S+)") do
    links { lib }
  end
end
