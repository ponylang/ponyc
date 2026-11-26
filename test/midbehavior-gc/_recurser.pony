actor \nodoc\ _Recurser
  new create() => _run()

  be _run() =>
    _descend(8)

  fun \noinline\ _descend(depth: U64) =>
    // Every level holds an array of the same size across the recursive call.
    // If a collection freed one of them, a deeper level could reuse its
    // storage and overwrite the value checked below.
    let held = Array[U64].init(depth, 64)
    if depth == 4 then @pony_triggergc(@pony_ctx()) end
    if depth > 0 then _descend(depth - 1) end
    try
      if held(63)? != depth then @pony_exitcode(30) end
    else
      @pony_exitcode(31)
    end
