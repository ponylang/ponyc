actor \nodoc\ _Unwinder
  new create() => _run()

  be _run() =>
    _climb(8)

  fun \noinline\ _climb(depth: U64): U64 =>
    let held = Array[U64].init(depth, 64)
    if depth == 0 then
      Array[U64].init(100, 64)
      return 0
    end
    let below = _climb(depth - 1)
    // Ask for a collection only after a deeper level has returned, so the
    // first poll that can collect runs while the recursion unwinds.
    if depth == 1 then @pony_triggergc(@pony_ctx()) end
    // If that collection freed a level's held array, this allocation could
    // reuse its storage and overwrite the value checked below.
    Array[U64].init(depth + 100, 64)
    try
      if held(63)? != depth then @pony_exitcode(32) end
    else
      @pony_exitcode(33)
    end
    below + 1
