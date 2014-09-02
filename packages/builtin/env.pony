class Env val
  let args: Array[String]

  new _create(argc: U64, argv: _Pointer[_Pointer[U8] val] val) =>
    args = Array[String]

    for i in Range[U64](0, argc) do
      args.append(recover String._cstring(argv(i)))
    end
