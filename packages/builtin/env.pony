class Env val
  """
  An environment holds the command line and other values injected into the
  program by default by the runtime.
  """
  let input: Stdin
  let out: StdStream
  let err: StdStream
  let args: Array[String] val

  new _create(argc: U64, argv: Pointer[Pointer[U8] iso] iso) =>
    """
    Builds an environment from the command line. This is done before the Main
    actor is created.
    """
    @os_stdout_setup[None]()

    input = Stdin._create(@os_stdin_setup[Bool]())
    out = StdStream._out()
    err = StdStream._err()

    args = recover
      let array = Array[String](argc)
      var i: U64 = 0

      while i < argc do
        array.push(
          recover
            String.from_cstring(
              argv._update(i, recover Pointer[U8] end))
          end
          )
        i = i + 1
      end

      consume array
    end

  new create(input': Stdin, out': StdStream, err': StdStream,
    args': Array[String] val) =>
    """
    Build an artificial environment.
    """
    input = input'
    out = out'
    err = err'
    args = args'

  fun tag exitcode(code: I32) =>
    """
    Sets the application exit code. If this is called more than once, the last
    value set will be the exit code. The exit code defaults to 0.
    """
    @pony_exitcode[None](code)
