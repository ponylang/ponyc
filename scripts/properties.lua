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