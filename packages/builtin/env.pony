class Env val
  let stdout: Stdout
  let args: Array[String]

  new _create(argc: U64, argv: Pointer[Pointer[U8] val] val) =>
    stdout = Stdout
    args = Array[String]

    for i in Range[U64](0, argc) do
      args.append(
        recover
          String.from_cstring(argv._apply(i))
        end
        )
    end

actor Stdout
  be print(a: String) => @printf[I32]("%s\n".cstring(), a.cstring())
