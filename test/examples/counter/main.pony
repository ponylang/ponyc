actor Counter
  var _count: U32

  new create() =>
    _count = 0

  be increment() =>
    _count = _count + 1

  be get_and_reset(main: Main) =>
    main.display(_count)
    _count = 0

actor Main
  new create(env: Env) =>
    var count: U32 = try @atoi[U32](env.args(1).cstring()) else 10 end
    var counter = Counter

    for i in Range[U32](0, count) do
      counter.increment()
    end

    counter.get_and_reset(this)

  be display(result: U32) =>
    @printf[I32]("%d\n".cstring(), result)
