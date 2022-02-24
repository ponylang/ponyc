use "collections"
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
  let _verbose: Bool
  let _exclude: Set[String] val
  let _out: OutStream
  let _err: OutStream

  new create(verbose: Bool, exclude: Set[String] val, out: OutStream,
    err: OutStream)
  =>
    _verbose = verbose
    _exclude = exclude
    _out = out
    _err = err

  fun _str_pony_extension(): String => ".pony"
  fun _str_expected_exit_code(): String => "expected-exit-code.txt"

  fun find(auth: FileAuth, path': String)
    : (Array[_TestDefinition] val | None)
  =>
    let path = if path'.size() > 0 then path' else "." end

    if _verbose then _out.print("Searching for tests in " + path) end

    let fp = FilePath(auth, path)
    if not fp.exists() then
      _err.print(_Colors.red() + path + ": path does not exist"
        + _Colors.none())
      return
    end

    let fi =
      try
        FileInfo(fp)?
      else
        _err.print(_Colors.red() + path + ": unable to get file info"
          + _Colors.none())
        return
      end

    if not fi.directory then
      _err.print(_Colors.red() + path + ": not a directory" + _Colors.none())
      return
    end

    let dir =
      try
        Directory(fp)?
      else
        _err.print(_Colors.red() + path + ": unable to open directory"
          + _Colors.none())
        return
      end

    let entries =
      try
        dir.entries()?
      else
        _err.print(_Colors.red() + path + ": unable to get entries"
          + _Colors.none())
        return
      end

    recover
      let definitions = Array[_TestDefinition]
      for entry in (consume entries).values() do
        if _exclude.contains(entry) then continue end

        match _get_definition(auth, dir.path.path, entry)
        | let def: _TestDefinition => definitions.push(def)
        end
      end
      if _verbose then
        _out.print("Found " + definitions.size().string()
          + " test definitions.")
      end
      definitions
    end

  fun _get_definition(auth: FileAuth, parent: String,
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
        _err.print(_Colors.red() + child
          + ": unable to get directory entries for" + child + _Colors.none())
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
          _err.print(_Colors.red() + child + "/" + entry
            + ": unable to open file" + _Colors.none())
          return None
        end
      end
    end

    if has_pony_sources then
      _TestDefinition(child, dir.path.path, expected_exit_code)
    end

  fun _get_expected_exit_code(fp: FilePath): I32 ? =>
    match OpenFile(fp)
    | let f: File =>
      for line in FileLines(f) do
        try
          return line.i32()?
        else
          _err.print(_Colors.red() + fp.path
            + ": unable to parse expected error code: '" + line.clone() + "'"
            + _Colors.none())
        end
        break
      end
    end
    error
