class Env val
  """
  An environment holds the command line and other values injected into the
  program by default by the runtime.
  """
  let input: Stdin
  let out: StdStream
  let err: StdStream
  let args: Array[String] val
  let vars: Array[String] val

  new _create(argc: U64, argv: Pointer[Pointer[U8] iso] iso,
    envp: Pointer[Pointer[U8] iso] iso)
  =>
    """
    Builds an environment from the command line. This is done before the Main
    actor is created.
    """
    @os_stdout_setup[None]()

    input = Stdin._create(@os_stdin_setup[Bool]())
    out = StdStream._out()
    err = StdStream._err()

    args = _strings_from_pointers(consume argv, argc)
    vars = _strings_from_pointers(consume envp, 0)

  new create(input': Stdin, out': StdStream, err': StdStream,
    args': Array[String] val, vars': Array[String] val)
  =>
    """
    Build an artificial environment.
    """
    input = input'
    out = out'
    err = err'
    args = args'
    vars = vars'

  fun tag exitcode(code: I32) =>
    """
    Sets the application exit code. If this is called more than once, the last
    value set will be the exit code. The exit code defaults to 0.
    """
    @pony_exitcode[None](code)

  fun tag _strings_from_pointers(data: Pointer[Pointer[U8] iso] iso, len: U64):
    Array[String] iso^
  =>
    let array = recover Array[String](len) end
    var i: U64 = 0

    while true do
      let entry = data._update(i, recover Pointer[U8] end)

      if entry.is_null() then
        break
      end

      array.push(recover String.from_cstring(consume entry, 0, false) end)
      i = i + 1
    end

    array
