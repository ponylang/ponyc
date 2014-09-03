function link_libponyc()
  libdirs {
    llvm_config("--libdir")
  }

  if os.is("windows") then
    links { "Shlwapi.lib" }
  end

  links { "libponyc" }
  local output = llvm_config("--libs")
  
  for lib in string.gmatch(output, "-l(%S+)") do
    links { lib }
  end
end
