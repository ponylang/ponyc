class \nodoc\ _Arrays
  embed from_if: Array[U32]
  embed from_while: Array[U32]
  embed from_repeat: Array[U32]
  embed from_bare_break: Array[U32] =
    repeat break until true else [as U32: 7] end

  new create(seed: U32, run_while: Bool, use_break: Bool) =>
    from_if = if use_break then [as U32: seed] else [as U32: seed + 1] end

    var remaining: U32 = if run_while then 2 else 0 end
    from_while = while remaining > 0 do
      remaining = remaining - 1
      let value =
        if use_break then
          break [as U32: seed]
        else
          seed + remaining + 1
        end
      [as U32: value]
    else
      Array[U32].init(seed + 2, 1)
    end

    from_repeat = repeat
      if use_break then break [as U32: seed + 3] end
      if run_while then continue end
      [as U32: seed + 5]
    until true else
      [as U32: seed + 4]
    end

class \nodoc\ _NestedLoops
  embed outer: Array[U32]
  let inner: U32

  new create(seed: U32) =>
    var saved: Array[U32] = []
    var pending = true
    outer = while pending do
      pending = false
      saved = while true do break [as U32: seed + 1] else [] end
      while pending do
        None
      else
        break (if seed == 0 then break [as U32: 1] else [as U32: seed] end)
      end
      [as U32: seed + 2]
    else
      []
    end
    inner = try saved(0)? else 0 end

actor \nodoc\ Main
  new create(env: Env) =>
    let seed = env.args.size().u32()
    let from_break = _Arrays(seed, true, true)
    let from_body = _Arrays(seed, true, false)
    let from_else = _Arrays(seed, false, false)
    let nested = _NestedLoops(seed)
    let nested_operand = _NestedLoops(0)

    try
      if from_break.from_while(0)? != seed then env.exitcode(1); return end
      if from_body.from_while(0)? != (seed + 1) then env.exitcode(2); return end
      if from_else.from_while(0)? != (seed + 2) then env.exitcode(3); return end
      if from_break.from_repeat(0)? != (seed + 3) then
        env.exitcode(4)
        return
      end
      if from_body.from_repeat(0)? != (seed + 4) then
        env.exitcode(5)
        return
      end
      if from_break.from_if(0)? != seed then env.exitcode(6); return end
      if from_body.from_if(0)? != (seed + 1) then env.exitcode(7); return end
      if nested.outer(0)? != seed then env.exitcode(8); return end
      if nested_operand.outer(0)? != 1 then env.exitcode(9); return end
      if nested.inner != (seed + 1) then env.exitcode(10); return end
      if from_else.from_repeat(0)? != (seed + 5) then
        env.exitcode(11)
        return
      end
      if from_break.from_bare_break(0)? != 7 then env.exitcode(12) end
    else
      env.exitcode(13)
    end
