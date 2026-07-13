use "collections"
use "files"

primitive _StdinClose
  """
  Close the program's stdin as soon as it starts. What a test gets when it has
  neither a stdin.txt nor a stdin-delay-seconds.txt.
  """

class val _StdinWrite
  """
  Write the contents of the test's stdin.txt to the program, then close its
  stdin. The contents can be empty: what a test with an empty stdin.txt asks for
  is a stdin that is closed with nothing in it.
  """
  let data: String val

  new val create(data': String val) =>
    data = data'

class val _StdinDelay
  """
  Hold the program's stdin open, writing nothing to it, for the number of
  seconds in the test's stdin-delay-seconds.txt; then write the contents of its
  stdin.txt, which are empty when it has none, and close.

  This is how a test is given a stdin that is open with no input on it, which is
  what a program blocked on a read of stdin is waiting on.

  A program cannot be given a stdin that stays open for its whole life: a
  ProcessMonitor does not reap a child until it has closed the child's stdin, so
  a test whose stdin never closed would never report an exit code.
  """
  let seconds: U64
  let data: String val

  new val create(seconds': U64, data': String val) =>
    seconds = seconds'
    data = data'

type _Stdin is (_StdinClose | _StdinWrite | _StdinDelay)

class val _TestDefinition
  let name: String
  let path: String
  let expected_exit_code: I32

  let stdin: _Stdin

  // Arguments passed to the program, from the test's program-args.txt.
  let program_args: Array[String] val

  new val create(name': String, path': String, expected_exit_code': I32,
    stdin': _Stdin, program_args': Array[String] val)
  =>
    name = name'
    path = path'
    expected_exit_code = expected_exit_code'
    stdin = stdin'
    program_args = program_args'

class val _BrokenTest
  """
  A directory the runner could not turn into a test: it could not be opened or
  listed, or it has Pony sources but a configuration file could not be read.
  Unlike an entry that is not a test directory (None), the runner fails the run
  on one of these instead of dropping it.
  """
  let name: String
  let reason: String

  new val create(name': String, reason': String) =>
    name = name'
    reason = reason'

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
  fun _str_stdin(): String => "stdin.txt"
  fun _str_stdin_delay_seconds(): String => "stdin-delay-seconds.txt"
  fun _str_program_args(): String => "program-args.txt"

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

    let definitions = recover iso Array[_TestDefinition] end
    let broken = Array[_BrokenTest]
    for entry in (consume entries).values() do
      if _exclude.contains(entry) then continue end

      match _get_definition(auth, dir.path.path, entry)
      | let def: _TestDefinition => definitions.push(def)
      | let bt: _BrokenTest => broken.push(bt)
      end
    end

    if broken.size() > 0 then
      _err.print(_Colors.red() + broken.size().string()
        + " test(s) could not be configured; failing the run:" + _Colors.none())
      for bt in broken.values() do
        _err.print(_Colors.red() + "  " + bt.name + ": " + bt.reason
          + _Colors.none())
      end
      return
    end

    let result: Array[_TestDefinition] val = consume definitions
    if _verbose then
      _out.print("Found " + result.size().string() + " test definitions.")
    end
    result

  fun _get_definition(auth: FileAuth, parent: String,
    child: String): (_TestDefinition | _BrokenTest | None)
  =>
    let fp = FilePath(auth, Path.join(parent, child))

    // An entry that is not a directory is not a test; skip it. A directory the
    // runner cannot open or list is a broken test, not a skip.
    if not (try FileInfo(fp)?.directory else false end) then
      return None
    end

    let dir =
      try
        Directory(fp)?
      else
        return _BrokenTest(child, "unable to open directory")
      end

    let entries =
      try
        dir.entries()?
      else
        return _BrokenTest(child, "unable to read directory entries")
      end

    var has_pony_sources = false
    var expected_exit_code = I32(0)
    var stdin_data: (String val | None) = None
    var delay_seconds: (U64 | None) = None
    var program_args = recover val Array[String] end
    var config_error: (String | None) = None
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
          let exit_code_fp = FilePath.from(dir.path, entry)?
          match \exhaustive\ _get_expected_exit_code(exit_code_fp)
          | let code: I32 => expected_exit_code = code
          | let reason: String => config_error = reason
          end
        else
          config_error = "unable to read " + _str_expected_exit_code()
        end
      end

      if entry_lower == _str_stdin() then
        try
          stdin_data = _get_stdin(FilePath.from(dir.path, entry)?)?
        else
          config_error = "unable to read " + _str_stdin()
        end
      end

      if entry_lower == _str_stdin_delay_seconds() then
        try
          delay_seconds = _get_delay_seconds(FilePath.from(dir.path, entry)?)?
        else
          config_error = "unable to read " + _str_stdin_delay_seconds()
        end
      end

      if entry_lower == _str_program_args() then
        try
          program_args = _get_program_args(FilePath.from(dir.path, entry)?)?
        else
          config_error = "unable to read " + _str_program_args()
        end
      end
    end

    let stdin: _Stdin =
      match (delay_seconds, stdin_data)
      | (let s: U64, let d: String val) => _StdinDelay(s, d)
      | (let s: U64, None) => _StdinDelay(s, "")
      | (None, let d: String val) => _StdinWrite(d)
      else
        _StdinClose
      end

    if not has_pony_sources then
      None
    else
      match \exhaustive\ config_error
      | let reason: String => _BrokenTest(child, reason)
      | None =>
        _TestDefinition(child, dir.path.path, expected_exit_code, stdin,
          program_args)
      end
    end

  fun _get_stdin(fp: FilePath): String val ? =>
    """
    The contents of a test's stdin.txt.
    """
    match OpenFile(fp)
    | let f: File =>
      f.read_string(f.size())
    else
      error
    end

  fun _get_delay_seconds(fp: FilePath): U64 ? =>
    match OpenFile(fp)
    | let f: File =>
      for line in FileLines(f) do
        return line.u64()?
      end
      error
    else
      error
    end

  fun _get_program_args(fp: FilePath): Array[String] val ? =>
    match OpenFile(fp)
    | let f: File =>
      let lines: Array[String] val = f.read_string(f.size()).split_by("\n")
      recover val
        let args = Array[String]
        for line in lines.values() do
          let arg = line.clone() .> strip()
          if arg.size() > 0 then
            args.push(consume arg)
          end
        end
        args
      end
    else
      error
    end

  fun _get_expected_exit_code(fp: FilePath): (I32 | String) =>
    """
    The expected exit code from a test's expected-exit-code.txt, or a reason it
    could not be read. The reason is returned rather than printed so that a
    directory that turns out not to be a test stays quiet.
    """
    match OpenFile(fp)
    | let f: File =>
      for line in FileLines(f) do
        try
          return line.i32()?
        else
          return "unable to parse expected exit code: '" + line.clone() + "'"
        end
      end
      _str_expected_exit_code() + " is empty"
    else
      "unable to read " + _str_expected_exit_code()
    end
