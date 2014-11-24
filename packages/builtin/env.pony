class Env val
  """
  An environment holds the command line and other values injected into the
  program by default by the runtime.
  """
  let out: StdStream
  let err: StdStream
  let args: Array[String] val

  new _create(argc: U64, argv: Pointer[Pointer[U8] iso] iso) =>
    """
    Builds an environment from the command line. This is done before the Main
    actor is created.
    """
    out = StdStream._out()
    err = StdStream._err()

    args = recover
      let array = Array[String]
      var i: U64 = 0

      while i < argc do
        array.append(
          recover
            String.from_cstring(
              argv._update(i, recover Pointer[U8].null() end))
          end
          )
        i = i + 1
      end

      consume array
    end

  new create(out': StdStream, err': StdStream, args': Array[String] val) =>
    """
    Build an artificial environment.
    """
    out = out'
    err = err'
    args = args'
