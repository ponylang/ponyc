use "files"

class val _TestDefinition
  let name: String
  let path: String
  let expected_exit_code: I32

  new val create(name': String, path': String, expected_exit_code': I32) =>
    name = name'
    path = path'
    expected_exit_code = expected_exit_code'

class _TestDefinitions
  let verbose: Bool
  let out: OutStream
  let err: OutStream

  new create(verbose': Bool, out': OutStream, err': OutStream) =>
    verbose = verbose'
    out = out'
    err = err'

  fun _str_pony_extension(): String => ".pony"
  fun _str_expected_exit_code(): String => "expected-exit-code.txt"

  fun find(auth: (AmbientAuth | FileAuth), path': String)
    : (Array[_TestDefinition] val | None)
  =>
    let path = if path'.size() > 0 then path' else "." end

    if verbose then out.print("Searching for tests in " + path) end

    let fp = FilePath(auth, path)
    if not fp.exists() then
      err.print("Path does not exist: " + path)
      return
    end

    let fi =
      try
        FileInfo(fp)?
      else
        err.print("Unable to get info: " + path)
        return
      end

    if not fi.directory then
      err.print("Not a directory: " + path)
      return
    end

    let dir =
      try
        Directory(fp)?
      else
        err.print("Unable to open directory: " + path)
        return
      end

    let entries =
      try
        dir.entries()?
      else
        err.print("Unable to get entries: " + path)
        return
      end

    recover
      let definitions = Array[_TestDefinition]
      for entry in (consume entries).values() do
        match _get_definition(auth, dir.path.path, entry)
        | let def: _TestDefinition => definitions.push(def)
        end
      end
      if verbose then
        out.print("Found " + definitions.size().string() + " test definitions.")
      end
      definitions
    end

  fun _get_definition(auth: (AmbientAuth | FileAuth), parent: String,
    child: String): (_TestDefinition | None)
  =>
    let dir =
      try
        Directory(FilePath(auth, Path.join(parent, child)))?
      else
        return None
      end

    let entries =
      try
        dir.entries()?
      else
        err.print("Unable to get directory entries for: " + child)
        return None
      end

    var has_pony_sources = false
    var expected_exit_code = I32(0)
    let ext_size = ISize.from[USize](_str_pony_extension().size())
    for entry in (consume entries).values() do
      let entry_lower = entry.lower()
      try
        let ext_pos = entry_lower.rfind(_str_pony_extension())?
        let ent_size = ISize.from[USize](entry_lower.size())
        if (ext_pos >= 0) and (ext_pos == (ent_size - ext_size)) then
          has_pony_sources = true
        end
      end

      if entry_lower == _str_expected_exit_code() then
        try
          expected_exit_code =
            _get_expected_exit_code(FilePath.from(dir.path, entry)?)?
        else
          err.print("Unable to open file: " + child + "/" + entry)
          return None
        end
      end
    end

    if has_pony_sources then
      _TestDefinition(child, dir.path.path, expected_exit_code)
    else
      None
    end

  fun _get_expected_exit_code(fp: FilePath): I32 ? =>
    match OpenFile(fp)
    | let f: File =>
      for line in FileLines(f) do
        try
          return line.i32()?
        else
          err.print("Unable to parse expected error code: '" + line.clone()
            + "'")
        end
        break
      end
    end
    error
