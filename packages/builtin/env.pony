class Env val
  let stdout: Stdout
  let args: Array[String] val

  new _create(argc: U64, argv: Pointer[Pointer[U8] iso] iso) =>
    stdout = Stdout

    args = recover
      var array = Array[String]
      var i: U64 = 0

      while i < argc do
        array.append(
          recover
            String.from_cstring(
              argv._update(i, recover Pointer[U8]._null() end))
          end
          )
        i = i + 1
      end

      consume array
    end

  new create(stdout': Stdout, args': Array[String] val) =>
    stdout = stdout'
    args = args'

actor Stdout
  be print(a: String) => @printf[I32]("%s\n".cstring(), a.cstring())
