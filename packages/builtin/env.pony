class Env val
  """
  An environment holds the command line and other values injected into the
  program by default by the runtime.
  """
  let root: (Root | None)
  let input: Stdin
  let out: StdStream
  let err: StdStream
  let args: Array[String] val
  let vars: Array[String] val
  let cwd: String

  new _create(argc: U64, argv: Pointer[Pointer[U8]] val,
    envp: Pointer[Pointer[U8]] val)
  =>
    """
    Builds an environment from the command line. This is done before the Main
    actor is created.
    """
    root = Root._create()
    @os_stdout_setup[None]()

    input = Stdin._create(@os_stdin_setup[Bool]())
    out = StdStream._out()
    err = StdStream._err()

    args = _strings_from_pointers(argv, argc)
    vars = _strings_from_pointers(envp, 0)
    cwd = recover String.from_cstring(@os_cwd[Pointer[U8]]()) end

  new create(root': (Root | None), input': Stdin, out': StdStream,
    err': StdStream, args': Array[String] val, vars': Array[String] val)
  =>
    """
    Build an artificial environment. A root capability must be supplied. The
    current working directory cannot be changed, as it it not concurrency safe
    to do so.
    """
    root = root'
    input = input'
    out = out'
    err = err'
    args = args'
    vars = vars'
    cwd = recover String.from_cstring(@os_cwd[Pointer[U8]]()) end

  fun tag exitcode(code: I32) =>
    """
    Sets the application exit code. If this is called more than once, the last
    value set will be the exit code. The exit code defaults to 0.
    """
    @pony_exitcode[None](code)

  fun tag _strings_from_pointers(data: Pointer[Pointer[U8]] val, len: U64):
    Array[String] iso^
  =>
    let array = recover Array[String](len) end
    var i: U64 = 0

    while
      let entry = data._apply(i = i + 1)
      not entry.is_null()
    do
      array.push(recover String.copy_cstring(entry) end)
    end

    array
