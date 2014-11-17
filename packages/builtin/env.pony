class Env val
  """
  An environment holds the command line and other values injected into the
  program by default by the runtime.
  """
  let stdout: Stdout
  let args: Array[String] val

  new _create(argc: U64, argv: Pointer[Pointer[U8] iso] iso) =>
    """
    Builds an environment from the command line. This is done before the Main
    actor is created.
    """
    stdout = Stdout._create()

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

  new create(stdout': Stdout, args': Array[String] val) =>
    """
    Build an artificial environment.
    """
    stdout = stdout'
    args = args'

actor Stdout
  new _create() => None

  be print(a: String) => @printf[I32]("%s\n".cstring(), a.cstring())
  be write(a: String) => @printf[I32]("%s".cstring(), a.cstring())
