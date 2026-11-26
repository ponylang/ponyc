actor \nodoc\ _Returner
  new create() => _run()

  be _run() =>
    _build(8)

  fun \noinline\ _build(depth: U64): Array[U64] =>
    if depth == 0 then
      let made = Array[U64].init(7, 64)
      // The collection runs at this level's exit poll, while the array is
      // held only as the value being returned.
      @pony_triggergc(@pony_ctx())
      return made
    end
    let below = _build(depth - 1)
    // If the collection freed the returned array, this allocation could reuse
    // its storage and overwrite the value checked below.
    Array[U64].init(depth + 100, 64)
    try
      if below(63)? != 7 then @pony_exitcode(34) end
    else
      @pony_exitcode(35)
    end
    below
