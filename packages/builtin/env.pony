class Env val
  """
  An environment holds the command line and other values injected into the
  program by default by the runtime.
  """
  let root: (AmbientAuth | None)
  let input: Stdin
  let out: StdStream
  let err: StdStream
  let args: Array[String] val
  let _envp: Pointer[Pointer[U8]] val
  let _vars: (Array[String] val | None)

  new _create(argc: U64, argv: Pointer[Pointer[U8]] val,
    envp: Pointer[Pointer[U8]] val)
  =>
    """
    Builds an environment from the command line. This is done before the Main
    actor is created.
    """
    root = AmbientAuth._create()
    @os_stdout_setup[None]()

    input = Stdin._create(@os_stdin_setup[Bool]())
    out = StdStream._out()
    err = StdStream._err()

    args = _strings_from_pointers(argv, argc)
    _envp = envp
    _vars = None

  new create(root': (AmbientAuth | None), input': Stdin, out': StdStream,
    err': StdStream, args': Array[String] val,
    vars': (Array[String] val | None))
  =>
    """
    Build an artificial environment. A root capability may be supplied.
    """
    root = root'
    input = input'
    out = out'
    err = err'
    args = args'
    _envp = recover val Pointer[Pointer[U8]]._alloc(0) end
    _vars = vars'

  fun vars(): Array[String] val =>
    """
    Return the environment variables as an array of strings of the form
    "key=value".
    """
    match _vars
      | let v: Array[String] val => v
    else
      if not _envp.is_null() then
        _strings_from_pointers(_envp, _count_strings(_envp))
      else
        recover Array[String] end
      end
    end

  fun tag exitcode(code: I32) =>
    """
    Sets the application exit code. If this is called more than once, the last
    value set will be the exit code. The exit code defaults to 0.
    """
    @pony_exitcode[None](code)

  fun tag _count_strings(data: Pointer[Pointer[U8]] val): U64 =>
    var i: U64 = 0

    while
      let entry = data._apply(i)
      not entry.is_null()
    do
      i = i + 1
    end
    i

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
