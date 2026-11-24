class \nodoc\ _Arrays
  embed empty: Array[U32] = []
  embed frozen: Array[U32] val = recover val [as U32: 7; 11] end
  embed grouped: Array[U32]
  embed nested: Array[Array[U32]]
  let left: U32
  let right: U32

  new create(seed: U32) =>
    ((grouped)) = Array[U32].init(seed, 1)
    ((left, (right, (nested)))) =
      ((U32(43), (U32(47), [[as U32: seed; seed + 1]; [as U32: seed + 2]])))

primitive \nodoc\ _First
primitive \nodoc\ _Second

class \nodoc\ _Conditional[T: Any #read]
  embed items: Array[U32]
  embed matched: Array[U32]
  let marker: U32

  new create(seed: U32, fail: Bool) ? =>
    ((items, marker)) =
      if fail then
        error
      else
        iftype T <: U32 then
          ([as U32: seed], U32(17))
        else
          (Array[U32].init(11, 1), U32(19))
        end
      end

    matched = match \exhaustive\ (if seed == 0 then _First else _Second end)
    | _First => [as U32: 23]
    | _Second => Array[U32].init(29, 1)
    end

actor \nodoc\ Main
  new create(env: Env) =>
    let seed = env.args.size().u32()
    let arrays = _Arrays(seed)

    if arrays.empty.size() != 0 then env.exitcode(1); return end
    if arrays.frozen.size() != 2 then env.exitcode(2); return end
    if arrays.nested.size() != 2 then env.exitcode(3); return end
    if (arrays.left != 43) or (arrays.right != 47) then
      env.exitcode(4)
      return
    end

    try
      if (arrays.frozen(0)? != 7) or (arrays.frozen(1)? != 11) then
        env.exitcode(5)
        return
      end

      let first = arrays.nested(0)?
      let second = arrays.nested(1)?
      if (first.size() != 2) or (second.size() != 1) then
        env.exitcode(6)
        return
      end

      if (first(0)? != seed) or (first(1)? != (seed + 1)) or
        (second(0)? != (seed + 2))
      then
        env.exitcode(7)
        return
      end

      if arrays.grouped(0)? != seed then env.exitcode(9); return end

      let literal = _Conditional[U32](seed, false)?
      let constructor = _Conditional[U64](0, false)?
      if (literal.items(0)? != seed) or (literal.marker != 17) or
        (constructor.items(0)? != 11) or (constructor.marker != 19)
      then
        env.exitcode(10)
        return
      end

      try
        _Conditional[U32](seed, true)?
        env.exitcode(11)
        return
      end

      if (literal.matched(0)? != 29) or (constructor.matched(0)? != 23) then
        env.exitcode(12)
      end
    else
      env.exitcode(8)
    end
